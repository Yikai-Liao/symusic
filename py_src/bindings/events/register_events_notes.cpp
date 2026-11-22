#include "../core/binding_prelude.h"

#include "event_bindings.h"

namespace symusic::py {

void bind_note_family_types(nb::module_& m) {
    bind_event<Note>(
        m,
        "Note",
        "NoteList",
        "Note(time, duration, pitch, velocity=0, *, ttype=Tick)\n\nA musical note.",
        "time"_a,
        "duration"_a,
        "pitch"_a,
        "velocity"_a = 0
    );
    bind_event<KeySignature>(
        m,
        "KeySignature",
        "KeySignatureList",
        "KeySignature(time, key, minor, *, ttype=Tick)\n\nA key signature change.",
        "time"_a,
        "key"_a,
        "minor"_a
    );
    bind_event<TimeSignature>(
        m,
        "TimeSignature",
        "TimeSignatureList",
        "TimeSignature(time, numerator, denominator, *, ttype=Tick)\n\nA time signature change.",
        "time"_a,
        "numerator"_a,
        "denominator"_a
    );
    bind_event<TextMeta>(
        m,
        "TextMeta",
        "TextMetaList",
        "TextMeta(time, text, *, ttype=Tick)\n\nA lyric or marker.",
        "time"_a,
        "text"_a
    );
}

}   // namespace symusic::py
