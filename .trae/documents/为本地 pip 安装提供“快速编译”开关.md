## 目标
- 本地 `pip install .` 时，支持一个简洁的开关关闭 LTO/高优化，并统一降到 `-O1` 以缩短编译时间。

## 现状与切入点
- 构建体系：`pyproject.toml` 使用 `scikit_build_core.build`（`py_src` + C++/CMake/nanobind）。参见 `pyproject.toml:7`、`pyproject.toml:63–70`。
- 顶层 CMake 定义了 `ENABLE_LTO`，默认开启，并在支持时为 `symusic`/`core` 启用 IPO（`CMakeLists.txt:12`、`CMakeLists.txt:152–168`）。
- Python 扩展通过 `nanobind_add_module` 构建，当前传入了 `LTO` 等标志（`CMakeLists.txt:82–95`）。
- 默认 Release 构建常见编译器会使用 `-O3`（CMake 内置），nanobind 模块在优化构建类型下还会加 `-Os`。

## 实现方案
1. 在顶层 `CMakeLists.txt` 增加选项：`option(SYMUSIC_FAST_BUILD "Use O1 and disable LTO for faster local builds" OFF)`。
2. 当 `SYMUSIC_FAST_BUILD=ON` 时：
   - 关闭 LTO：`set(ENABLE_LTO OFF)` 并绕过为目标设置 IPO；同时在 `nanobind_add_module` 的实参中条件性去掉 `LTO`。
   - 将优化级别统一调整到 O1：
     - GNU/Clang：覆盖 `CMAKE_C_FLAGS_RELEASE/CMAKE_CXX_FLAGS_RELEASE` 为 `-O1 -DNDEBUG`；同样覆盖 `RELWITHDEBINFO` 为 `-O1 -g -DNDEBUG`。
     - MSVC：设置 `CMAKE_C_FLAGS_RELEASE/CMAKE_CXX_FLAGS_RELEASE` 为 `/O1 /DNDEBUG`（及 `RelWithDebInfo` 变体）。
   - 禁用 nanobind 的“最小体积优化”以避免 `-Os` 覆盖：在 `FAST_BUILD` 分支给 `nanobind_add_module` 传入 `NOMINSIZE`。
3. 保持默认行为不变：未开启时仍按当前 LTO 与优化策略构建。

## 使用方式
- 新开关（推荐）：`pip install . -C cmake.define.SYMUSIC_FAST_BUILD=ON`
- 备用（无需改代码的即时方案，等价效果）：
  - `pip install . -C cmake.define.ENABLE_LTO=OFF -C cmake.build-type=RelWithDebInfo -C cmake.args=-DCMAKE_C_FLAGS_RELWITHDEBINFO=-O1 -C cmake.args=-DCMAKE_CXX_FLAGS_RELWITHDEBINFO=-O1`

## 验证步骤
- 本地执行两次构建：
  - 基线：`pip install .`（观察编译时间与最终二进制体积）。
  - 快速模式：`pip install . -C cmake.define.SYMUSIC_FAST_BUILD=ON`。
- 打开详细编译输出（例如添加 `-C cmake.verbose=true` 或在 CMake 内打印状态），确认：
  - 不再出现 `-flto`/IPO 启用日志（参考 `CMakeLists.txt:152–168`）。
  - 编译命令使用 `-O1`（或 MSVC 的 `/O1`），无 `-O3/-Os`。
- 运行包内基础导入与核心功能的快速校验，确保功能与接口不受影响。

## 代码改动范围
- 修改 `CMakeLists.txt`：新增 `SYMUSIC_FAST_BUILD` 选项；在 LTO 与 `nanobind_add_module` 处加条件分支；设置优化级别覆盖（含 MSVC）。
- 不改动 `pyproject.toml` 默认定义，仅通过 `--config-settings` 传参驱动此开关。