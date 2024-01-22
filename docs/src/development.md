# Development

## Environment

Since symusic is bound to python using nanobind, it would be a bit complicated to set a development environment.

The key problem is that, you need to let you IDE detect the right python evn on your system.
Here, I recommend you to open your ide (like clion or vscode) in your terminal, after you have activated your python env.

```bash
conda activate symusic-env
clion path/to/symusic
```

You could also set your http proxy before open your ide in terminal.

The only requirement for symusic is [nanobind](https://github.com/wjakob/nanobind). You could install it by:

```bash
pip install nanobind
```

> Note that, cpython 3.12 get some memory leak problem, and it will be detected by nanobind.
> So I recommend you to use cpython <= 3.11 currently.

As for compiler, it should support c++20. `clang` (linux and mac), `gcc` and `msvc` all works (I have tested them).

## Build

If you want to compile the python binding target, you need to set `-DBUILD_PY:BOOL=ON` when using cmake.
Also, `-DBUILD_TEST:BOOL=ON` for some test cases.

If you are using clion, just set the cmake options to:
```text
-DBUILD_TEST:BOOL=ON -DBUILD_PY:BOOL=ON
```

For installing symusic to your python env, you could use `pip install .` in the root directory of symusic.
And everything is done.

## PYI File

Here, we use `nanobind-stubgen` to generate the pyi file for symusic.core. And we wrote a `generate_stub.sh` to do this.

> Note that the 0.1.4 version of nanobind-stubgen get some problem, so we use the 0.1.3 version here.

After generating the pyi file, you need run `pre-commit run --all-files` to format the pyi file.

And always check the git diff before committing.
The import part of the auto generated pyi file is not correct, and you need to fix it manually.
(Most of the time, you just need to keep the previous import part.)

## Format

We use `pre-commit` to format the code. You could install it by:

```bash
pip install pre-commit
```

And then, run `pre-commit install` to install the pre-commit script

You could run it manually by `pre-commit run --all-files`, or just run it before committing by `pre-commit run --all-files --hook-stage commit`

The config is in `pyproject.toml`
