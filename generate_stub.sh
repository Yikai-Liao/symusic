rm -rf dist build *.egg-info
python3 setup.py build bdist_wheel
pip install dist/*.whl
pybind11-stubgen symusic.core --numpy-array-remove-parameters -o .
pip uninstall -y symusic
rm -rf dist build *.egg-info
