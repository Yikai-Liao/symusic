#pip install . --user
nanobind-stubgen symusic.core --out .
rm symusic/core.pyi
mv symusic.core.pyi symusic/core.pyi
#pip uninstall symusic
