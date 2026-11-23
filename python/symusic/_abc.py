"""Convert ABC and MIDI with the executables bundled alongside the package."""

from __future__ import annotations

import os
import re
import shutil
import subprocess
import tempfile
import warnings
from pathlib import Path
from typing import TYPE_CHECKING, TypeVar

if TYPE_CHECKING:
    from collections.abc import Callable

_Score = TypeVar("_Score")


class ABCConversionWarning(UserWarning):
    """Diagnostics from an ABC converter that produced a usable result."""


_BIN = Path(__file__).resolve().parent / "bin"
_EXE_SUFFIX = ".exe" if os.name == "nt" else ""
_ABC2MIDI = _BIN / f"abc2midi{_EXE_SUFFIX}"
_MIDI2ABC = _BIN / f"midi2abc{_EXE_SUFFIX}"
_TEMP_PREFIX = "symusic-abc-"
# readnumf in parseabc.c stops consuming digits once it reaches this value.
_REFERENCE_TRUNCATION_THRESHOLD = 214_748_363
_MAX_REFERENCE = (_REFERENCE_TRUNCATION_THRESHOLD - 1) * 10 + 9
# parseabc.c reads a single whitespace-delimited path of up to 79 bytes.
_INCLUDE = re.compile(rb"(?:^|(?<=[\r\n]))%%abc-include[ \t\v\f]*(\S+)")
_STRESS = re.compile(
    rb"(?P<prefix>"
    rb"(?:(?:^|(?<=[\r\n]))[ \t]*(?:%%MIDI|(?P<instruction>I[ \t]*:[ \t]*MIDI))"
    rb"|(?P<inline>\[I:)[ \t]*MIDI)"
    rb"[ \t]+ptstress[ \t]+)"
    # parsemusic cuts the line at the first percent sign before parsing inline fields.
    rb"(?P<argument>(?(inline)[^\]%\r\n]+(?=\])|[^\r\n]+))"
    # Skip other fields, music annotations, and comments that can contain literal [I: text.
    rb"|(?:^|(?<=[\r\n]))[ \t]*[A-Za-z+][ \t]*:[^\r\n]*"
    rb'|"[^"%\r\n]*"?'
    rb"|\[[A-Za-z]:[^\]%\r\n]*"
    rb"|%[^\r\n]*"
)

# parseabc.c accepts spaces and tabs before and around the field delimiter.
_TUNE_LINE = re.compile(
    rb"(?:^|(?<=[\r\n]))(?:"
    rb"[ \t]*X[ \t]*:(?![:|])[ \t]*(?P<reference>[0-9]*)"
    rb"|(?i:%%MidiOff|%%MidiOn)|%%(?:begin|end)ps(?=[\r\n]|$)"
    rb"|[^\r\n]*?%%(?:begin|end)text)[^\r\n]*"
)
_INCLUDE_LINE = re.compile(_INCLUDE.pattern + rb"[^\r\n]*(?:\r\n|\n\r|\r|\n)")


def _diagnostics(stdout: bytes, stderr: bytes) -> str:
    return b"\n".join(part.strip() for part in (stdout, stderr) if part.strip()).decode(
        "utf-8",
        errors="replace",
    )


def _conversion_error(
    result: subprocess.CompletedProcess[bytes],
    reason: str,
) -> RuntimeError:
    tool = Path(result.args[0]).stem
    msg = f"{tool} {reason} (exit status {result.returncode})"
    diagnostics = _diagnostics(result.stdout, result.stderr)
    if diagnostics:
        msg += f"\n{diagnostics}"
    return RuntimeError(msg)


def _complete_midi(data: bytes) -> bool:
    # A fatal exit during track generation can leave an unpatched chunk length.
    # Check the full output envelope before passing events to the MIDI reader.
    if len(data) < 14 or not data.startswith(b"MThd\x00\x00\x00\x06"):
        return False
    track_count = int.from_bytes(data[10:12], "big")
    offset = 14
    for _ in range(track_count):
        if data[offset : offset + 4] != b"MTrk":
            return False
        size = int.from_bytes(data[offset + 4 : offset + 8], "big")
        offset += 8 + size
        if (
            size < 4
            or offset > len(data)
            or data[offset - 3 : offset] != b"\xff\x2f\x00"
        ):
            return False
    return track_count > 0 and offset == len(data)


