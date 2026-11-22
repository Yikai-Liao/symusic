"""Provide the native offline FluidSynth runtime for symusic."""

from pathlib import Path

from ._library import LIBRARY_NAME

__all__ = ["library_path"]


def library_path() -> Path:
    """Return the absolute path of the bundled FluidSynth shared library."""
    return Path(__file__).resolve().with_name(LIBRARY_NAME)
