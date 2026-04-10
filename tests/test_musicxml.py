from __future__ import annotations

from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
from xml.etree import ElementTree as ET

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
}

MX_HEURISTIC_VOICE_EXPORT_ERROR = r"heuristic voice reconstruction"
MX_EXPECTED_EXPORT_FAILURE_REASON = "track contains overlapping note spans that would require heuristic voice reconstruction"
MX_EXPECTED_EXPORT_FAILURES = {
    relative_path: MX_EXPECTED_EXPORT_FAILURE_REASON
    for relative_path in [
        "foundsuite/ARBEAU Belle qui tiens ma vie.xml",
        "foundsuite/An Chloe.xml",
        "foundsuite/Berlioz_Le_Corsaire.xml",
        "foundsuite/Black Note Study Op 10 no 5.xml",
        "foundsuite/Deutscher Tanz D.820.1.xml",
        "foundsuite/Invention 1.xml",
        "foundsuite/Invention 2.xml",
        "foundsuite/Invention 3.xml",
        "foundsuite/Invention_10.xml",
        "foundsuite/Invention_11.xml",
        "foundsuite/Invention_12.xml",
        "foundsuite/Invention_13.xml",
        "foundsuite/Invention_14.xml",
        "foundsuite/Invention_15.xml",
        "foundsuite/Invention_4.xml",
        "foundsuite/Invention_5.xml",
        "foundsuite/Invention_6.xml",
        "foundsuite/Invention_7.xml",
        "foundsuite/Invention_8.xml",
        "foundsuite/Invention_9.xml",
        "foundsuite/Moments Musicaux Op16 No4.xml",
        "foundsuite/O_Holy_Night-Adam-1871.xml",
        "foundsuite/O_Holy_Night.xml",
        "foundsuite/PepAiraSco.xml",
        "foundsuite/Rimsky-Korsakov Op11 No4.xml",
        "foundsuite/RonCLunSco.xml",
        "foundsuite/SCHUBERT An die Sonne.xml",
        "foundsuite/Schubert_der_Mueller.xml",
        "ksuite/Bombe.xml",
        "logicpro/logic01a_homoSapiens.xml",
        "lysuite/ly23f_Tuplets_DurationButNoBracket.xml",
        "lysuite/ly42b_MultiVoice_MidMeasureClefChange.xml",
        "mjbsuite/OctoberXML.xml",
        "mjbsuite/freezing.old.xml",
        "mjbsuite/freezing.xml",
        "mjbsuite/krz_v40.xml",
        "musuite/A_Walk_through_the_Park.xml",
        "musuite/testDurationRoundingError.xml",
        "musuite/testDurationRoundingError_ref.xml",
        "musuite/testDynamics2.xml",
        "musuite/testDynamics3.xml",
        "musuite/testDynamics3_ref.xml",
        "musuite/testGrace2.xml",
        "musuite/testNotesRests1.xml",
        "musuite/testTuplets2.xml",
        "musuite/testTuplets2_ref.xml",
        "musuite/testTuplets3.xml",
        "musuite/testTuplets3_ref.xml",
        "musuite/testVoiceMapper1.xml",
        "musuite/testVoiceMapper1_ref.xml",
        "musuite/testVoiceMapper2.xml",
        "musuite/testVoiceMapper2_ref.xml",
        "musuite/testVoiceMapper3.xml",
        "musuite/testWedge2.xml",
        "musuite/testWedge2_ref.xml",
        "recsuite/ActorPreludeSample.xml",
        "recsuite/BeetAnGeSample.xml",
        "recsuite/Binchois.xml",
        "recsuite/BrahWiMeSample.xml",
        "recsuite/DebuMandSample.xml",
        "recsuite/Dichterliebe01.xml",
        "recsuite/FaurReveSample.xml",
        "recsuite/MahlFaGe4Sample.xml",
        "recsuite/MozaChloSample.xml",
        "recsuite/MozaChloSample2.xml",
        "recsuite/MozaVeilSample.xml",
        "recsuite/MozartPianoSonata.xml",
        "recsuite/SchbAvMaSample.xml",
        "recsuite/Telemann.xml",
    ]
}