def _stage_input_files(data: bytes, workdir: Path) -> tuple[bytes, set[bytes]]:
    # Resolve paths in the caller's cwd and use short ASCII names in the converter's cwd.
    includes: dict[bytes, bytes] = {}
    stress_files: dict[bytes, bytes] = {}

    def stage_stress(match: re.Match[bytes]) -> bytes:
        original = match["argument"]
        if original is None:
            return match[0]
        comment = b""
        if match["instruction"]:
            # parsefield in parseabc.c splits I: fields at the first percent sign.
            original, separator, suffix = original.partition(b"%")
            comment = separator + suffix
        if original not in stress_files:
            name = f"stress-{len(stress_files)}.txt"
            try:
                shutil.copyfile(os.fsdecode(original), workdir / name)
            except (OSError, ValueError):
                # This may be an inline stress pattern or a missing file.
                return match[0]
            stress_files[original] = name.encode("ascii")
        return match["prefix"] + stress_files[original] + comment

    def stage_include(match: re.Match[bytes]) -> bytes:
        original = match[1][:79]
        if original not in includes:
            try:
                content = Path(os.fsdecode(original)).read_bytes()
            except (OSError, ValueError):
                # Let the converter report its original include diagnostic.
                return match[0]
            name = f"include-{len(includes)}.abc"
            (workdir / name).write_bytes(_STRESS.sub(stage_stress, content))
            includes[original] = name.encode("ascii")
        return b"%%abc-include " + includes[original]

    data = _INCLUDE.sub(stage_include, _STRESS.sub(stage_stress, data))
    return data, set(includes.values())


def _isolate_tune(data: bytes, tune: int, workdir: Path, includes: set[bytes]) -> bytes:
    # Expand staged includes once so a tune can begin or continue within one.
    def expand_include(match: re.Match[bytes]) -> bytes:
        if match[1] not in includes:
            return match[0]
        content = (workdir / match[1].decode("ascii")).read_bytes()
        return content if content.endswith((b"\r", b"\n")) else content + b"\n"

    if includes:
        data = _INCLUDE_LINE.sub(expand_include, data)
    reference = str(tune).encode("ascii")
    ignored = False
    first = selected = None
    for line in _TUNE_LINE.finditer(data):
        content = line[0]
        # parseline applies endtext after begintext, then checks MidiOff and MidiOn.
        if b"%%begintext" in content or content == b"%%beginps":
            ignored = True
        if b"%%endtext" in content or content == b"%%endps":
            ignored = False
        if content[:9].lower() == b"%%midioff":
            ignored = True
        if content[:8].lower() == b"%%midion":
            ignored = False
        if not ignored and line["reference"] is not None:
            if first is None:
                first = line.start()
            if selected is not None:
                return data[:first] + data[selected : line.start()]
            digits = line["reference"]
            if digits and (digits.lstrip(b"0") or b"0") == reference:
                selected = line.start()

    if selected is not None:
        return data[:first] + data[selected:]
    msg = f"ABC tune X:{tune} was not found"
    raise ValueError(msg)


