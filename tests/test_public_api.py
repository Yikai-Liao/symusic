import errno
import os
import re
from collections.abc import MutableSequence
from importlib.util import find_spec

import numpy as np
import pytest
import symusic
from symusic import (
    Note,
    NoteList,
    Pedal,
    Quarter,
    Score,
    Second,
    Tempo,
    TextMeta,
    TextMetaList,
    Tick,
    Track,
)


@pytest.mark.parametrize("ttype", [Tick, Quarter, Second])
def test_concepts_reuse_classes(ttype):
    score = Score(ttype=ttype)
    note = Note(0, 2, 60, ttype=ttype)
    score.tracks.append(Track(notes=[note], ttype=ttype))
    assert type(score) is Score
    assert type(score.tracks[0]) is Track
    assert type(score.tracks[0].notes) is NoteList
    assert type(score.tracks[0].notes[0]) is Note
    assert score.ttype is score.tracks[0].notes.ttype is note.ttype is ttype
    match note:
        case Note(time, duration, pitch):
            assert (time, duration, pitch) == (0, 2, 60)
        case _:
            pytest.fail("The Note class pattern did not match")
    assert type(score.to(Second)) is Score
    assert score.to(Second).ttype is Second


@pytest.mark.parametrize("unit", [Quarter, Second])
def test_positional_list_copy_preserves_unit(unit):
    score = Score(ttype=unit)
    score.tracks.append(Track(notes=[Note(0, 1, 60, ttype=unit)], ttype=unit))
    for source in (score.tracks, score.tracks[0].notes):
        copied = type(source)(source)
        assert copied.ttype is unit
        assert copied == source
        copied.clear()
        assert len(source) == 1


def test_list_rejects_mixed_units():
    notes = NoteList([Note(0, 2, 60)])
    second = Note(0, 2, 62, ttype=Second)
    for operation in (
        lambda: notes.append(second),
        lambda: notes.insert(0, second),
        lambda: notes.extend([notes[0], second]),
        lambda: notes.__setitem__(slice(None), [second]),
    ):
        with pytest.raises(ValueError, match="units must match"):
            operation()
    assert notes == [Note(0, 2, 60)]
    score = Score()
    with pytest.raises(ValueError, match="units must match"):
        score.tracks = (Track(ttype=Second),)
    with pytest.raises(ValueError, match="units must match"):
        Track().notes = NoteList(ttype=Second)


def test_removed_forms():
    for name in ("core", "factory", "types"):
        assert find_spec(f"symusic.{name}") is None
    for obj, name in (
        (Score(), "tpq"),
        (Note(0, 1, 60), "start"),
        (Tempo(0, 120), "tempo"),
    ):
        assert not hasattr(obj, name)
        assert not hasattr(obj, "__use_count")
    with pytest.raises(ValueError, match="ttype"):
        Score(ttype="tick")
    with pytest.raises(ValueError, match="ttype"):
        Score().to("second")
    with pytest.raises(TypeError):
        Score(Score())
    with pytest.raises(TypeError, match="ticks_per_quarter"):
        Score(960)
    assert not hasattr(Score, "from_file")


def test_checked_python_inputs(tmp_path):
    with pytest.raises(ValueError, match="ticks_per_quarter"):
        Score(ticks_per_quarter=0)
    with pytest.raises(ValueError, match="time"):
        Note(2**31, 1, 60)
    with pytest.raises(TypeError):
        Note(0.5, 1, 60)
    with pytest.raises(FileNotFoundError):
        Score(tmp_path / "missing.mid")
    with pytest.raises(IsADirectoryError):
        Score(tmp_path, format="midi")


def test_score_checks_format_before_reading(tmp_path):
    path = tmp_path / "missing.unknown"
    with pytest.raises(ValueError, match="format"):
        Score(path)
    with pytest.raises(ValueError, match="format"):
        Score(path, format="unknown")
    with pytest.raises(FileNotFoundError):
        Score(path, format="midi")


@pytest.mark.skipif(
    os.name == "nt", reason="chmod does not revoke read access on Windows"
)
def test_midi_file_preserves_permission_error(tmp_path):
    path = tmp_path / "unreadable.mid"
    path.touch()
    path.chmod(0)
    if os.access(path, os.R_OK):
        pytest.skip("the test process can read files without read permission")
    assert path.is_file()
    with pytest.raises(PermissionError) as error:
        Score(path)
    assert error.value.errno == errno.EACCES
    assert error.value.filename == str(path)