MX_IMPORTABLE_CASES = [
    relative_path
    for relative_path in MX_CORPUS_CASES
    if relative_path not in MX_EXPECTED_IMPORT_FAILURES
]
MX_EXPORTABLE_CASES = [
    relative_path
    for relative_path in MX_IMPORTABLE_CASES
    if relative_path not in MX_EXPECTED_EXPORT_FAILURES
]
MX_STRICT_ROUNDTRIP_CASES = MX_EXPORTABLE_CASES


def make_roundtrip_score() -> Score:
    score = Score(480)
    score.tempos.append(Tempo(0, 120))
    score.time_signatures.append(TimeSignature(0, 4, 4))
    score.time_signatures.append(TimeSignature(1920, 3, 4))
    score.key_signatures.append(KeySignature(0, 2, 0))

    piano = Track("Piano", 0, False)
    piano.notes.append(Note(0, 480, 60, 91))
    piano.notes.append(Note(0, 480, 64, 88))
    piano.notes.append(Note(480, 480, 67, 87))
    piano.notes.append(Note(1680, 720, 69, 95))
    piano.lyrics.append(TextMeta(0, "la"))
    piano.lyrics.append(TextMeta(1680, "so"))

    drums = Track("", 0, True)
    drums.notes.append(Note(1920, 240, 36, 100))
    drums.notes.append(Note(2160, 240, 38, 96))

    score.tracks.append(piano)
    score.tracks.append(drums)
    return score


def sample_path(relative_path: str) -> Path:
    return MX_RESOURCES / relative_path


def musicxml_contract_snapshot(score: Score) -> tuple:
    return (
        score.ticks_per_quarter,
        sorted((event.time, round(event.tempo, 9)) for event in score.tempos),
        sorted(
            (event.time, event.numerator, event.denominator)
            for event in score.time_signatures
        ),
        sorted(
            (event.time, event.key, event.tonality) for event in score.key_signatures
        ),
        [
            (
                track.name,
                track.program,
                track.is_drum,
                sorted(
                    (note.start, note.duration, note.pitch, note.velocity)
                    for note in track.notes
                ),
                sorted((lyric.time, lyric.text) for lyric in track.lyrics),
            )
            for track in score.tracks
        ],
    )


def roundtrip_musicxml(score: Score) -> Score:
    return Score.from_musicxml(score.dumps_musicxml())


def test_musicxml_mx_corpus_expectation_sets_are_consistent() -> None:
    corpus = set(MX_CORPUS_CASES)

    assert set(MX_EXPECTED_IMPORT_FAILURES).issubset(corpus)
    assert set(MX_EXPECTED_EXPORT_FAILURES).issubset(corpus)
    assert set(MX_EXPECTED_IMPORT_FAILURES).isdisjoint(MX_EXPECTED_EXPORT_FAILURES)


def test_musicxml_roundtrip_in_memory() -> None:
    score = make_roundtrip_score()
    xml = score.dumps_musicxml()

    assert xml.startswith("<?xml")
    assert musicxml_contract_snapshot(
        Score.from_musicxml(xml),
    ) == musicxml_contract_snapshot(score)


def test_musicxml_file_roundtrip_and_format_detection(tmp_path: Path) -> None:
    score = make_roundtrip_score()
    expected_snapshot = musicxml_contract_snapshot(score)

    musicxml_path = tmp_path / "roundtrip.musicxml"
    xml_path = tmp_path / "roundtrip.xml"

    score.dump_musicxml(musicxml_path)
    xml_path.write_text(score.dumps_musicxml(), encoding="utf-8")

    loaded_musicxml = Score(musicxml_path)
    loaded_xml = Score(xml_path)
    loaded_xml_explicit = Score.from_file(xml_path, format="xml")

    assert musicxml_contract_snapshot(loaded_musicxml) == expected_snapshot
    assert musicxml_contract_snapshot(loaded_xml) == expected_snapshot
    assert musicxml_contract_snapshot(loaded_xml_explicit) == expected_snapshot


def test_musicxml_mxl_rejected(tmp_path: Path) -> None:
    mxl_path = tmp_path / "unsupported.mxl"
    mxl_path.write_bytes(b"placeholder")

    with pytest.raises(
        ValueError,
        match=r"Compressed MusicXML \(.mxl\) is not supported yet",
    ):
        Score(mxl_path)

    with pytest.raises(
        ValueError,
        match=r"Compressed MusicXML \(.mxl\) is not supported yet",
    ):
        Score.from_file(mxl_path, format="musicxml")


