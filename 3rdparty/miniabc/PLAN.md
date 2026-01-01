# miniabc 架构重构与演进计划（修订版）

> 目标：以 **Symbolic-First + 可等价回写** 为核心，彻底清理过重的数据模型与不清晰的基础类型组织，重建解析/语义/渲染三段式架构。核心交付是替换 `3rdparty/abcmidi`，实现 **Symusic Score <-> ABC String** 的可靠双向转换。重构允许激进但必须 **可验证、可回写、可扩展**。

## 1. 现状诊断（问题不限于示例）

### 1.1 结构与语义层混乱
- AST/语义层/渲染层职责边界不清晰，导致状态污染、不可回写。
- 语义层过早转换为 MIDI（信息不可逆），削弱了完整语义表达与等价回写能力。

### 1.2 数据模型臃肿与类型不合理
- **核心事件类型偏重**：`NoteEvent/ChordEvent/RestEvent/BarEvent` 含大量 `string/vector/bool` 混排，体积偏大。
- **语义字段使用 string**：accidental、barline、note letter、tie/grace 等语义应为枚举或小型整数，避免无效值。
- **字段排布与对齐浪费**：`int/bool/string/vector` 混排，产生 padding 与冗余拷贝。
- **基础类型分散**：`support.h`/`fraction.h`/`error.h` 重叠，语义边界不清晰。

### 1.3 解析与错误定位不足
- Lexy 仅用于抓字符串，语义解析仍依赖手写 `parse_xxx`。
- 错误定位粗糙，字段内部无法精确报错。

### 1.4 标准覆盖与可扩展性不足
- 宏/用户符号/重复结尾等 ABC 语法未覆盖。
- 对 ABC 扩展与元数据（I: 指令、U: 定义、m: 宏）缺乏结构化承载。