def test_public_callable_signatures():
    for name in symusic.__all__:
        obj = getattr(symusic, name)
        assert callable(obj)
        members = {
            name: obj,
            **{
                attr: getattr(obj, attr)
                for attr in vars(obj)
                if not attr.startswith("_") and callable(getattr(obj, attr))
            },
        }
        for attr, member in members.items():
            doc = member.__doc__ or ""
            assert re.match(rf"{re.escape(attr)}\(", doc), (name, attr, doc)
            for token in ("symusic::", "std::", "shared<"):
                assert token not in doc, (name, attr, token, doc)


def test_unsupported_text_columns():
    assert not hasattr(TextMeta, "from_numpy")
    with pytest.raises(RuntimeError):
        TextMetaList.from_numpy()


def test_pedal_start():
    pedal = Pedal(1, 2)
    assert not hasattr(pedal, "start")
    assert pedal.end == 3


@pytest.mark.parametrize(
    "name", [name for name in symusic.__all__ if name.endswith("List")]
)
def test_public_lists_register_mutable_sequence(name):
    items = getattr(symusic, name)()
    assert isinstance(items, MutableSequence)
    with pytest.raises(TypeError, match="unhashable"):
        hash(items)


@pytest.mark.parametrize(
    "make_item", [lambda i: Note(i, 1, 60), lambda i: Track(str(i))]
)
def test_mutable_sequence_operations(make_item):
    first, second, third = map(make_item, range(3))
    empty = NoteList() if isinstance(first, Note) else symusic.TrackList()
    empty.insert(-100, first)
    empty.insert(100, second)
    items = alias = empty
    items += [first]
    assert items is alias
    assert items.index(first, -2, 100) == 2
    with pytest.raises(ValueError, match="Item is not in list"):
        items.index(first, 1, 2)
    assert items.count(first) == 2
    assert third not in items
    items.remove(first)
    assert items.pop() == first
    assert items == [second]

    items.extend([first, third])
    items.reverse()
    assert list(items) == [third, first, second]
    assert list(reversed(items)) == [second, first, third]
    items.reverse()
    assert items[::-1] == [third, first, second]
    items[3:1] = [second, third]
    assert items == [second, first, third, second, third]
    items[::-2] = [first, second, first]
    assert items == [first, first, second, second, first]
    del items[::-2]
    assert items == [first, second]
    items[:1] = []
    assert items == [second]
    items.clear()
    assert not items


@pytest.mark.parametrize("format_name", ["midi", "abc"])
def test_score_binary_file_and_explicit_format(tmp_path, format_name):
    from io import BytesIO

    score = Score(ticks_per_quarter=480)
    score.tracks.append(Track(notes=[Note(0, 480, 60, 80)]))
    stream = BytesIO()
    with pytest.raises(ValueError, match="format"):
        score.dump(stream)
    assert score.dump(stream, format=format_name) is None
    stream.seek(0)
    with pytest.raises(ValueError, match="format"):
        Score(stream)
    assert Score(stream, format=format_name).note_count() == 1
    path = tmp_path / "score.data"
    score.dump(path, format=format_name)
    assert Score(path, format=format_name).note_count() == 1


def test_score_file_write_errors(tmp_path):
    with pytest.raises(FileNotFoundError):
        Score().dump(tmp_path / "missing" / "out.mid")
    directory = tmp_path / "directory.mid"
    directory.mkdir()
    with pytest.raises(IsADirectoryError):
        Score().dump(directory)
    with pytest.raises(ValueError, match="format"):
        Score().dump(tmp_path / "out.mid", format="unknown")


def test_copy_protocol_and_hashes():
    import copy
    from collections.abc import MutableSequence

    score = Score()
    score.tracks.append(Track(notes=[Note(0, 1, 60)]))
    track = score.tracks[0]
    notes = track.notes
    for item in (score, track, notes, notes[0], score.tracks):
        assert item.__hash__ is None
        assert item.copy() == copy.deepcopy(item)
        with pytest.raises(TypeError):
            item.copy(deep=False)
    assert isinstance(notes, MutableSequence)
    assert isinstance(score.tracks, MutableSequence)
    independent = notes.copy()
    shallow = copy.copy(notes)
    shallow[0].pitch = 61
    assert notes[0].pitch == 61
    assert independent[0].pitch == 60
    with pytest.raises(TypeError):
        notes.__copy__(False)
    assert notes.sort() is None


