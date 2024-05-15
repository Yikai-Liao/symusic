from pathlib import Path

HERE = Path(__file__).parent

MIDI_PATHS = sorted((HERE.parent / "testcases").rglob("*.mid"))
