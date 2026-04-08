from __future__ import annotations

from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

import pytest
from symusic import KeySignature, Note, Score, Tempo, TextMeta, TimeSignature, Track


def make_roundtrip_score() -> Score:
    score = Score(480)
    score.tempos.append(Tempo(0, 120))
    score.time_signatures.append(TimeSignature(0, 4, 4))
    score.time_signatures.append(TimeSignature(1920, 3, 4))
    score.key_signatures.append(KeySignature(0, 2, 0))

    piano = Track("Piano", 0, False)
    piano.notes.append(Note(0, 2400, 60, 91))
    piano.notes.append(Note(0, 480, 64, 88))
    piano.notes.append(Note(480, 480, 67, 87))
    piano.notes.append(Note(1920, 360, 69, 95))
    piano.lyrics.append(TextMeta(0, "la"))

    drums = Track("Drums", 0, True)
    drums.notes.append(Note(1920, 240, 36, 100))
    drums.notes.append(Note(2160, 240, 38, 96))

    score.tracks.append(piano)
    score.tracks.append(drums)
    return score


def normalize_score(score: Score) -> None:
    score.tempos.sort(key=lambda event: (event.time, event.tempo))
    score.time_signatures.sort(
        key=lambda event: (event.time, event.numerator, event.denominator),
    )
    score.key_signatures.sort(key=lambda event: (event.time, event.key, event.tonality))
    for track in score.tracks:
        track.notes.sort(key=lambda note: (note.start, note.pitch, note.end, note.velocity))
        track.lyrics.sort(key=lambda lyric: (lyric.time, lyric.text))


def test_musicxml_roundtrip_in_memory() -> None:
    score = make_roundtrip_score()
    xml = score.dumps_musicxml()

    assert xml.startswith("<?xml")

    reloaded = Score.from_musicxml(xml)

    normalize_score(score)
    normalize_score(reloaded)
    assert reloaded == score


def test_musicxml_file_roundtrip_and_format_detection(tmp_path: Path) -> None:
    score = make_roundtrip_score()

    musicxml_path = tmp_path / "roundtrip.musicxml"
    xml_path = tmp_path / "roundtrip.xml"

    score.dump_musicxml(musicxml_path)
    xml_path.write_text(score.dumps_musicxml(), encoding="utf-8")

    loaded_musicxml = Score(musicxml_path)
    loaded_xml = Score(xml_path)
    loaded_xml_explicit = Score.from_file(xml_path, format="xml")

    normalize_score(score)
    normalize_score(loaded_musicxml)
    normalize_score(loaded_xml)
    normalize_score(loaded_xml_explicit)

    assert loaded_musicxml == score
    assert loaded_xml == score
    assert loaded_xml_explicit == score


def test_musicxml_mxl_rejected(tmp_path: Path) -> None:
    mxl_path = tmp_path / "unsupported.mxl"
    mxl_path.write_bytes(b"placeholder")

    with pytest.raises(ValueError, match=r"Compressed MusicXML \(.mxl\) is not supported yet"):
        Score(mxl_path)

    with pytest.raises(ValueError, match=r"Compressed MusicXML \(.mxl\) is not supported yet"):
        Score.from_file(mxl_path, format="musicxml")


def test_musicxml_conversion_is_safe_under_parallel_calls(tmp_path: Path) -> None:
    score = make_roundtrip_score()
    xml = score.dumps_musicxml()
    expected_notes = Score.from_musicxml(xml).note_num()

    def dump_once(index: int) -> int:
        path = tmp_path / f"parallel-{index}.musicxml"
        score.dump_musicxml(path)
        return Score(path).note_num()

    def load_once(_: int) -> int:
        return Score.from_musicxml(xml).note_num()

    with ThreadPoolExecutor(max_workers=8) as executor:
        dumped = list(executor.map(dump_once, range(4)))
        loaded = list(executor.map(load_once, range(4)))

    assert dumped == [score.note_num()] * 4
    assert loaded == [expected_notes] * 4
