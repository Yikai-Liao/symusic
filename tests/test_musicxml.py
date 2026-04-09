from __future__ import annotations

from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

import pytest
from symusic import KeySignature, Note, Score, Tempo, TextMeta, TimeSignature, Track

REPO_ROOT = Path(__file__).resolve().parents[1]
MX_RESOURCES = REPO_ROOT / "3rdparty" / "mx" / "Resources"
MX_CORPUS_CASES = sorted(
    path.relative_to(MX_RESOURCES).as_posix()
    for path in MX_RESOURCES.rglob("*")
    if path.suffix in {".xml", ".musicxml"}
)

MX_EXPECTED_IMPORT_FAILURES = {
    "generalxml/fake.xml": r"score-timewise or score-partwise",
    "lysuite/ly41g_PartNoId.xml": r"'id' is a required attribute but was not found",
    "lysuite/ly41h_TooManyParts.xml": r"MusicXML part reconciliation failed",
    "musuite/testPartsSpecialCases.xml": r"MusicXML part reconciliation failed",
}

MX_LOSSY_ROUNDTRIP_CASES = {
    # Lyrics attached to tied continuations or sustained-note interiors do not have a stable
    # note-onset anchor in the flattened Score model, so v1 preserves note segmentation and drops
    # those lyrics on export.
    "foundsuite/An Chloe.xml": "lyric-only loss on non-onset continuation",
    "foundsuite/Schubert_der_Mueller.xml": "lyric-only loss on tied continuation",
    "lysuite/ly33i_Ties_NotEnded.xml": "lyric-only loss on non-onset continuation",
    # These samples still expose unresolved measure-layout canonicalization gaps.
    "foundsuite/PezR44Sco.xml": "measure-layout canonicalization shifts note starts",
    "mjbsuite/krz_v40.xml": "dense conflicting time-signature stream remains lossy",
    "recsuite/Binchois.xml": "leading implicit measure export still shifts note starts",
}

MX_IMPORTABLE_CASES = [
    relative_path
    for relative_path in MX_CORPUS_CASES
    if relative_path not in MX_EXPECTED_IMPORT_FAILURES
]
MX_STRICT_ROUNDTRIP_CASES = [
    relative_path
    for relative_path in MX_IMPORTABLE_CASES
    if relative_path not in MX_LOSSY_ROUNDTRIP_CASES
]


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


def sample_path(relative_path: str) -> Path:
    return MX_RESOURCES / relative_path


def musicxml_contract_snapshot(score: Score) -> tuple:
    return (
        score.ticks_per_quarter,
        sorted((event.time, round(event.tempo, 9)) for event in score.tempos),
        sorted((event.time, event.numerator, event.denominator) for event in score.time_signatures),
        sorted((event.time, event.key, event.tonality) for event in score.key_signatures),
        [
            (
                track.name or f"Track {index + 1}",
                track.program,
                track.is_drum,
                sorted((note.start, note.duration, note.pitch, note.velocity) for note in track.notes),
                sorted((lyric.time, lyric.text) for lyric in track.lyrics),
            )
            for index, track in enumerate(score.tracks)
        ],
    )


def test_musicxml_mx_corpus_expectation_sets_are_consistent() -> None:
    corpus = set(MX_CORPUS_CASES)

    assert set(MX_EXPECTED_IMPORT_FAILURES).issubset(corpus)
    assert set(MX_LOSSY_ROUNDTRIP_CASES).issubset(corpus)
    assert set(MX_EXPECTED_IMPORT_FAILURES).isdisjoint(MX_LOSSY_ROUNDTRIP_CASES)


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


@pytest.mark.parametrize("relative_path", MX_CORPUS_CASES)
def test_musicxml_import_covers_full_mx_corpus(relative_path: str) -> None:
    path = sample_path(relative_path)

    if relative_path in MX_EXPECTED_IMPORT_FAILURES:
        with pytest.raises(RuntimeError, match=MX_EXPECTED_IMPORT_FAILURES[relative_path]):
            Score(path)
        return

    Score(path)


@pytest.mark.parametrize("relative_path", MX_IMPORTABLE_CASES)
def test_musicxml_export_and_reimport_covers_full_mx_corpus(relative_path: str) -> None:
    score = Score(sample_path(relative_path))
    Score.from_musicxml(score.dumps_musicxml())


