try:
    from typing import Protocol # type: ignore
except ImportError:
    from typing_extensions import Protocol  # type: ignore

from typing import Union

__all__ = ['TimeUnit', 'GeneralTimeUnit']

class TimeUnit(Protocol):
    def __repr__(self) -> str: ...

    def is_time_unit(self) -> bool: ...

GeneralTimeUnit = Union[TimeUnit, str]