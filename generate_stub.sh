#pip install . --user
nanobind-stubgen symusic.core --out .
rm python/symusic/core.pyi
mv symusic.core.pyi python/symusic/core.pyi
#pip uninstall symusic
