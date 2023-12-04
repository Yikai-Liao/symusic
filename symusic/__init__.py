from typing import Union
from . import core
from . import utils


class Score:
    def __new__(cls, path):
        return cls.from_file(path)

    @classmethod
    def from_file(cls, path):
        return core.ScoreTick(path)
