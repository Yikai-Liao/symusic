rm -rf dist build *.egg-info
python3 -m pip install -e . --upgrade
pybind11-stubgen symusic.core --numpy-array-remove-parameters -o .
pip uninstall -y symusic
rm -rf dist build *.egg-info
