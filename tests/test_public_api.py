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
    with pytest.raises(TypeError):
        Score.from_file("x.mid", fmt="mid")


def test_checked_python_inputs(tmp_path):
    with pytest.raises(ValueError, match="ticks_per_quarter"):
        Score(0)
    with pytest.raises(OverflowError):
        Note(2**31, 1, 60)
    with pytest.raises(TypeError):
        Note(0.5, 1, 60)
    with pytest.raises(FileNotFoundError):
        Score(tmp_path / "missing.mid")
    with pytest.raises(IsADirectoryError):
        Score.from_file(tmp_path, format="midi")


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
        Score.from_file(path)
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
    with pytest.raises(NotImplementedError):
        TextMeta.from_numpy(np.array([0]), np.array(["text"]))
    with pytest.raises(RuntimeError):
        TextMetaList.from_numpy()


def test_pedal_start():
    pedal = Pedal(1, 2)
    pedal.start = 3
    assert pedal.time == 3


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
