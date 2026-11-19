"""Locate or install the symusic agent skill bundled with the package.

The skill is the user guide written for coding agents: ``SKILL.md`` and the
pages under ``references/``. From the command line::

    python -m symusic.skill path
    python -m symusic.skill install ~/.claude/skills

``install`` copies the skill to ``DIR/symusic``.
"""

from __future__ import annotations

import argparse
import shutil
import sys
from pathlib import Path

__all__ = ["install", "path"]


def path() -> Path:
    """Return the bundled skill directory, the one that holds ``SKILL.md``."""
    skill = Path(__file__).resolve().parent / "skills" / "symusic"
    if not (skill / "SKILL.md").is_file():
        msg = f"The symusic skill is not installed at {skill}; reinstall symusic"
        raise FileNotFoundError(msg)
    return skill


def install(directory: str | Path, force: bool = False) -> Path:
    """Copy the skill to ``directory/symusic`` and return that path.

    An existing ``directory/symusic`` raises ``FileExistsError`` unless
    ``force`` is true, in which case it is replaced. A target that overlaps
    the bundled skill raises ``ValueError``.
    """
    source = path().resolve()
    target = Path(directory).expanduser() / "symusic"
    # Replacing a symlink removes the link, not the directory it points to.
    resolved_target = target.parent.resolve() / target.name
    if resolved_target.is_relative_to(source) or source.is_relative_to(resolved_target):
        msg = (
            f"{target} overlaps the bundled skill at {source}; choose another directory"
        )
        raise ValueError(msg)
    if target.exists() or target.is_symlink():
        if not force:
            msg = f"{target} exists; pass force=True to replace it"
            raise FileExistsError(msg)
        if target.is_dir() and not target.is_symlink():
            shutil.rmtree(target)
        else:
            target.unlink()
    shutil.copytree(source, target)
    return target


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        prog="python -m symusic.skill",
        description="Locate or install the symusic agent skill.",
    )
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("path", help="print the bundled skill directory")
    install_parser = commands.add_parser(
        "install", help="copy the skill to DIR/symusic"
    )
    install_parser.add_argument(
        "directory", metavar="DIR", help="skills directory of the agent"
    )
    install_parser.add_argument(
        "--force", action="store_true", help="replace an existing copy"
    )
    args = parser.parse_args(argv)
    try:
        if args.command == "path":
            print(path())
        else:
            print(install(args.directory, force=args.force))
    except (FileExistsError, FileNotFoundError, ValueError) as error:
        parser.exit(1, f"error: {error}\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
