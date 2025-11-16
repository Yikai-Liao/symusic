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

nanobind 2.9 introduces an official `nanobind_add_stub` CMake command. The
symusic build now relies on this integration, so no standalone script is needed.
Whenever you configure CMake with `-DBUILD_SYMUSIC_PY=ON` (which is the case
for `pip install .`), the build automatically runs the stub generator and
produces both `core.pyi` and a `py.typed` marker inside the build directory.
Those artifacts are then installed next to the compiled `symusic/core` module
and are shipped with wheels and editable installs.

If you need to regenerate the stubs manually, simply rebuild the `symusic_core_stub`
target (e.g. `cmake --build build --target symusic_core_stub`). You will find
the resulting file at `<build>/symusic/core.pyi`.

After touching the bindings, run `pre-commit run --all-files` to keep the stub
and the rest of the sources formatted, and always review the resulting diff
before sending a patch.

## Format

We use `pre-commit` to format the code. You could install it by:

```bash
pip install pre-commit
```

And then, run `pre-commit install` to install the pre-commit script

You could run it manually by `pre-commit run --all-files`, or just run it before committing by `pre-commit run --all-files --hook-stage commit`

The config is in `pyproject.toml`
