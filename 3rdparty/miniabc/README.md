# miniabc

miniabc is a lightweight CMake-based placeholder that will be extracted as a standalone project later.

## Layout

- `CMakeLists.txt` defines the `miniabc` static library with optional Lexy-based parsing helpers.
- `include/miniabc/miniabc.h` exports the minimal public API.
- `src/miniabc.cpp` provides the implementation target consumers would link against.
- `3rdparty/lexy` is intended for the Lexy submodule pinned to `v2025.05.0`.

## Usage

Invoke CMake from the project root or include this directory as a subdirectory in a larger build.

```sh
cmake -S 3rdparty/miniabc -B build-miniabc
cmake --build build-miniabc
```

Set `MINIABC_USE_LEXY=OFF` if you want to build without the Lexy dependency.