def test_musicxml_conversion_is_safe_under_parallel_calls(tmp_path: Path) -> None:
    score = make_roundtrip_score()
    xml = score.dumps_musicxml()
    expected_snapshot = musicxml_contract_snapshot(score)

    def dump_once(index: int) -> tuple:
        path = tmp_path / f"parallel-{index}.musicxml"
        score.dump_musicxml(path)
        return musicxml_contract_snapshot(Score(path))

    def load_once(_: int) -> tuple:
        return musicxml_contract_snapshot(Score.from_musicxml(xml))

    with ThreadPoolExecutor(max_workers=8) as executor:
        dumped = list(executor.map(dump_once, range(4)))
        loaded = list(executor.map(load_once, range(4)))

    assert dumped == [expected_snapshot] * 4
    assert loaded == [expected_snapshot] * 4


@pytest.mark.parametrize("relative_path", MX_CORPUS_CASES)
def test_musicxml_import_covers_full_mx_corpus(relative_path: str) -> None:
    path = sample_path(relative_path)

    if relative_path in MX_EXPECTED_IMPORT_FAILURES:
        with pytest.raises(
            RuntimeError,
            match=MX_EXPECTED_IMPORT_FAILURES[relative_path],
        ):
            Score(path)
        return

    Score(path)


@pytest.mark.parametrize("relative_path", MX_IMPORTABLE_CASES)
def test_musicxml_export_and_reimport_covers_full_mx_corpus(relative_path: str) -> None:
    score = Score(sample_path(relative_path))
    if relative_path in MX_EXPECTED_EXPORT_FAILURES:
        with pytest.raises(RuntimeError, match=MX_HEURISTIC_VOICE_EXPORT_ERROR):
            score.dumps_musicxml()
        return

    Score.from_musicxml(score.dumps_musicxml())


@pytest.mark.parametrize("relative_path", MX_STRICT_ROUNDTRIP_CASES)
def test_musicxml_roundtrip_preserves_supported_contract_for_full_mx_corpus(
    relative_path: str,
) -> None:
    score = Score(sample_path(relative_path))
    reloaded = roundtrip_musicxml(score)

    assert musicxml_contract_snapshot(reloaded) == musicxml_contract_snapshot(score)


def test_musicxml_imports_timewise_documents() -> None:
    score = Score(sample_path("mjbsuite/hello_timewise.xml"))

    assert score.ticks_per_quarter == 1
    assert [
        (event.time, event.numerator, event.denominator)
        for event in score.time_signatures
    ] == [
        (0, 4, 4),
    ]
    assert len(score.tracks) == 1
    assert score.tracks[0].name == "Music"
    assert [
        (note.start, note.duration, note.pitch, note.velocity)
        for note in score.tracks[0].notes
    ] == [
        (0, 4, 60, 64),
    ]


@pytest.mark.parametrize(
    ("relative_path", "expected_tempos"),
    [
        ("ksuite/k012a_Tempo_Markings.xml", [(0, 120.0), (8, 40.0)]),
        ("musuite/testTempo1.xml", [(0, 60.0), (16, 126.000126000126)]),
    ],
)
def test_musicxml_imports_tempos_from_external_samples(
    relative_path: str,
    expected_tempos: list[tuple[int, float]],
) -> None:
    score = Score(sample_path(relative_path))

    assert len(score.tempos) == len(expected_tempos)
    for tempo, (expected_time, expected_value) in zip(
        score.tempos,
        expected_tempos,
        strict=True,
    ):
        assert tempo.time == expected_time
        assert tempo.tempo == pytest.approx(expected_value)


def test_musicxml_roundtrip_preserves_decimal_tempo_precision() -> None:
    score = Score(sample_path("lysuite/ly31c_MetronomeMarks.xml"))
    reloaded = roundtrip_musicxml(score)

    assert [event.time for event in reloaded.tempos] == [
        event.time for event in score.tempos
    ]
    assert [event.tempo for event in reloaded.tempos] == pytest.approx(
        [event.tempo for event in score.tempos],
    )


