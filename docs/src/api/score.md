# Score

## Attributes & Properties

| Attribute           | Type              | Description                          |
|---------------------|-------------------|--------------------------------------|
| `ticks_per_quarter` | int               | Number of ticks per quarter note     |
| `tpq`               | int               | Alias for `ticks_per_quarter`        |
| `tracks`            | TrackList         | List of tracks in the score          |
| `time_signatures`   | TimeSignatureList | List of time signatures in the score |
| `key_signatures`    | KeySignatureList  | List of key signatures in the score  |
| `tempos`            | TempoList         | List of tempos in the score          |
| `markers`           | TextMetaList      | List of markers in the score         |

| Property-Like | Type                      | Description                            |
|---------------|---------------------------|----------------------------------------|
| `ttype`       | Tick \| Quarter \| Second | Time unit of the score                 |
| `start`()     | int                       | The global start time of the score     |
| `end`()       | int                       | The global end time of the score       |
| `note_num`()  | int                       | The total number of notes in the score |
| `empty`()     | bool                      | Whether the score is empty             |

## Constructors & IO

| Method                                                                              | Description                                                  |
|-------------------------------------------------------------------------------------|--------------------------------------------------------------|
| `Score`(self, tpq: int = 960, ttype: GeneralTimeUnit)                               | Create a new score with the given `tpq`                      |
| `Score`(self, other: Score, ttype: GeneralTimeUnit)                                 | Create a new score by deep copying the `other` score         |
| `Score`(self, path: Union[str, Path], ttype: GeneralTimeUnit, fmt: Optional[str])   | Load a score from the given file path (midi or abc notation) |
| `from_file`(cls, path: Union[str, Path], ttype: GeneralTimeUnit, fmt: Optional[str] | Load a score from the given file path (midi or abc notation) |
| `from_midi`(cls, data: bytes, ttype: GeneralTimeUnit)                               | Load a score from the given midi data (bytes)                |
| `from_abc`(cls, data: str, ttype: GeneralTimeUnit)                                  | Load a score from the given abc notation data (str)          |
| `from_tpq`(cls, tpq: int = 960, ttype: GeneralTimeUnit)                             | Create a new score with the given `tpq`                      |
| `from_other`(cls, other: Score, ttype: GeneralTimeUnit)                             | Create a new score by deep copying the `other` score         |
| `dump_midi`(self, path: Union[str, Path])                                           | Dump the score to a midi file at the given path              |
| `dumps_midi`(self)                                                                  | Dump the score to a midi file and return the bytes           |
| `dump_abc`(self, path: Union[str, Path])                                            | Dump the score to an abc file at the given path              |
| `dumps_abc`(self)                                                                   | Dump the score to an abc file and return the str             |

Note that `pickle` is supported for serialization and deserialization.

## Conversion

| Method                                                                           | Description                                                                                                                                                     |
|----------------------------------------------------------------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `copy`(self, deep=True)                                                          | Return a deep(default) or shallow copy of the score. Both `copy.copy` and `copy.deepcopy` functions are supported                                               |
| `to`(self, ttype, min_dur: Optional[target_unit])                                | Convert the score to a new `Score` with the given `ttype`                                                                                                       |
| `pianoroll`(self, modes: List[str], pitch_range=(0, 128), encode_velocity=False) | Only for `TickScore`. Convert the score to a 3D piano-roll matrix (numpy.ndarray) with the given modes. The pitch range and velocity encoding can be specified. |

## Modification

| Method                                                                                | Description                                                                                               |
|---------------------------------------------------------------------------------------|-----------------------------------------------------------------------------------------------------------|
| `resample`(self, tpq: int, min_dur: Optional[int])                                    | Resample a score of any `ttype` into a new `TickScore` with the given `tpq`                               |
| `clip`(self, start: unit, end: unit, clip_end=False, inplace=False)                   | Clip the score to the given range. If `clip_end` is `True`, notes will be clipped if they end after `end` |
| `adjust_time`(self, original_times: List[unit], new_times: List[unit], inplace=False) | Adjust the time of the events in the score from the original times to the new times(Interpolating)        |
| `sort`(self, reverse=False, inplace=True)                                             | Sort all events in the score by their default compare rules. The order of tracks would also be sorted.    |
| `shift_time`(self, offset: unit, inplace=False)                                       | Shift the time of all the events in the score by the given offset                                         |
| `shift_pitch`(self, offset: int, inplace=False)                                       | Shift the pitch of all the notes in the score by the given offset                                         |
| `shift_velocity`(self, offset: int, inplace=False)                                    | Shift the velocity of all the notes in the score by the given offset                                      |
