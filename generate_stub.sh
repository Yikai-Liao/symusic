pip install . --user
pybind11-stubgen symusic.core --numpy-array-remove-parameters -o .
ruff format symusic/core.pyi
pip uninstall symusic