def test_to_numpy_uses_field_names_and_bool_minor():
    from symusic import KeySignature, KeySignatureList

    signatures = KeySignatureList([KeySignature(0, -3, True)])
    arrays = signatures.to_numpy()
    assert set(arrays) == {"time", "key", "minor"}
    assert arrays["minor"].dtype == np.dtype(bool)
    assert KeySignatureList.from_numpy(**arrays) == signatures
    assert signatures[0].minor is True
    assert not hasattr(signatures, "numpy")


@pytest.mark.parametrize("stride", [2, -2])
def test_key_signature_from_strided_booleans(stride):
    from symusic import KeySignature, KeySignatureList

    signatures = KeySignatureList(
        [
            KeySignature(0, -3, True),
            KeySignature(1, 2, False),
            KeySignature(2, 1, False),
            KeySignature(3, 0, True),
        ]
    )
    arrays = {name: column[::stride] for name, column in signatures.to_numpy().items()}
    assert KeySignatureList.from_numpy(**arrays) == signatures[::stride]


def test_key_signature_from_numpy_checks_integer_minor():
    from symusic import KeySignatureList

    columns = {"time": np.array([0, 1]), "key": np.array([-3, 2])}
    signatures = KeySignatureList.from_numpy(**columns, minor=np.array([1, 0]))
    assert [signature.minor for signature in signatures] == [True, False]
    with pytest.raises(ValueError, match="minor"):
        KeySignatureList.from_numpy(**columns, minor=np.array([1, 2]))


@pytest.mark.parametrize("offset", [8, 2**80])
def test_shift_range_failure_does_not_partially_mutate(offset):
    score = Score()
    score.tracks.extend(
        [
            Track(notes=[Note(0, 1, 60, 60)]),
            Track(notes=[Note(0, 1, 125, 125)]),
        ]
    )
    previous = score.copy()
    for operation in (score.shift_pitch, score.shift_velocity):
        with pytest.raises(ValueError, match=r"pitch|velocity|offset"):
            operation(offset)
        assert score == previous


def test_resample_keyword_boundary():
    with pytest.raises(TypeError):
        Score().resample(ticks_per_quarter=480)
    with pytest.raises(TypeError):
        Score().resample(480, 1)


def test_unknown_pianoroll_mode():
    with pytest.raises(ValueError, match="Unknown"):
        Score().pianoroll(["unknown"])


@pytest.mark.parametrize("value", [-1, 128, 2**80])
def test_program_range_is_checked(value):
    with pytest.raises(ValueError, match="program"):
        Track(program=value)
    track = Track()
    with pytest.raises(ValueError, match="program"):
        track.program = value
    assert track.program == 0


def test_numpy_integer_columns_validate_before_narrowing():
    arrays = {
        "time": np.array([0]),
        "duration": np.array([1]),
        "pitch": np.array([60]),
        "velocity": np.array([80]),
    }
    assert NoteList.from_numpy(**arrays)[0] == Note(0, 1, 60, 80)
    arrays["pitch"][0] = 316
    with pytest.raises(ValueError, match="pitch"):
        NoteList.from_numpy(**arrays)


@pytest.mark.parametrize(
    ("name", "is_drum"),
    [("Piano", False), ("Kit's cymbal", True), ("Both '\" quotes\\\n\t\0中", False)],
)
def test_track_repr_quotes_name(name, is_drum):
    track = Track(name=name, program=3, is_drum=is_drum, ttype=Quarter)
    assert repr(track) == (
        f"Track(ttype='Quarter', program=3, is_drum={is_drum!r}, name={name!r}, "
        "notes=0, lyrics=0)"
    )


def test_score_repr_quotes_time_unit():
    score = Score(ticks_per_quarter=480, ttype=Second)
    assert repr(score) == (
        "Score(ttype='Second', ticks_per_quarter=480, begin=0, end=0, tracks=0, "
        "notes=0, time_sig=0, key_sig=0, markers=0)"
    )
