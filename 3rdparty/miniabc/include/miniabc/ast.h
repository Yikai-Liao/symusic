#pragma once

#include "miniabc/support.h"
#include <string>
#include <vector>
#include <variant>
#include <optional>

namespace miniabc::ast {

struct Field {
    std::string    id;
    std::string    value;
    SourceLocation loc;
};

struct Directive {
    std::string    text;
    SourceLocation loc;
};

struct Lyric {
    bool           block = false;
    std::string    text;
    SourceLocation loc;
};

struct Tuplet {
    int            p = 0;
    int            q = 0;
    int            r = 0;
    SourceLocation loc;
};

struct Note {
    std::string              accidentals;
    char                     letter = 'C';
    std::string              octave_mod;
    std::string              length_text;
    bool                     tie              = false;
    bool                     explicit_natural = false;
    bool                     grace            = false;
    std::vector<std::string> decorations;
    std::vector<std::string> chord_symbols;
    SourceLocation           loc;
};

struct Rest {
    char           symbol = 'z';
    std::string    length_text;
    bool           invisible = false;
    SourceLocation loc;
};

struct Bar {
    std::string    token;
    SourceLocation loc;
};

struct InlineField {
    Field field;
};

struct Chord {
    std::vector<Note> notes;
    std::string       length_text;
    bool              grace = false;
    SourceLocation    loc;
};

struct GraceGroup {
    std::vector<Note> notes;
    SourceLocation    loc;
};

struct BrokenRhythm {
    bool           greater = true;   // '>' when true, '<' when false
    SourceLocation loc;
};

struct Continuation {
    SourceLocation loc;
};

using MusicItem = std::
    variant<Note, Rest, Chord, Bar, Tuplet, InlineField, GraceGroup, BrokenRhythm, Continuation>;

struct MusicLine {
    std::vector<MusicItem> items;
    SourceLocation         loc;
};

struct Statement {
    std::variant<Field, Directive, Lyric, MusicLine> payload;
};

struct Document {
    std::vector<Statement> statements;
};

}   // namespace miniabc::ast