@pytest.mark.parametrize("relative_path", MX_STRICT_ROUNDTRIP_CASES)
def test_musicxml_roundtrip_preserves_supported_contract_for_full_mx_corpus(
    relative_path: str,
) -> None:
    score = Score(sample_path(relative_path))
    reloaded = Score.from_musicxml(score.dumps_musicxml())

    assert musicxml_contract_snapshot(reloaded) == musicxml_contract_snapshot(score)


def test_musicxml_imports_timewise_documents() -> None:
    score = Score(sample_path("mjbsuite/hello_timewise.xml"))

    assert score.ticks_per_quarter == 1
    assert [(event.time, event.numerator, event.denominator) for event in score.time_signatures] == [
        (0, 4, 4)
    ]
    assert len(score.tracks) == 1
    assert score.tracks[0].name == "Music"
    assert [(note.start, note.duration, note.pitch, note.velocity) for note in score.tracks[0].notes] == [
        (0, 4, 60, 64)
    ]


@pytest.mark.parametrize(
    ("relative_path", "expected_tempos"),
    [
        ("ksuite/k012a_Tempo_Markings.xml", [(0, 120.0), (8, 40.0)]),
        ("musuite/testTempo1.xml", [(0, 60.0), (16, 126.000126000126)]),
    ],
)
def test_musicxml_imports_tempos_from_external_samples(
    relative_path: str, expected_tempos: list[tuple[int, float]]
) -> None:
    score = Score(sample_path(relative_path))

    assert len(score.tempos) == len(expected_tempos)
    for tempo, (expected_time, expected_value) in zip(score.tempos, expected_tempos, strict=True):
        assert tempo.time == expected_time
        assert tempo.tempo == pytest.approx(expected_value)


def test_musicxml_roundtrip_preserves_decimal_tempo_precision() -> None:
    score = Score(sample_path("lysuite/ly31c_MetronomeMarks.xml"))
    reloaded = Score.from_musicxml(score.dumps_musicxml())

    assert [event.time for event in reloaded.tempos] == [event.time for event in score.tempos]
    assert [event.tempo for event in reloaded.tempos] == pytest.approx(
        [event.tempo for event in score.tempos]
    )


def test_musicxml_roundtrip_preserves_canonicalized_global_events_for_freezing_sample() -> None:
    score = Score(sample_path("mjbsuite/freezing.old.xml"))
    reloaded = Score.from_musicxml(score.dumps_musicxml())

    assert musicxml_contract_snapshot(reloaded) == musicxml_contract_snapshot(score)


def test_musicxml_imports_lyrics_from_external_sample() -> None:
    score = Score(sample_path("musuite/testLyricsVoice2a.xml"))

    assert len(score.tracks) == 2
    assert score.tracks[0].name == "Alto"
    assert score.tracks[1].name == "Tenor"
    assert [(lyric.time, lyric.text) for lyric in score.tracks[0].lyrics] == [
        (0, "Lyrics"),
        (1, "in"),
        (2, "voice"),
        (3, "1."),
    ]
    assert [(lyric.time, lyric.text) for lyric in score.tracks[1].lyrics] == [
        (0, "Here"),
        (1, "in"),
        (2, "voice"),
        (3, "2 !"),
    ]


def test_musicxml_roundtrip_preserves_notes_for_tie_attached_lyrics() -> None:
    score = Score(sample_path("lysuite/ly33i_Ties_NotEnded.xml"))
    reloaded = Score.from_musicxml(score.dumps_musicxml())

    assert [
        (note.start, note.duration, note.pitch, note.velocity) for note in reloaded.tracks[0].notes
    ] == [
        (note.start, note.duration, note.pitch, note.velocity) for note in score.tracks[0].notes
    ]
    assert len(reloaded.tracks[0].lyrics) < len(score.tracks[0].lyrics)


def test_musicxml_imports_drum_and_program_metadata() -> None:
    drum_score = Score(sample_path("musuite/testDrumset1.xml"))
    transposition_score = Score(sample_path("custom/transposition.musicxml"))

    assert len(drum_score.tracks) == 1
    assert drum_score.tracks[0].name == "Drumset"
    assert drum_score.tracks[0].is_drum is True
    assert drum_score.tracks[0].program == 0

    assert len(transposition_score.tracks) == 31
    assert transposition_score.tracks[0].name == "Piccolo"
    assert transposition_score.tracks[0].program == 72
    assert transposition_score.tracks[1].name == "Flute 1"
    assert transposition_score.tracks[1].program == 73
    assert [track.name for track in transposition_score.tracks if track.is_drum] == [
        "Percussion 1",
        "Percussion 2",
    ]
    assert [track.program for track in transposition_score.tracks if track.is_drum] == [48, 48]
