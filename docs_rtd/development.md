# Development

## Environment

Since symusic is bound to python using nanobind, it would be a bit complicated to set a development environment.

The key problem is that, you need to let you IDE detect the right python evn on your system. Here, I recommend you to open your ide (like clion or vscode) in your terminal, after you have activated your python env.

```bash
conda activate symusic-env
clion path/to/symusic
```

You could also set your http proxy before open your ide in terminal.

The only requirement for symusic is [nanobind](https://github.com/wjakob/nanobind). You could install it by:

```bash
pip install nanobind
```

> Note that, cpython 3.12 get some memory leak problem, and it will be detected by nanobind. So I recommend you to use cpython <= 3.11 currently.

As for compiler, it should support c++20. `clang` (linux and mac), `gcc` and `msvc` all works (I have tested them).

## Build

If you want to compile the python binding target, you need to set `-DBUILD_PY:BOOL=ON` when using cmake. Also, `-DBUILD_TEST:BOOL=ON` for some test cases.

If you are using clion, just set the cmake options to:

```text
-DBUILD_TEST:BOOL=ON -DBUILD_PY:BOOL=ON
```

For installing symusic to your python env, you could use `pip install .` in the root directory of symusic. And everything is done.

## PYI File

nanobind 2.9 introduces an official `nanobind_add_stub` CMake command. The symusic build now relies on this integration, so no standalone script is needed. Whenever you configure CMake with `-DBUILD_SYMUSIC_PY=ON` (which is the case for `pip install .`), the build automatically runs the stub generator and produces both `core.pyi` and a `py.typed` marker inside the build directory. Those artifacts are then installed next to the compiled `symusic/core` module and are shipped with wheels and editable installs.

If you need to regenerate the stubs manually, simply rebuild the `symusic_core_stub` target (e.g. `cmake --build build --target symusic_core_stub`). You will find the resulting file at `<build>/symusic/core.pyi`.

After touching the bindings, run `pre-commit run --all-files` to keep the stub and the rest of the sources formatted, and always review the resulting diff before sending a patch.

## Type hints

Symusic exposes a relatively rich typing surface so that downstream projects can benefit from IDE completion and static analysis:

### Core generics

- The nanobind bindings now “lie” in the generated stubs so that classes like `ScoreTick`, `TrackQuarter`, `NoteTickList`, … derive from generic helpers. This enables Python code to annotate values using `core.Score[core.Tick]` or `core.PyVec[core.Note[core.Tick]]`.
- The Python package mirrors these helpers. Importing `import symusic.types as smt` gives you `smt.Score`, `smt.Note`, `smt.PyVec`, etc., that map directly to the nanobind types. Prefer the generic form when you need an explicit time unit, e.g.

```python
import symusic.core as core
import symusic.types as smt

score: smt.Score[core.Tick] = core.ScoreTick(480)
notes: core.PyVec[core.Note[core.Tick]] = core.NoteTickList()
```

### List aliases

- To keep runtime behavior simple, the convenience aliases exposed from `symusic.types` (e.g. `NoteList`, `TimeSignatureList`) remain ordinary `Union` types over their tick/quarter/second variants. Downstream code should treat them as “either of the concrete `*List` classes” rather than full generics.
- `General*List` helpers now collapse to `Union[List[Event], <list alias>]`, so existing factory signatures keep accepting plain Python lists without any type juggling.

### Testing the typing surface

- The file `tests/typecheck/test_typing_mypy.py` contains narrow mypy-based regression tests that exercise the combinations we care about (`Score[T]`, `PyVec[Event[T]]`, `General*List`, etc.). Run it after touching bindings or the `types` module:

```bash
pip install .[test]
pytest tests/typecheck/test_typing_mypy.py
```

- These tests rely on the freshly built `symusic` package inside the active virtualenv. Always reinstall (`pip install .` or `pip install .[test]`) or copy the updated files into `.venv/lib/.../symusic/` before invoking pytest, otherwise mypy will keep validating stale stubs.

## Format

We use `pre-commit` to format the code. You could install it by:

```bash
pip install pre-commit
```

And then, run `pre-commit install` to install the pre-commit script.

You could run it manually by `pre-commit run --all-files`, or just run it before committing by `pre-commit run --all-files --hook-stage commit`.

The config is in `pyproject.toml`.