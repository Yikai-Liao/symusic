# Track

## Attributes & Properties

| Attribute     | Type              | Description                              |
|---------------|-------------------|------------------------------------------|
| `name`        | str               | Name of the track                        |
| `program`     | int               | Program number of the track (Instrument) |
| `is_drum`     | bool              | Whether the track is a drum track        |
| `notes`       | NoteList          | List of notes in the track               |
| `controls`    | ControlChangeList | List of control changes in the track     |
| `pitch_bends` | PitchBendList     | List of pitch bends in the track         |
| `pedals`      | PedalList         | List of pedals in the track              |
| `lyrics`      | TextMetaList      | List of lyrics in the track              |

| Property-Like | Type                      | Description                            |
|---------------|---------------------------|----------------------------------------|
| `ttype`       | Tick \| Quarter \| Second | Time unit of the track                 |
| `start`()     | int                       | The global start time of the track     |
| `end`()       | int                       | The global end time of the track       |
| `note_num`()  | int                       | The total number of notes in the track |
| `empty`()     | bool                      | Whether the track is empty             |

## Constructors

```python
Track(
    self, name: str = "",
    program: int = 0,
    is_drum: bool = False,
    notes: smt.GeneralNoteList = None,
    controls: smt.GeneralControlChangeList = None,
    pitch_bends: smt.GeneralPitchBendList = None,
    pedals: smt.GeneralPedalList = None,
    ttype: smt.GeneralTimeUnit = TimeUnit.tick
)
```

You could also create a empty Track through `Track.empty()`.

## Conversion

| Method                                                                           | Description                                                                                                                                                     |
|----------------------------------------------------------------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `copy`(self, deep=True)                                                          | Return a deep(default) or shallow copy of the track. Both `copy.copy` and `copy.deepcopy` functions are supported                                               |
| `pianoroll`(self, modes: List[str], pitch_range=(0, 128), encode_velocity=False) | Only for `TickTrack`. Convert the track to a 2D piano-roll matrix (numpy.ndarray) with the given modes. The pitch range and velocity encoding can be specified. |

## Modification

| Method                                                                                | Description                                                                                               |
|---------------------------------------------------------------------------------------|-----------------------------------------------------------------------------------------------------------|
| `clip`(self, start: unit, end: unit, clip_end=False, inplace=False)                   | Clip the track to the given range. If `clip_end` is `True`, notes will be clipped if they end after `end` |
| `adjust_time`(self, original_times: List[unit], new_times: List[unit], inplace=False) | Adjust the time of the events in the track from the original times to the new times(Interpolating)        |
| `sort`(self, reverse=False, inplace=True)                                             | Sort all events in the track by their default compare rules.                                              |
| `shift_time`(self, offset: unit, inplace=False)                                       | Shift the time of all the events in the track by the given offset                                         |
| `shift_pitch`(self, offset: int, inplace=False)                                       | Shift the pitch of all the notes in the track by the given offset                                         |
| `shift_velocity`(self, offset: int, inplace=False)                                    | Shift the velocity of all the notes in the track by the given offset                                      |