def test_musicxml_roundtrip_preserves_repeated_time_signatures_at_distinct_times() -> (
    None
):
    score = Score(480)
    score.time_signatures.append(TimeSignature(0, 4, 4))
    score.time_signatures.append(TimeSignature(1920, 4, 4))

    track = Track("Piano", 0, False)
    track.notes.append(Note(0, 480, 60, 90))
    track.notes.append(Note(1920, 480, 62, 91))
    score.tracks.append(track)

    reloaded = roundtrip_musicxml(score)

    assert [
        (event.time, event.numerator, event.denominator)
        for event in reloaded.time_signatures
    ] == [
        (0, 4, 4),
        (1920, 4, 4),
    ]


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
    reloaded = roundtrip_musicxml(score)

    assert [
        (note.start, note.duration, note.pitch, note.velocity)
        for note in reloaded.tracks[0].notes
    ] == [
        (note.start, note.duration, note.pitch, note.velocity)
        for note in score.tracks[0].notes
    ]
    assert [(lyric.time, lyric.text) for lyric in score.tracks[0].lyrics] == [
        (4, "AB"),
        (12, "CDE"),
    ]
    assert [(lyric.time, lyric.text) for lyric in reloaded.tracks[0].lyrics] == [
        (4, "AB"),
        (12, "CDE"),
    ]


def test_musicxml_export_preserves_lyrics_on_tie_continuation_segments() -> None:
    score = Score(480)
    score.time_signatures.append(TimeSignature(0, 4, 4))

    piano = Track("Piano", 0, False)
    piano.notes.append(Note(1440, 960, 60, 90))
    piano.lyrics.append(TextMeta(1920, "tail"))
    score.tracks.append(piano)

    xml = score.dumps_musicxml()
    # The XML is generated by symusic in-process, so parsing it here is trusted test input.
    root = ET.fromstring(xml)  # noqa: S314
    lyric_notes = [
        (
            int(measure.attrib["number"]),
            [tie.attrib.get("type") for tie in note.findall("tie")],
            [text.text or "" for text in note.findall("./lyric/text")],
        )
        for measure in root.findall("./part/measure")
        for note in measure.findall("note")
        if note.findall("./lyric/text")
    ]
    reloaded = Score.from_musicxml(xml)

    assert lyric_notes == [(2, ["stop"], ["tail"])]
    assert [(lyric.time, lyric.text) for lyric in reloaded.tracks[0].lyrics] == [
        (1440, "tail"),
    ]


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
    assert [track.program for track in transposition_score.tracks if track.is_drum] == [
        48,
        48,
    ]


def test_musicxml_import_tolerates_unmatched_raw_parts() -> None:
    score_a = Score(sample_path("lysuite/ly41h_TooManyParts.xml"))
    score_b = Score(sample_path("musuite/testPartsSpecialCases.xml"))

    assert len(score_a.tracks) == 1
    assert len(score_b.tracks) == 1
    assert score_b.tracks[0].name == "Soprano"


def test_musicxml_roundtrip_preserves_duplicate_note_velocities() -> None:
    score = Score(480)
    score.time_signatures.append(TimeSignature(0, 4, 4))

    track = Track("Piano", 0, False)
    track.notes.append(Note(0, 480, 60, 32))
    track.notes.append(Note(0, 480, 60, 96))
    score.tracks.append(track)

    reloaded = roundtrip_musicxml(score)

    assert [
        (note.start, note.duration, note.pitch, note.velocity)
        for note in reloaded.tracks[0].notes
    ] == [
        (0, 480, 60, 32),
        (0, 480, 60, 96),
    ]


def test_musicxml_export_preserves_empty_track_names() -> None:
    score = Score(480)
    score.time_signatures.append(TimeSignature(0, 4, 4))

    track = Track("", 0, False)
    track.notes.append(Note(0, 480, 60, 90))
    score.tracks.append(track)

    reloaded = roundtrip_musicxml(score)

    assert [track.name for track in reloaded.tracks] == [""]


def test_musicxml_export_rejects_tracks_that_need_heuristic_voice_reconstruction() -> (
    None
):
    score = Score(480)
    score.time_signatures.append(TimeSignature(0, 4, 4))

    track = Track("Piano", 0, False)
    track.notes.append(Note(0, 960, 60, 90))
    track.notes.append(Note(480, 480, 64, 91))
    score.tracks.append(track)

    with pytest.raises(RuntimeError, match=MX_HEURISTIC_VOICE_EXPORT_ERROR):
        score.dumps_musicxml()
