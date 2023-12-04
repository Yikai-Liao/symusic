from typing import Union
from pathlib import Path

from . import core  # type: ignore
from . import protoc as smp

__all__ = ['TimeUnit', 'Score']

"""
All the Factory classes are initialized when the module is imported.
And the objects are created when the factory is called.
Note that the factory can not be created again by users,
because the factory is not exposed to the users by setting __all__ manually.
"""

class TimeUnitFactory:
    def __init__(self) -> None:
        self._tick = core.Tick()
        self._quarter = core.Quarter()
        self._second = core.Second()

    @property
    def tick(self) -> smp.TimeUnit:
        return self._tick
    
    @property
    def quarter(self) -> smp.TimeUnit:
        return self._quarter
    
    @property
    def second(self) -> smp.TimeUnit:
        return self._second

    def __call__(self, time_unit: Union[smp.TimeUnit, str]) -> smp.TimeUnit:
        """
        Create a TimeUnit object from a str, e.g. 'tick', 'quarter', 'second'
        It is used to dispatch the correct TimeUnit object.
        However, it is recommended to use the TimeUnit.tick, TimeUnit.quarter, TimeUnit.second for better performance.
        """
        if isinstance(time_unit, str):
            return self.from_str(time_unit)
        try:
            time_unit.is_time_unit()
            return time_unit
        except AttributeError:
            raise TypeError(f'{time_unit} is not a TimeUnit object')

    def from_str(self, time_unit: str) -> smp.TimeUnit:
        time_unit = time_unit.lower()
        if time_unit == 'tick':
            return self.tick
        elif time_unit == 'quarter':
            return self.quarter
        elif time_unit == 'second':
            return self.second
        else:
            raise ValueError(f'Invalid time unit: {time_unit}')
        
TimeUnit = TimeUnitFactory()

class ScoreFactory:

    def __call__(
        self, file: Union[str, Path],
        time_unit: smp.GeneralTimeUnit = TimeUnit.tick):
        
        if isinstance(file, str) or isinstance(file, Path):
            return self.from_file(file, time_unit)
        

    def from_file(
        self, path: Union[str, Path], 
        time_unit: smp.GeneralTimeUnit = TimeUnit.tick):

        return self.get_core_class(time_unit)(str(path))
        

    def get_core_class(self, time_unit: smp.GeneralTimeUnit):

        unit = TimeUnit(time_unit)

        if unit == TimeUnit.tick:
            return core.ScoreTick
        elif unit == TimeUnit.quarter:
            return core.ScoreQuarter
        elif unit == TimeUnit.second:
            raise NotImplementedError('ScoreSecond is not implemented yet')
        else:
            raise ValueError(f'Invalid time unit: {unit}')
        
Score = ScoreFactory()