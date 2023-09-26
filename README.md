# pyscore

A cross platform `note level` midi decoding libaray with lightening speed.
The libaray is written in cpp and offers a python binding using pybind11.

Notice that this libaray is under development:
* Writing back to midi files is currently not supported
* Not all features in midi are suppored, like lyrics and pitchbend
* There is no pre-built wheel yet
* We haven't uploaded it to pypi

## Installation

```bash
git clone git@github.com:Yikai-Liao/pyscore.git --recursive
cd pyscore
python setup.py bdist_wheel
```

This will create a dist folder and generate a `.whl` file in it, like `pyscore-0.0.1-cp311-cp311-win_amd64.whl`.
And just install the `.whl` file.

```bash
pip install ./pyscore-0.0.1-cp311-cp311-win_amd64.whl
```
