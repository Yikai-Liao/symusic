## 原则
- 所有渲染问题通过修改绑定侧 docstring 解决，不在 docs_rtd 下加钩子或二次加工。
- 恢复并稳定“方法签名可见”、属性/方法分组清晰、示例为可复制的多行代码块。

## 修改范围与具体变更（py_src）
- 文件：`py_src/bindings/track/track_bindings.h`
  - 删除模板中的分隔线/节标题（如“Time semantics”“--------------”），改为普通列表说明（Notes）。
  - 将示例统一为 `.. code-block:: python`，每行四空格缩进，多行示例完整在块内；示例不含 `>>>`。
  - 为属性追加 docstring（类型+简述）：`notes/controls/pedals/pitch_bends/lyrics/is_drum/program/name/ttype`。
  - 所有方法保持简洁说明，签名由页面 `automethod` 呈现。
- 文件：`py_src/bindings/score/score_bindings.h`
  - 同步清理分隔线/标题，`Parameters` 改为规范字段；`Notes` 内放时间语义说明。
  - 示例改为多行代码块（缩进齐全），不含 `>>>`。
  - 为属性补 docstring：`ticks_per_quarter/tpq/tracks/time_signatures/key_signatures/tempos/markers/ttype`。
- 文件：`py_src/bindings/events/event_bindings.h`
  - 统一每个事件类模板（Note/ControlChange/Tempo/Pedal/PitchBend/KeySignature/TimeSignature/TextMeta）：
    - 移除“Time semantics”标题与分隔线，改为 Notes 列表；文本不含反引号对象引用。
    - 若有示例，改为多行代码块（每行缩进）。
  - 为关键属性补 docstring（`time/duration/pitch/velocity/...`），“Type: … + 简述”。
- 文件：`py_src/bindings/docstring_helpers.h`
  - 精简时间度量描述，去掉 `tpq/tempo` 的反引号与易触发锚点的措辞（已按此口径调整）。

## 页面与生成（轻度配置，非钩子）
- Tracks/Score/Events 页面结构（`docs_rtd/api/core/*.md`）：
  - 顶部 `.. autoclass::` 仅作类简介。
  - 下方分组：`Attributes` 用 `.. autoattribute::`；`Methods` 用 `.. automethod::`，确保函数签名在标题内显示；避免再使用 `:members:` 引起混排。
  - 保持样式隐藏类名前缀，方法标题更简洁。
- High-level API 修复：
  - `docs_rtd/api/index.md` 的 autosummary 使用完全限定名并输出至 `generated/`；如仍生成 `autodata`，改为手写 `generated/symusic.Score.rst`，用 `.. autoclass:: symusic.Score` + 分组，保证不为空。

## 构建与验证
- 使用 venv，`pip install .` 后清理构建：`rm -rf docs_rtd/_build && sphinx-build -b html docs_rtd docs_rtd/_build/html`。
- 验证点：
  - 不再出现分隔线或异常括号/句点的参数项；示例为可复制的多行代码块；属性与方法分组清晰且函数签名可见；属性显示类型与注释。
  - High-level（symusic.Score 等）页面非空并含分组。

## 说明
- 不再使用 docs_rtd 扩展钩子做内容纠偏；所有矫正直接改 docstring 源。
- 你之前认可的“方法签名可见、属性与方法分开”会被稳定保留，不再回退为混合展示。