def _read_abc(
    data: bytes, tune: int | None
) -> tuple[bytes, subprocess.CompletedProcess[bytes]]:
    """Return complete MIDI bytes and the diagnostics from converting one tune."""
    if tune is not None:
        if not isinstance(tune, int) or isinstance(tune, bool):
            msg = "tune must be an integer or None"
            raise TypeError(msg)
        if not 0 <= tune <= _MAX_REFERENCE:
            msg = f"ABC tune X:{tune} was not found"
            raise ValueError(msg)

    with tempfile.TemporaryDirectory(prefix=_TEMP_PREFIX) as directory:
        workdir = Path(directory)
        data, includes = _stage_input_files(data, workdir)
        args = [_ABC2MIDI, "stdin"]
        if tune is not None:
            # The converter uses 0 as a sentinel and large references can
            # alias longer numbers. Isolate these by their full decimal value.
            if 0 < tune < _REFERENCE_TRUNCATION_THRESHOLD:
                args.append(str(tune))
            else:
                data = _isolate_tune(data, tune, workdir, includes)
        output = workdir / "output.mid"
        args.extend(("-o", output.name, "-silent"))

        if _INCLUDE.search(data):
            # abc2midi opens this file in text mode and, after an include,
            # seeks back to a position it saved with ftell. The position is
            # exact only with native line endings: on POSIX a CRLF line stops
            # between the two bytes, and the leftover LF reads as a blank line
            # that ends the tune; on Windows ftell counts every LF as CRLF.
            input_path = workdir / "input.abc"
            input_path.write_bytes(
                data.replace(b"\r\n", b"\n").replace(b"\n", os.linesep.encode())
            )
            args[1] = input_path.name
            result = subprocess.run(args, cwd=workdir, capture_output=True)
        else:
            result = subprocess.run(args, input=data, cwd=workdir, capture_output=True)

        if result.returncode not in (0, 1):
            raise _conversion_error(result, "failed")
        if not output.is_file():
            if tune is not None and (
                result.returncode == 0 or b"No tune processed." in result.stdout
            ):
                msg = f"ABC tune X:{tune} was not found"
                diagnostics = _diagnostics(result.stdout, result.stderr)
                if diagnostics:
                    msg += f"\n{diagnostics}"
                raise ValueError(msg)
            raise _conversion_error(result, "did not produce MIDI output")
        midi = output.read_bytes()
        if not _complete_midi(midi):
            raise _conversion_error(result, "produced incomplete MIDI output")

    return midi, result


def parse_abc(
    data: bytes,
    tune: int | None,
    from_midi: Callable[[bytes], _Score],
) -> _Score:
    """Read a tune with the score type's MIDI parser and report converter diagnostics."""
    midi, result = _read_abc(data, tune)
    try:
        score = from_midi(midi)
    except (ValueError, RuntimeError) as exc:
        raise _conversion_error(result, f"produced invalid MIDI: {exc}") from exc

    diagnostics = _diagnostics(result.stdout, result.stderr)
    if diagnostics or result.returncode:
        warnings.warn(
            diagnostics or f"abc2midi exited with status {result.returncode}",
            ABCConversionWarning,
            stacklevel=3,
        )
    return score


def write_abc(midi: bytes, warn: bool) -> str:
    """Transcribe MIDI to ABC text, optionally warning about converter diagnostics."""
    with tempfile.TemporaryDirectory(prefix=_TEMP_PREFIX) as directory:
        input_path = Path(directory) / "input.mid"
        input_path.write_bytes(midi)
        result = subprocess.run(
            [_MIDI2ABC, input_path.name],
            cwd=directory,
            stdin=subprocess.DEVNULL,
            capture_output=True,
        )
    if result.returncode != 0:
        raise _conversion_error(result, "failed")

    # With no -o, midi2abc writes its diagnostics before the ABC header on
    # stdout. Its default second line names our staged input, not the score.
    stdout = result.stdout.replace(b"\r\n", b"\n")
    header = b"X: 1\n"
    prefix, separator, body = stdout.partition(header)
    if not separator:
        raise _conversion_error(result, "did not produce ABC output")
    body = body.removeprefix(b"T: from input.mid\n")
    diagnostics = _diagnostics(
        prefix.removeprefix(b"calling midi2abc\n"),
        result.stderr,
    )
    try:
        abc = (header + body).decode("utf-8")
    except UnicodeDecodeError as exc:
        raise _conversion_error(result, "produced ABC that is not UTF-8") from exc
    if warn and diagnostics:
        warnings.warn(diagnostics, ABCConversionWarning, stacklevel=3)
    return abc
