"""Utils methods for tests."""

from __future__ import annotations

from pathlib import Path
from typing import TYPE_CHECKING

from symusic import Score, Track

if TYPE_CHECKING:
    from collections.abc import Sequence

    from symusic.core import TrackTickList

HERE = Path(__file__).parent

TESTCASES_PATH = HERE / "testcases"
MIDI_PATHS_ONE_TRACK = sorted((TESTCASES_PATH / "One_track_MIDIs").rglob("*.mid"))
MIDI_PATHS_MULTITRACK = sorted((TESTCASES_PATH / "Multitrack_MIDIs").rglob("*.mid"))
MIDI_PATHS_CORRUPTED = sorted((TESTCASES_PATH / "MIDIs_corrupted").rglob("*.mid"))
MIDI_PATHS_ALL = MIDI_PATHS_ONE_TRACK + MIDI_PATHS_MULTITRACK
print(TESTCASES_PATH)
print(MIDI_PATHS_ONE_TRACK)


def merge_tracks(
    tracks: list[Track] | TrackTickList | Score,
    effects: bool = True,
) -> Track:
    r"""Merge several ``symusic.Track``\s.

    The notes (and optionally effects) will be concatenated and sorted by time.
    All the tracks will be merged into the first ``Track`` of the list.

    :param tracks: list of tracks to merge, or ``symusic.Score`` object.
    :param effects: will also merge effects, i.e. control changes, sustain pedals and
        pitch bends. (default: ``True``)
    :return: the merged track.
    """
    tracks_ = tracks.tracks if isinstance(tracks, Score) else tracks

    # Change name
    tracks_[0].name += "".join([" / " + t.name for t in tracks_[1:]])

    # Gather and sort notes
    notes_sum = []
    for track in tracks_:
        notes_sum += track.notes
    tracks_[0].notes = notes_sum
    tracks_[0].notes.sort()
    if effects:
        # Pedals
        pedals_sum, cc_sum, pb_sum = [], [], []
        for track in tracks_:
            pedals_sum += track.pedals
            cc_sum += track.controls
            pb_sum += track.pitch_bends
        tracks_[0].pedals = pedals_sum
        tracks_[0].pedals.sort()
        # Control changes
        tracks_[0].controls = cc_sum
        # tracks_[0].controls = sum((t.controls for t in tracks_), [])
        tracks_[0].controls.sort()
        # Pitch bends
        tracks_[0].pitch_bends = pb_sum
        tracks_[0].pitch_bends.sort()

    # Keeps only one track
    if isinstance(tracks, Score):
        tracks.tracks = [tracks_[0]]
    else:
        for _ in range(1, len(tracks)):
            del tracks[1]
        tracks[0] = tracks_[0]
    return tracks_[0]


def merge_scores(scores: Sequence[Score]) -> Score:
    r"""Merge a list of ``symusic.Score``\s into a single one.

    This method will combine all their tracks and global events such as tempo changes
    or time signature changes.

    :param scores: ``symusic.Score``\s to merge.
    :return: the merged ``symusic.Score``.
    """
    if not all(score.tpq == scores[0].tpq for score in scores):
        msg = (
            "Some `Score`s have different time divisions (`tpq`). They must be all"
            "identical in order to merge the `Scores`s."
        )
        raise ValueError(msg)

    score = Score(scores[0].tpq)
    for score_split in scores:
        score.tracks.extend(score_split.tracks)
        score.tempos.extend(score_split.tempos)
        score.time_signatures.extend(score_split.time_signatures)
        score.key_signatures.extend(score_split.key_signatures)
        score.lyrics.extend(score_split.lyrics)
        score.markers.extend(score_split.markers)

    # sorting is done by Symusic automatically
    return score


def concat_scores(scores: Sequence[Score], end_ticks: Sequence[int]) -> Score:
    r"""Concatenate a sequence of ``symusic.Score``\s.

    **Note:** the tracks are concatenated in the same order as they are given.
    **The scores must all have the same time division.** (``score.tpq``)
    The concatenated score might have identical consecutive tempos, time signatures or
    key signatures values.

    :param scores: ``symusic.Score``\s to concatenate.
    :param end_ticks: the ticks indicating the end of each score. The end for the last
        score is not required.
    :return: the concatenated ``symusic.Score``.
    """
    if not all(
        score.ticks_per_quarter == scores[0].ticks_per_quarter for score in scores
    ):
        err_msg = "The provided scores do not have all the same time division."
        raise ValueError(err_msg)
    if not len(end_ticks) >= len(scores) - 1:
        err = f"Missing end values: got {len(end_ticks)}, expected {len(scores) - 1}."
        raise ValueError(err)

    # Create the concatenated Score with empty tracks
    score_concat = Score(scores[0].ticks_per_quarter)
    score_concat.tracks = scores[0].tracks.copy()

    for mi in range(len(scores)):
        # Shift the Score
        score = scores[mi].copy()
        if mi > 0:
            score = score.shift_time(end_ticks[mi - 1])

        # Concatenate global messages
        score_concat.tempos.extend(score.tempos)
        score_concat.time_signatures.extend(score.time_signatures)
        score_concat.key_signatures.extend(score.key_signatures)
        score_concat.markers.extend(score.markers)

        # Concatenate track messages (except for the first Score)
        if mi == 0:
            continue
        for ti, track in enumerate(score.tracks):
            score_concat.tracks[ti].notes.extend(track.notes)
            score_concat.tracks[ti].controls.extend(track.controls)
            score_concat.tracks[ti].pitch_bends.extend(track.pitch_bends)
            score_concat.tracks[ti].pedals.extend(track.pedals)
            score_concat.tracks[ti].lyrics.extend(track.lyrics)

    return score_concat