### 1.5 基于 abc_standard.md 的缺口清单（当前未覆盖或仅部分覆盖）
- **文件与结构**：多曲目 tunebook、嵌入式片段/嵌入式曲目、自由文本/排版文本区块。
- **行续接**：字段续接 `+:`、音乐行续接 `\` 的跨行合并语义（含注释/指令穿插）。
- **信息字段**：除 `X/T/M/L/Q/K/V/I` 外的大量字段未结构化承载（如 `C/O/A/Z/N/G/H/R/P/B/D/F/S` 等）。
- **Key 语法扩展**：模式、显式/修正调号（`K:... exp` 与附加 accidentals）、以及 `K:` 中的 clef/transpose/octave/stafflines/middle 等参数。
- **Accidental 规则与指令**：如 `%%propagate-accidentals`、`%%writeout-accidentals` 及相关 playback/notation 语义。
- **Barline 与重复结构**：全量 barline 语法（如 `::`、`|:|`、`|[|` 等）与 `|1`/`|2`/`[1`/`[2` 变体结尾。
- **Slur 与 Tie**：slur `(` `)` 与 tuplet 语法冲突未解决；tie 仅支持单音，未覆盖和弦/跨小节边界语义。
- **Broken rhythm**：`>>`/`<<` 多级点附语义未支持。
- **Symbol line / Spacer rest**：`s`/`s:` 等符号线与 spacer rest 未支持。
- **Decorations / Redefinable symbols**：`U:` 映射、`+` 语法、装饰音作用域/顺序规则未覆盖。
- **Annotations / Chord symbols**：带位置的注释（`^"text"`、`_"text"`、`>"text"` 等）未区分；和弦符号语法未结构化解析。
- **Chords**：和弦级别装饰与长度/前后缀规则未完善。
- **Voices**：`V:` 语义字段（name/clef/transpose 等）与 voice overlay `&`、`%%score` 分组未支持。
- **Lyrics**：编号/多段歌词、对齐规则、字段续接与 `W:` block 的完整语义未实现。
- **Macros**：`m:` 静态/转调宏未实现（含展开时机与与 `U:` 的交互）。
- **Directives**：`%%` 与 `I:` 指令未解析为结构化配置（播放/排版/意外规则）。

### 1.6 基于 abcmidi 的兼容性缺口（新增）
- `%%abc-include`/`%%abc-version` 处理缺失（含递归保护与版本影响）。
- **微分音/调律**：microtone accidentals 与 `%%temperament` 未支持。
- **复杂拍号**：`M:(2+3+2)/8` 等复杂拍号分组缺少结构表达。
- **Tempo 文本**：`Q:"Allegro" 1/4=120 "rit."` 等前后缀文本未承载。
- **语音参数完整性**：`V:` 的 `name/sname/middle/other` 未结构化。
- **类型与排版 token**：`y` spacing、`$` score linebreak、`!`/`*` 行末控制符未建模。
- **扩展 overlay**：`(& ... &)` 未建模（仅有 `&` split）。
- **旧式和弦**：`+CE+` 旧和弦约定未兼容（可作为兼容解析分支）。
- **P: 在曲体内**：`P:` 作为分段标记（body-level part）未建模。
- **%%MIDI 指令**：program/channel/transpose/drummap 等对 Score 映射尚未定义。

### 1.7 其它潜在问题（举一反三）
- 字符串与临时对象过多，缺少池化/轻量容器策略。
- 多处状态继承逻辑分散，缺少统一的 VoiceState/MeasureState。
- 测试与回写验证不足，缺少“语义等价写回”基准。


## 2. 重构目标（原则驱动）

- **KISS/YAGNI**：只实现 ABC 标准必需结构；减少字段、避免猜测性扩展。
- **DRY**：统一符号解析与长度解析逻辑；不重复 parse_xxx。
- **SOLID**：解析(AST) / 语义(Document) / 渲染(Renderer) 分层解耦。
- **性能优先**：结构轻量、布局紧凑、最少分配、序列友好。
- **等价回写**：保证 `parse -> render` 语义等价（音高/时值/符号等价）。

## 2.1 Symusic Score <-> ABC 互转目标

**ABC -> Score（解析侧）**
- 解析为 `Document` 后生成 `Score`，至少覆盖：
  - 音高/时值/速度（Note + Rest + Chord）
  - 拍号/速度/调号（M/Q/K）
  - 歌词行（w:/W:）
  - 轨道/声部名称（V: name/sname）
- Tie 合并：`-` 跨小节的同音必须合并为单个 `Score` note。
- Tuplet/BrokenRhythm/Grace：语义层先归一化为实际时值再写入 Score。
- microtone/temperament：如果 Score 无法表示，保留在 `Document` 且在 Score 转换中记录为警告（或映射为 PitchBend，待定）。

**Score -> ABC（渲染侧）**
- 必须输出可被 `parse_document()` 再读回的 ABC（等价回写）。
- 头部字段最小完备：`X/T/M/L/Q/K`，并为每个 track 生成 `V:`。
- program/is_drum/channel：通过 `%%MIDI program/channel` 输出（与 VoiceProperties 对应）。
- Unit length 自动推断（基于 note duration 的 gcd），保证输出简洁。

**双向等价定义**
- 以 **Score 的音高/时值/tempo** 为等价判断；非 Score 可表示的信息保留在 `Document` 中。


## 3. 新架构设计（提案）

### 3.1 文件组织（清晰语义）

```
include/miniabc
├── miniabc.h           # 公共 API (Document, parse/render)
├── ast.h               # 纯语法 AST
├── fraction.hpp        # Fraction 及相关算法
├── error.hpp           # Diagnostic + SourceLocation
├── types.hpp           # enum/轻量结构体（accidental、note letter、bar type 等）
└── container.hpp       # 容器别名（当前 std::vector，后期可切换 svector）

src
├── parse.cpp           # Lexy 语法定义 + AST 产出
├── semantic.cpp        # AST -> Document（符号优先）
├── render.cpp          # Document -> String
└── common.cpp/h         # 纯内部工具（可逐步消灭）
```

> `support.h` 将被移除；基础类型明确拆分为 `fraction.hpp`/`error.hpp`。


### 3.2 数据模型（Symbolic-First + 轻量）

#### 核心原则
- **语义字段枚举化**：accidental、barline、note letter、tie、grace 等统一 enum（不做 bit 级别控制）。
- **减少字符串**：仅保留必要文本（装饰音、指令、宏、歌词、和弦符号）；其余使用 enum 或小整型。
- **布局优化**：合理排序字段，避免 `string/vector` 与小字段混排造成 padding。
- **容器抽象**：当前保持 `std::vector`，后期可替换为 `svector`。

#### NoteEvent 设计方向（示例，最终以实现为准）
```
struct NoteEvent {
    NoteLetter letter;                 // enum: C..B
    Accidental accidental;             // enum: Natural/Sharp/Flat/Double...
    std::optional<Microtone> micro;    // 微分音（可选）
    int octave;                        // 绝对八度
    int slur_start;                    // '(' 计数
    int slur_end;                      // ')' 计数
    bool tie;
    bool grace;
    Fraction duration_factor;          // 原始时值倍率
    std::vector<std::string> decorations;
    std::vector<std::string> chord_symbols;
    std::vector<Annotation> annotations;
};
```

> MIDI pitch/实际时值作为 **派生视图或工具函数** 输出，避免存入 NoteEvent。

#### 其它核心结构的同步收敛（必须一起调整）
- **RestEvent**：`symbol` 改 enum；`invisible/spacer/measure` 归入 `RestType`，并提供 `measure_count` 支持 `Z`。
- **BarEvent**：保留 `repeat_start/repeat_end/final` 等 bool（更直观），同时保留 `Barline` enum。
- **ChordEvent**：chord symbol 存在于 ChordEvent（不复制到 chord 内部每个 NoteEvent）。
- **Voice**：`lyrics` 仍保留 `vector<string>`，后期可引入解析后的 token。
- **Header/Tempo/Key/Meter**：字段类型收敛为小整型/枚举，避免字符串依赖。

### 3.2.1 AST 结构（含嵌套关系）

> AST 仅表示**语法结构**与**原始符号**，保留文本与位置，不进行语义归一化。

```
ast::File
└── items[] : FileItem
    ├── FreeText { text, loc }                   // 文件级自由文本/注释块
    └── Tune
        └── statements[] : Statement
            └── payload : variant
                ├── Field        { id, value, loc }
                ├── FieldContinuation { value, loc }     // "+:"
                ├── Directive    { text, loc }           // "%%..." 原样保留
                ├── Lyric        { block, text, loc }     // w:/W:
                ├── SymbolLine   { block, text, loc }     // s:/S:
                └── MusicLine    { items[], loc }
                    └── MusicItem : variant
                        ├── Note         { accidental, microtone, letter, is_lower, octave_shift,
                        │                 length, tie, grace, slur_start, slur_end,
                        │                 decorations[], annotations[], chord_symbols[], loc }
                        ├── Rest         { type, length, measure_count, loc }
                        ├── Chord        { notes[], length, grace, slur_start, slur_end,
                        │                 decorations[], annotations[], chord_symbols[], loc }
                        ├── GraceGroup   { notes[], loc }
                        ├── Bar          { type, repeat_start, repeat_end, final, endings[], loc }
                        ├── Tuplet       { p, q, r, loc }
                        ├── InlineField  { field }       // [K:..] 等
                        ├── VoiceOverlay { kind, loc }    // "&" / "(&" / "&)"
                        ├── Spacing      { length, loc }  // 'y'
                        ├── LineBreak    { kind, count, loc } // '$' / '!' / '*' / '\\'
                        ├── BeamBreak    { loc }          // 显式断beam/空白
                        ├── BrokenRhythm { greater, loc } // > / <
                        └── Continuation { loc }          // "\" 行续接
```

> 多声部在 AST 中以“线性事件序列 + 语义分配”的方式表示：  
> - `Field{id="V"}` 与 `InlineField{V:..}` 作为 **Voice 切换标记** 保留在序列中  
> - `VoiceOverlay` 用于 `&` 叠声部边界  
> 语义层根据这些标记将事件分配到不同 Voice。

### 3.2.2 最终数据结构（含嵌套关系）

> Document 是**语义模型**，保持符号信息但允许等价回写；MIDI 由派生视图计算。

```
TuneBook
├── tunes[] : Document
├── free_text[] : TextBlock
└── file_directives[]    // 文件级 %% 指令或无法归类的行

Document
├── Header
│   ├── reference_number (X)     // 曲目编号 / tune id
│   ├── titles[] / primary_title (T)
│   ├── meter (M)
│   ├── unit_note_length (L)
│   ├── tempo (Q)
│   ├── key (K)
│   ├── instructions[]   // I:, m:, U:, %%directive 结构化
│   └── extra_fields[]   // 其它信息字段（C/O/A/Z/N/G/H/R/P/B/D/F/S...）
├── voices[] : Voice
│   ├── id / name
│   ├── properties        // clef/transpose/octave/stafflines 等
│   ├── elements[] : Element
│   │   ├── NoteEvent
│   │   ├── RestEvent
│   │   ├── ChordEvent (notes[])
│   │   ├── BarEvent (barline / repeat / endings)
│   │   ├── TupletEvent / BrokenRhythmEvent
│   │   ├── BeamBreakEvent
│   │   ├── InlineFieldEvent
│   │   └── PartEvent (P: in body)
│   ├── lyrics[]          // 原始歌词行或解析后的 token（后期）
│   └── symbol_lines[]    // s:/S: 符号行
└── directives[]          // 全局或未结构化的指令（保底承载）
```

> 说明：最终结构会包含必要的可扩展占位（如 instructions/extra_fields），保证标准字段可承载。

### 3.2.3 AST Struct 草案（供评审）

> 仅为结构草案，字段与命名可调整；关键是嵌套关系与职责划分。

```cpp
namespace miniabc::ast {

using VecStr = std::vector<std::string>;
// AST 复用 types.hpp 中的枚举（NoteLetter/Accidental/RestType/Barline 等）。

struct LengthSpec {
    bool     present = false;
    Fraction value{1, 1};
    // present=false 表示无长度后缀；present=true 表示已解析的长度倍率
};

struct Microtone {
    int dir = 0;               // +1 上行, -1 下行
    Fraction amount{0, 1};     // e.g. 1/2, 1/4
};

struct Field {
    std::string id;
    std::string value;
    SourceLocation loc;
};

struct FieldContinuation {
    std::string value; // "+:" 续接内容
    SourceLocation loc;
};

struct Directive {
    std::string text;
    SourceLocation loc;
};

struct Lyric {
    bool block = false; // W: block lyrics
    std::string text;
    SourceLocation loc;
};

struct SymbolLine {
    bool block = false; // S: block symbols
    std::string text;
    SourceLocation loc;
};

struct Annotation {
    AnnotationPos pos = AnnotationPos::Above;
    std::string text;
};

struct Note {
    NoteLetter letter = NoteLetter::C;
    Accidental accidental = Accidental::None;
    std::optional<Microtone> microtone;
    int octave_shift = 0; // ',' / '''
    int slur_start = 0;
    int slur_end = 0;
    bool is_lower = false;
    bool tie = false;
    bool grace = false;
    LengthSpec length;
    VecStr decorations;
    std::vector<Annotation> annotations;
    VecStr chord_symbols;
    SourceLocation loc;
};

struct Rest {
    RestType type = RestType::Visible; // z/x/s/Z
    int measure_count = 0;             // Z2, Z4...
    LengthSpec length;
    SourceLocation loc;
};

struct Chord {
    int slur_start = 0;
    int slur_end = 0;
    bool grace = false;
    LengthSpec length;
    std::vector<Note> notes;
    VecStr decorations;
    std::vector<Annotation> annotations;
    VecStr chord_symbols;
    SourceLocation loc;
};

struct GraceGroup {
    std::vector<Note> notes;
    SourceLocation loc;
};

struct Bar {
    Barline type = Barline::Single;
    bool repeat_start = false;
    bool repeat_end = false;
    bool final = false;
    std::vector<int> endings; // [1 [2 [1,2]
    SourceLocation loc;
};

struct Tuplet {
    int p = 0;
    int q = 0;
    int r = 0;
    SourceLocation loc;
};

struct InlineField {
    Field field;
};

struct VoiceOverlay {
    OverlayKind kind = OverlayKind::Split;
    SourceLocation loc; // "&", "(&", "&)"
};

struct Spacing {
    LengthSpec length; // y1/2 等
    SourceLocation loc;
};

struct LineBreak {
    LineBreakKind kind = LineBreakKind::Forced; // '$', '!', '*', '\\'
    int count = 0;                               // '*' 个数
    SourceLocation loc;
};

struct BeamBreak {
    SourceLocation loc; // 显式断beam/空白
};

struct BrokenRhythm {
    bool greater = true; // > / <
    SourceLocation loc;
};

struct Continuation {
    SourceLocation loc; // "\"
};

using MusicItem = std::variant<
    Note,
    Rest,
    Chord,
    Bar,
    Tuplet,
    InlineField,
    GraceGroup,
    VoiceOverlay,
    Spacing,
    LineBreak,
    BeamBreak,
    BrokenRhythm,
    Continuation>;

struct MusicLine {
    std::vector<MusicItem> items;
    SourceLocation loc;
};

struct Statement {
    std::variant<Field, FieldContinuation, Directive, Lyric, SymbolLine, MusicLine> payload;
};

struct Tune {
    std::vector<Statement> statements;
};

struct FreeText {
    std::string text;
    SourceLocation loc;
};

using FileItem = std::variant<FreeText, Tune>;

struct File {
    std::vector<FileItem> items;
};

} // namespace miniabc::ast
```

### 3.2.4 最终 Struct 草案（供评审）

```cpp
namespace miniabc {

// types.hpp
enum class NoteLetter { C, D, E, F, G, A, B };
enum class Accidental { None, Natural, Sharp, Flat, DoubleSharp, DoubleFlat };
enum class RestType { Visible, Invisible, Spacer, Measure };
enum class Barline {
    Single,
    Double,
    Dotted,
    ThickThin,
    ThinThick,
    Final,
    RepeatBoth,
    DoubleRepeat,
    Unknown
};
enum class ClefType { None, Treble, Bass, Alto, Tenor, Percussion, Tab, Unknown };
enum class MeterType { Simple, Common, Cut, Free, Complex };
enum class OverlayKind { Split, ExtendedStart, ExtendedEnd };
enum class LineBreakKind { Continuation, Forced, Score, Star };
enum class DirectiveScope { File, Header, Body };
enum class AnnotationPos { Above, Below, Left, Right, Center };
enum class MacroKind { Static, Transposing };

// error.hpp / fraction.hpp
struct SourceLocation { std::size_t line = 0; std::size_t column = 0; };
struct Diagnostic { std::size_t line = 0; std::size_t column = 0; std::string message; };
struct Fraction { std::int64_t num = 0; std::int64_t den = 1; };

struct Microtone {
    int dir = 0;               // +1 上行, -1 下行
    Fraction amount{0, 1};     // e.g. 1/2, 1/4
};

struct Pitch {
    NoteLetter letter = NoteLetter::C;
    Accidental accidental = Accidental::None;
    std::optional<Microtone> microtone;
    int octave = 4;
};

struct ClefSpec {
    ClefType type = ClefType::None;
    int line = 0;            // e.g. treble line 2
    int octave_shift = 0;    // +8 / -8
};

// 高层容器
struct TextBlock {
    std::string text;
    SourceLocation loc;
};

struct TuneBook {
    std::string abc_version;              // %%abc-version
    std::vector<Document> tunes;
    std::vector<TextBlock> free_text;
    std::vector<Directive> file_directives; // 文件级 %%
};

// 头部与字段
struct InfoField {
    std::string id;
    std::string value;
};

struct Directive {
    DirectiveScope scope = DirectiveScope::Body;
    std::string name;   // e.g. "abc-include", "MIDI", "temperament"
    std::string value;  // raw args
};

struct Instruction {
    std::string key;   // I:/%% 指令名
    std::string value;
};

struct MacroDef {
    MacroKind kind;
    std::string lhs;
    std::string rhs;
};

struct UserSymbolDef {
    char symbol = 0;   // H-W, h-w, ~
    std::string replacement;
    char container = '!'; // !...! or "..."
};

struct Meter {
    MeterType type = MeterType::Simple;
    int numerator = 4;
    int denominator = 4;
    std::vector<int> grouping; // e.g. (2+3+2)/8 -> {2,3,2}
};

struct TempoMark {
    Fraction beat{1,4};
    double qpm = 120.0;
    bool relative = false;
    std::string pre_text;   // "Allegro"
    std::string post_text;  // "rit."
};

struct KeyAccidental {
    NoteLetter letter = NoteLetter::C;
    Accidental accidental = Accidental::None;
    std::optional<Microtone> microtone;
};

struct KeySignature {
    int accidentals = 0;
    bool is_minor = false;
    bool explicit_mode = false;               // K: ... exp
    std::vector<KeyAccidental> modifiers;     // K: ... ^f _b ...
};

struct Header {
    int reference_number = 1; // X:
    std::vector<std::string> titles;
    Meter meter{};
    Fraction unit_note_length{1,8};
    TempoMark tempo{};
    KeySignature key{};
    std::vector<Instruction> instructions; // I: (结构化)
    std::vector<Directive> directives;     // %% 指令（header scope）
    std::vector<MacroDef> macros;           // m:
    std::vector<UserSymbolDef> user_symbols; // U:
    std::vector<InfoField> extra_fields;
    std::vector<std::string> parts;         // P: (header)
};

struct Annotation {
    AnnotationPos pos = AnnotationPos::Above;
    std::string text;
};

struct NoteEvent {
    NoteLetter letter = NoteLetter::C;
    Accidental accidental = Accidental::None;
    std::optional<Microtone> microtone;
    int slur_start = 0;
    int slur_end = 0;
    int octave = 4; // absolute octave for rendering
    bool tie = false;
    bool grace = false;
    Fraction duration_factor{1,1};
    std::vector<std::string> decorations;
    std::vector<std::string> chord_symbols;
    std::vector<Annotation> annotations;
};

struct RestEvent {
    RestType type = RestType::Visible;
    int measure_count = 0; // Z2, Z4...
    Fraction duration{0,1};
};

struct BarEvent {
    Barline type = Barline::Single;
    bool repeat_start = false;
    bool repeat_end = false;
    bool final = false;
    std::vector<int> endings; // 1/2/... for variant endings
};

struct ChordEvent {
    bool grace = false;
    Fraction duration_factor{1,1};
    int slur_start = 0;
    int slur_end = 0;
    std::vector<NoteEvent> notes;
    std::vector<std::string> decorations;
    std::vector<std::string> chord_symbols;
    std::vector<Annotation> annotations;
};

struct TupletEvent {
    int p = 0;
    int q = 0;
    int r = 0;
};

struct BrokenRhythmEvent {
    bool greater = true; // > / <
};

struct SpacingEvent {
    Fraction duration{0,1}; // y1/2 等
};

struct LineBreakEvent {
    LineBreakKind kind = LineBreakKind::Forced; // '$', '!', '*', '\\'
    int count = 0;                               // '*' 个数
};

struct OverlayEvent {
    OverlayKind kind = OverlayKind::Split;       // & / (& / &)
};

struct BeamBreakEvent {};

struct PartEvent {
    std::string label; // P: in tune body
};

struct InlineFieldEvent {
    InfoField field; // 在 music line 中的 [K:..] / [M:..] 等
};

using Element = std::variant<
    NoteEvent,
    RestEvent,
    ChordEvent,
    BarEvent,
    TupletEvent,
    BrokenRhythmEvent,
    SpacingEvent,
    LineBreakEvent,
    OverlayEvent,
    BeamBreakEvent,
    PartEvent,
    InlineFieldEvent>;

struct VoiceProperties {
    int transpose_semitones = 0;
    int octave = 0;
    int staff_lines = 5;
    ClefSpec clef; // enum + line + octave shift
    std::string name;
    int program = -1;     // %%MIDI program
    int channel = -1;     // %%MIDI channel
    bool is_drum = false; // channel==10 或显式标记
    std::string short_name;       // sname=
    std::optional<Pitch> middle;  // middle=
    std::vector<InfoField> extra; // other=...
};

struct VoiceLayer {
    std::vector<Element> elements;
};

struct Voice {
    std::string id;
    VoiceProperties props;
    std::vector<VoiceLayer> layers; // overlay 时包含多个 layer
    std::vector<std::string> lyrics;
    std::vector<std::string> symbol_lines;
};

struct Document {
    Header header;
    std::vector<Voice> voices;
    std::vector<Directive> directives; // body scope %%
};

} // namespace miniabc
```

### 3.2.5 结构覆盖评审（对照缺口清单）
- **多曲目文件**：`TuneBook` + `ast::File` 支持 tunebook/自由文本/嵌入片段。
- **字段续接与行续接**：`FieldContinuation` + `Continuation` 已在 AST 结构中预留。
- **多声部与 overlay**：`Voice.layers` + `VoiceOverlay` token 支持 `V:` 与 `&` 叠声部。
- **变体结尾**：`BarEvent.endings` + repeat/final bool 覆盖 `[1`/`[2` 等。
- **Slur/Annotation/Decoration**：`slur_start/slur_end` + `Annotation` + `decorations` 支持连线与注释。
- **Rest 类型**：`RestType` + `measure_count` 支持 `z/x/s/Z`。
- **Key 扩展**：`KeySignature.modifiers/explicit_mode` 支持 `K:` 的修正/显式调号。
- **复杂拍号/Tempo 文本**：`Meter.grouping` + `TempoMark.pre_text/post_text` 预留结构。
- **微分音/调律**：`Microtone` + `KeyAccidental.microtone` + `Directive` 支持 `%%temperament` 承载。
- **排版/行控制**：`LineBreakEvent`/`SpacingEvent` 预留 `$`/`!`/`*`/`y` 等语义。
- **版本/包含**：`TuneBook.abc_version` + `Directive` 支持 `%%abc-version`/`%%abc-include`。
- **P: 分段**：`PartEvent` 支持 body-level 分段标记。

**仍需补齐或进一步细化的结构点：**
- **%%score 与 voice grouping**：需要引入 `ScoreGroup` 或结构化 directive。
- **Accidental directives**（`%%propagate-accidentals` 等）：需要结构化配置而非纯字符串。
- **Symbol lines 语义**：`symbol_lines` 已承载文本，但对齐规则仍需语义层实现。
- **Beam/spacing**：已预留 `BeamBreakEvent/SpacingEvent`，需在解析中明确何时生成。
- **Tuplet/BrokenRhythm**：已保留事件类型，但语义与回写策略需细化。
- **Include 执行语义**：`%%abc-include` 仅建模，尚需预处理/递归控制策略。
- **%%MIDI/播放语义**：仅结构承载，未定义对 Score 的映射策略。

### 3.2.6 未决点的基础处理方案（本阶段必须明确）

**1) %%score / voice grouping**
- **结构**：新增 `ScoreGroup`（结构化 directive），挂载到 `Document`。
- **语义**：仅用于输出顺序与分组，不影响音高/时值。
- **渲染**：若存在 `ScoreGroup`，在 header 输出 `%%score`；否则忽略。

**2) Accidental directives**
- **结构**：新增 `AccidentalPolicy`（`propagate`/`writeout`）挂在 `Document`。
- **语义**：仅影响渲染（是否写回临时升降号），不改变 `Score`。
- **渲染默认**：`propagate=octave`（与标准一致），`writeout=added`。

**3) %%MIDI / playback**
- **结构**：把 `%%MIDI` 解析为 `Directive{scope, name, value}`，保留原始参数。
- **映射到 Score**：仅处理 `program/channel`，写入 `VoiceProperties`。
- **未映射内容**（drummap/transpose 等）保留在 directives 中，渲染时原样回写。

**4) %%abc-include**
- **预处理顺序**：先展开 include，再进入 Lexy 解析。
- **递归保护**：维护 include stack，检测重复与过深嵌套（最大深度可配置）。
- **路径策略**：相对路径相对于包含文件；不允许越界访问（默认同目录）。

**5) U:/m: 展开策略**
- **默认策略**：保持结构化定义，不在解析阶段展开。
- **渲染策略**：按原输入顺序输出；提供选项 `expand_macros` 决定是否展开。
- **优先级**：若展开，先处理 `m:`（macro），再处理 `U:`（符号替换），与标准一致。

**6) Beam/spacing/linebreak**
- **解析**：`$`/`!`/`*`/`\\` -> `LineBreakEvent`，`y` -> `SpacingEvent`。
- **语义**：不影响 Score，只影响 render/formatting。


### 3.3 Lexy 深度集成
- 所有字段都在 Lexy 中解析为具体结构（`K/M/Q/L`、长度、连音、装饰音）。
- 错误定位精确到字符（包括字段内部）。
- 任何二次解析函数必须删除或合并到语法层。

### 3.3.1 Lexy 解析策略（细化）
- **分层语法**：`Document -> Statement -> (Field | Directive | Lyric | MusicLine)`；音乐行再拆成 `MusicItem`。
- **字段解析全覆盖**：`K/M/Q/L/V/I/m/U` 等直接输出结构体，不再捕获字符串后二次解析。
- **复杂字段语法**：  
  - `K:` 解析 mode + 变更调号 (`exp` / 修改 accidentals / clef 参数)  
  - `M:` 支持复杂拍号 `(2+3+2)/8`  
  - `Q:` 支持多种速度格式（含 `beat=tempo`、文字描述）
- **基础音符语义下沉**：accidental 直接解析为 enum；microtone 解析为 `Microtone`；octave 修饰累计为 `octave_shift`；长度后缀解析为 `LengthSpec`（不保留原始字符串）。
- **行续接**：  
  - `+:` 作为 FieldContinuation 语法直接解析并在 AST 中串联  
  - `\` 作为 MusicLine Continuation token；语义层做跨行合并
- **符号与文本行**：支持 `s:/S:` 符号行（SymbolLine），与歌词行并行解析。
- **Voice/Overlay**：  
  - `V:` 与 `[V:...]` 解析成 `Field/InlineField`，语义层切换 voice  
  - `&` 解析为 `VoiceOverlay` token；`(&`/`&)` 解析为 extended overlay
- **排版/行控制**：解析 `$`/`!`/`*`/`y` 为 `LineBreak/Spacing`。
- **P: 分段**：`P:` 在 header 存入 `Header.parts`，在 body 转为 `PartEvent`。
- **兼容旧式和弦**：支持 `+CE+` 旧约定为 chord_token（compat 分支）。
- **多曲目文件**：  
  - Lexy 产出 `ast::File`（包含 Tune 和 FreeText）  
  - 语义层以 `X:` 为起点切分 tune，生成 `TuneBook.tunes[]`
- **装饰音与符号重映射**：`U:`/`m:` 解析为结构化定义，语义层决定展开时机
- **连线与注释**：解析 slur `(` `)` 为 `slur_start/slur_end`；解析 `^"text"` 等为 `Annotation`。
- **休止类型**：解析 `z/x/s/Z` 到 `RestType`，`Z` 同时解析 `measure_count`。
- **版本/包含指令**：解析 `%%abc-version`/`%%abc-include` 为 `Directive`（包含实际展开由预处理处理）。
- **错误报告**：Lexy 使用 `report_error` 对字符级错误定位，避免 “field-level only” 报错。

### 3.3.2 写入/回写策略（Renderer）
- **Renderer 不使用 Lexy**：Lexy 仅用于解析；写回由 `render.cpp` 基于 `Document` 生成。
- **等价回写**：输出允许不同但 **语义等价**（音高/时值/符号一致）。
- **顺序规则**：严格遵循标准的构造顺序（装饰/注释/和弦符号/accidental/音高/八度/时值）。
- **指令与宏**：  
  - `m:`/`U:` 若保留，按头部输出；若在语义层展开，则写回展开后的音乐码。  
  - 未结构化指令保存在 `directives[]`，按输入顺序输出。

### 3.3.3 Lexy 规则设计蓝图（基于 learn 文档审视后修订）

> 依据 lexy “非声明式/显式分支”原则，规则必须明确分支条件与顺序，避免隐式回溯。

**总体结构**
- `namespace grammar { namespace dsl = lexy::dsl; }`
- `Document` 入口使用 `dsl::terminator(dsl::eof).list(dsl::p<statement>)`，确保 EOF 约束。
- 对需要分支的列表/循环使用 `dsl::list` + `dsl::peek` 条件，避免 `while_` 无法继续匹配的问题。

**Token 级别（token_production）**
- `accidental_token`：解析 `__/_/=/^/^^` 并直接映射 `Accidental` enum。
- `microtone_token`：解析 `^/`、`_/` 或分数字符串为 `Microtone`。
- `note_letter`：解析 `A-G/a-g` 并输出 `NoteLetter` + `is_lower`。
- `octave_mod`：直接累计 `'`/`,` 为 `octave_shift`（不保存字符串）。
- `length_suffix`：解析为 `LengthSpec`（`present` + `Fraction`），用 `dsl::digits`/`dsl::integer` 组合。
- `barline_token`：使用 `dsl::literal_set` 匹配多字符 barline（长的在前或交由 literal_set）。
- `decoration_token`：解析 `!xxx!` 或单字符装饰，输出 `std::string` 或 enum（后期可替换）。
- `annotation_token`：解析 `^"text"`/`_"text"`/`>"text"` 等，输出 `Annotation{pos,text}`。
- `inline_field_head`：解析 `[` + `Uppercase` + `:` 作为分支条件。
- `linebreak_token`：解析 `$`/`!`/`*`/`\\`，输出 `LineBreakEvent`。
- `spacing_token`：解析 `y` + length 为 `SpacingEvent`。

**分支与选择（避免错误回溯）**
- **Statement** 使用明确分支条件：  
  `directive_branch = dsl::peek("%%") >> directive_line`  
  `field_branch = dsl::peek(identifier + ':') >> field_line`  
  `lyric_branch = dsl::peek('w'|'W') >> lyric_line`  
  `symbol_branch = dsl::peek('s'|'S') >> symbol_line`  
  `cont_branch = dsl::peek("+:") >> field_continuation`
- **MusicItem** 必须指定分支条件，顺序为：  
  `inline_field` (peek `[A-Z]:`) → `repeat/barline` (peek `|`/`:`/`[|`) → `chord` (peek `[`)  
  → `tuplet` (peek `(` + digit) → `grace` (peek `{`) → `rest` (peek `z/x/s/Z`)  
  → `broken` (peek `>`/`<`) → `overlay` (peek `&`) → `beam_break` (explicit rule)  
  → `else_ >> note`
- `dsl::else_` 只用于最后分支，确保 “先检查明确 token，再回退到 note”。

**值构造（callbacks）**
- 使用 `lexy::callback`/`lexy::construct` 直接构造 AST structs，避免后置字符串解析。
- `dsl::capture` 仅用于字段文本/歌词/指令等必须保留的文本。
- `ListBuilder`/`sink` 组合用于 `decorations[]`/`annotations[]`/`notes[]` 等列表。

**空白与注释**
- `whitespace` 仅处理 `blank`（空格/制表），行尾用 `line_end` 规则处理注释与 `newline`。
- 避免全局吞行空白导致分支失效；在 music 解析中显式处理可跳过空白。

**错误定位**
- 每个产生 AST 的规则使用 `dsl::position` 记录 `SourceLocation`。
- 解析阶段使用 `lexy::parse` + 自定义错误回调，将 Lexy 错误转换为 `Diagnostic`。

### 3.3.4 Lexy 示例与必读文档（实现人员必须查看）

**必须阅读的文档（本仓库路径）：**
- `3rdparty/miniabc/3rdparty/lexy/docs/content/learn/warmup.adoc`
- `3rdparty/miniabc/3rdparty/lexy/docs/content/learn/branching.adoc`
- `3rdparty/miniabc/3rdparty/lexy/docs/content/learn/walkthrough.adoc`

**示例 1：Field 行解析（`X:`/`T:`/`K:` 等）**
```cpp
struct identifier : lexy::token_production {
    static constexpr auto rule  = dsl::identifier(dsl::ascii::alpha);
    static constexpr auto value = lexy::as_string<std::string>;
};

struct field_line {
    static constexpr auto rule = [] {
        auto id    = dsl::p<identifier>;
        auto value = dsl::capture(dsl::token(dsl::while_(dsl::peek_not(dsl::newline) >> dsl::any)));
        return dsl::position + id + dsl::lit_c<':'> + value + dsl::p<line_end>;
    }();

    static constexpr auto value = lexy::callback<ast::Field>([](auto pos, std::string id, auto val) {
        return ast::Field{std::move(id), std::string(val.begin(), val.end()), make_location(pos)};
    });
};
```

**示例 2：长度后缀解析为 `LengthSpec`（避免 length_text）**
```cpp
struct length_suffix : lexy::token_production {
    static constexpr auto rule = [] {
        auto digits = dsl::integer<int>(dsl::digits<>);
        auto slash  = dsl::lit_c<'/'>;
        auto frac   = digits + slash + dsl::opt(dsl::p<digits>);
        return dsl::capture(dsl::token(frac | slash | digits));
    }();
    static constexpr auto value = lexy::callback<ast::LengthSpec>([](auto lex) {
        return parse_length_spec(lex); // 将 lexeme 转成 LengthSpec{present, Fraction}
    });
};
```

**示例 3：barline 使用 `literal_set` 避免分支错误**
```cpp
struct barline_token : lexy::token_production {
    static constexpr auto rule = dsl::literal_set<":|]", "::", ":|", "|:", "||", "|]", "[|", "|">;
    static constexpr auto value = lexy::as_string<std::string>;
};
```

**示例 4：MusicItem 分支顺序（显式 `peek` + `else_`）**
```cpp
struct music_item {
    static constexpr auto rule = [] {
        auto inline_field = dsl::peek(dsl::lit_c<'['> + dsl::p<identifier> + dsl::lit_c<':'>)
                            >> dsl::p<inline_field_token>;
        auto barline = dsl::peek(dsl::lit_c<'|'> | dsl::lit_c<':'>) >> dsl::p<barline_token>;
        auto chord   = dsl::peek(dsl::lit_c<'['>) >> dsl::p<chord_token>;
        auto tuplet  = dsl::peek(dsl::lit_c<'('> + dsl::digit<>) >> dsl::p<tuplet_token>;
        auto grace   = dsl::peek(dsl::lit_c<'{'>) >> dsl::p<grace_token>;
        auto rest    = dsl::peek(dsl::lit_c<'z'> | dsl::lit_c<'x'> | dsl::lit_c<'s'> | dsl::lit_c<'Z'>)
                       >> dsl::p<rest_token>;
        auto note    = dsl::p<note_token>;
        return inline_field | barline | chord | tuplet | grace | rest | dsl::else_ >> note;
    }();
};
```

**示例 5：注释（Annotations）解析**
```cpp
struct annotation_token : lexy::token_production {
    static constexpr auto rule = [] {
        auto pos = dsl::lit_c<'^'> | dsl::lit_c<'_'> | dsl::lit_c<'<'> | dsl::lit_c<'>'>;
        auto txt = dsl::quoted(-dsl::ascii::control);
        return dsl::capture(pos) + txt;
    }();
    static constexpr auto value = lexy::callback<ast::Annotation>([](auto p, std::string text) {
        return make_annotation(p.begin()[0], std::move(text));
    });
};
```


### 3.4 语义层（State Engine）
- 引入 `ScoreState`/`VoiceState`/`MeasureState`，集中管理：
  - KeySignature 继承、拍号、单位音长、临时升降号
  - tuplet/broken rhythm 影响
  - grace note/overlay/chord 影响
- 输出 `Document` 保留符号信息，不做不可逆降维。


## 4. 实施计划（分阶段 + 可回滚）

### Phase 0：基线整理（已完成存档提交）
- 说明：已有一次存档提交作为基线。

### Phase 1：基础类型与文件结构重整
- 删除 `support.h`，拆分为 `fraction.hpp`、`error.hpp`。
- 清理 `fraction.h`/`error.h` 重复定义。
- 引入 `types.hpp`（accidental/note/bar/tie/tuplet 等枚举）。
- 引入 `container.hpp`（目前仍是 std::vector）。

### Phase 1.5：预处理层（abcmidi 兼容）
- 处理 `%%abc-version`（记录到 `TuneBook.abc_version`）。
- 处理 `%%abc-include`（文件展开 + 递归/重复保护）。
- 解析 `%%` directives 至 `Directive`（按 file/header/body scope 分类）。
- 为 `m:`/`U:` 提供展开开关（expand 或保留并在 render 中输出）。

### Phase 2：数据模型轻量化（不做 bit 级别控制）
- 重写 `NoteEvent/ChordEvent/RestEvent/BarEvent/Voice`：
  - accidental/letter/tie/grace/barline 等枚举化
  - 音高派生，不再保存 `midi_pitch`
  - 保留 `Fraction` 但避免字符串与 vector 混排
- 调整字段顺序与尺寸，减少 padding。
- 清理 `has_symbolic_info`，改为语义层唯一来源。

### Phase 3：语义层重构
- 语义层实现统一的 `State Engine`。
- 所有临时状态（measure accidentals/tuplet 等）集中管理。
- 语义层输出仅依赖 AST + 状态机，不依赖渲染。

### Phase 4：Lexy 深度解析
- 把 `parse_key/parse_meter/parse_tempo/parse_length` 下沉到 Lexy DSL。
- 删除所有字符串二次解析函数。
- 错误诊断包含行列与局部 token。

### Phase 5：等价回写与验证
- `render` 基于符号信息回写，允许输出在语义等价范围内变化。
- 构建最小验证集：`parse -> render -> parse` 的语义一致性。
- 以 `abc_standard.md` 为标准覆盖清单。

### Phase 6：标准覆盖扩展
- 宏 `m:`、用户符号 `U:`、多重结尾 `[1` `[2`。
- 复杂装饰音、叠音、分轨/多声部策略。


## 5. 成功标准（验收）
- `NoteEvent` 等核心对象明显轻量（体积减少、无 string accidental）。
- `support.h` 消失，`fraction.hpp`/`error.hpp` 明确。
- 解析错误定位精确到字段内部。
- `parse -> render` 等价回写通过基准集合。
- 标准覆盖可持续扩展。
