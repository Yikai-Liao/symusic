## 构建与浏览（使用 venv，并先构建包）

* 创建虚拟环境：`python3 -m venv .venv && source .venv/bin/activate`

* 构建并安装项目：`pip install .`（编译核心并自动生成 `.pyi`，无需手写 `.pyi`）

* 安装文档依赖：`python -m pip install -r docs_rtd/requirements.txt`（若需要 Tabs，增加 `sphinx-design` 并在 `conf.py` 启用）

* 清理旧构建：删除 `docs_rtd/_build/`

* 生成 HTML：`sphinx-build -b html docs_rtd docs_rtd/_build/html`

* 构建搜索索引（Pagefind）并本地预览；浏览 `docs_rtd/_build/html/index.html`

## 评估结论

* 文档基础良好，但“API Reference”（工厂与 `symusic.core`）是最大短板：docstring 与示例不足、签名与分页面质量不高、重复展示与排版混乱。

* 因此将“API Reference”设为最高优先级，先解决可用性与可读性问题。

## 改进重点：API Reference（最高优先级）

* 自动化与规范

  * 在 `docs_rtd/conf.py` 启用 `autosummary_generate=True`，设置 `autodoc_default_options`（`members`、`undoc-members`、`member-order=bysource` 等）、`nitpicky=True`

  * 统一 NumPy 风格 docstring：`Parameters/Returns/Raises/Notes/Examples/See Also`

  * 以编译生成的 `.pyi` 与 `py_src` 注释为信息源，无需额外 `.pyi`

* 版式与结构（T/Q/S 选项卡）

  * 引入 `sphinx-design`（或 `sphinx-tabs`）以支持 MyST Tabs，避免为 Tick/Quarter/Second 重复出三份页面

  * 每个核心类页面设置三个选项卡：Tick、Quarter、Second；各 Tab 内使用 `.. autoclass::` 展开成员，并在页首提供对照表（语义差异、场景建议、性能提示）

  * 为工厂 API 建立任务导向索引页：读取/写入、事件操作、时间换算、合成/导出、批处理，统一入口并交叉引用到具体类/方法

* 示例与可读性

  * 每个公共方法至少提供 1–2 个可复制示例；复杂行为补充边界条件与性能说明

  * 打开 `genindex`/`py-modindex` 与 `viewcode`（已配置），确保类型与术语交叉引用可跳转

## 次级改进（在 API 优化后推进）

* 用户指南与 Cookbook：I/O、时间单位与换算、事件与轨道操作、合成与导出、与 NumPy/Polars/Matplotlib 的互操作；整理 How-to 索引

* 示例画廊与可执行教程：选择性启用 `myst-nb` 执行或离线输出，提供图示/音频片段与缩略图导航

* 版本化与发布：在 Read the Docs 启用 stable/dev 多版本与切换；增加发行说明/迁移指南

* 搜索与导航优化：完善 Pagefind 摘要与权重；新增术语表（Glossary）与任务入口页

## 交付节奏与验收

* 里程碑 1（API Reference）：完成 T/Q/S 选项卡版式、autosummary 与 docstring 统一、示例补齐；通过完整构建与链接检查验收

* 里程碑 2（用户指南/Cookbook 与示例画廊）：完成主题索引与执行输出；抽样对比 Polars 的 User Guide 与 Cookbook

* 里程碑 3（版本化与搜索优化）：RTD 多版本启用、Pagefind 权重调优、术语表与索引完备

* 每次里程碑交付：提供“可浏览的 HTML 构建 + 变更清单”，并记录缺失交叉引用的修复清单

