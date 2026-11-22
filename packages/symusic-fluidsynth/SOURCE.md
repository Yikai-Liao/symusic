# Corresponding source

This package ships the FluidSynth and libsndfile shared libraries under LGPL-2.1-or-later. The license texts for these libraries and their Ogg, Vorbis and GCEM dependencies are in this directory. The libraries are replaceable at their installed paths.

The symusic release with the same version includes a corresponding-source archive, `symusic-fluidsynth-source.tar.gz`, at https://github.com/Yikai-Liao/symusic/releases. It contains the pinned sources and the build recipe. Extract it and run `python -m pip wheel ./packages/symusic-fluidsynth`. To rebuild from Git, check out the same release tag and run `git submodule update --init --recursive` before running the wheel command.
