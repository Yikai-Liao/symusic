pip install . --user
pybind11-stubgen symusic.core --numpy-array-remove-parameters -o .
pip uninstall symusic
