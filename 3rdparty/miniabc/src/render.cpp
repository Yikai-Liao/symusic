#include "miniabc/miniabc.h"
#include "miniabc/support.h"
#include "common.h"

#include <cmath>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <string>
#include <vector>

namespace miniabc {

using namespace std::string_view_literals;

const char* describe() noexcept { return "miniabc: standalone build helper"; }

std::string fraction_to_text(const Fraction frac) {
    auto norm = normalize(frac);
    if (norm.den == 1) { return std::to_string(norm.num); }
    return std::to_string(norm.num) + "/" + std::to_string(norm.den);
}

std::string render_meter(const Meter& meter) {
    if (meter.numerator == 4 && meter.denominator == 4) { return "C"; }
    if (meter.numerator == 2 && meter.denominator == 2) { return "C|"; }
    if (meter.denominator == 0) { return "C"; }
    return std::to_string(meter.numerator) + "/" + std::to_string(meter.denominator);
}

std::string render_tempo(const TempoMark& tempo) {
    std::ostringstream oss;
    oss << fraction_to_text(tempo.beat) << "=" << static_cast<int>(std::lround(tempo.qpm));
    return oss.str();
}

std::string render_key(const KeySignature& key) {
    static const std::unordered_map<int, std::string> major
        = {{0, "C"},
           {1, "G"},
           {2, "D"},
           {3, "A"},
           {4, "E"},
           {5, "B"},
           {6, "F#"},
           {7, "C#"},
           {-1, "F"},
           {-2, "Bb"},
           {-3, "Eb"},
           {-4, "Ab"},
           {-5, "Db"},
           {-6, "Gb"},
           {-7, "Cb"}};
    static const std::unordered_map<int, std::string> minor
        = {{0, "A"},
           {1, "E"},
           {2, "B"},
           {3, "F#"},
           {4, "C#"},
           {5, "G#"},
           {6, "D#"},
           {7, "A#"},
           {-1, "D"},
           {-2, "G"},
           {-3, "C"},
           {-4, "F"},
           {-5, "Bb"},
           {-6, "Eb"},
           {-7, "Ab"}};
    const auto& map = key.is_minor ? minor : major;
    auto        it  = map.find(key.accidentals);
    if (it != map.end()) { return it->second + (key.is_minor ? "m" : ""); }
    return key.is_minor ? "Am" : "C";
}

std::string render_length_suffix(const Fraction length, const Fraction unit) {
    auto ratio = div(length, unit);
    if (ratio.num == 0) { return "0"; }
    if (ratio.den == 1) {
        if (ratio.num == 1) { return {}; }
        return std::to_string(ratio.num);
    }
    if (ratio.num == 1) { return "/" + std::to_string(ratio.den); }
    return std::to_string(ratio.num) + "/" + std::to_string(ratio.den);
}

int floor_div(const int value, const int divisor) {
    if (divisor == 0) { return 0; }
    int quot = value / divisor;
    int rem  = value % divisor;
    if ((rem != 0) && ((rem < 0) != (divisor < 0))) { --quot; }
    return quot;
}

struct NoteRenderParts {
    std::string accidental;
    char        letter    = 'C';
    bool        lowercase = false;
    std::string octave_mod;
};

NoteRenderParts compute_note_render_parts(int midi_pitch, const std::array<int, 7>& key_table) {
    const auto pitch_class = static_cast<int>((midi_pitch % 12 + 12) % 12);
    int        best_idx    = 0;
    int        best_delta  = 999;

    for (int idx = 0; idx < static_cast<int>(LETTERS.size()); ++idx) {
        int delta = pitch_class - NATURAL_PITCH_CLASS[idx];
        while (delta > 6) { delta -= 12; }
        while (delta < -6) { delta += 12; }
        if (std::abs(delta) < std::abs(best_delta)
            || (std::abs(delta) == std::abs(best_delta) && delta > best_delta)) {
            best_delta = delta;
            best_idx   = idx;
        }
    }

    int accidental_delta = best_delta == 999 ? 0 : best_delta;
    int natural_pitch    = 48 + NATURAL_PITCH_CLASS[best_idx];
    int midi_no_acc      = midi_pitch - accidental_delta;
    int octave_steps     = floor_div(midi_no_acc - natural_pitch, 12);

    NoteRenderParts parts;
    if (accidental_delta > 0) {
        parts.accidental.assign(accidental_delta, '^');
    } else if (accidental_delta < 0) {
        parts.accidental.assign(-accidental_delta, '_');
    } else if (key_table[best_idx] != 0) {
        parts.accidental = "=";
    }

    if (octave_steps >= 1) {
        parts.lowercase = true;
        --octave_steps;
        while (octave_steps-- > 0) { parts.octave_mod.push_back('\''); }
    } else if (octave_steps < 0) {
        parts.lowercase = false;
        while (octave_steps++ < 0) { parts.octave_mod.push_back(','); }
    }
    parts.letter
        = parts.lowercase ? static_cast<char>(std::tolower(LETTERS[best_idx])) : LETTERS[best_idx];
    return parts;
}

std::string render_note_event(
    const NoteEvent& note, const Fraction unit, const std::array<int, 7>& key_table
) {
    const auto  parts = compute_note_render_parts(note.midi_pitch, key_table);
    std::string token;
    auto        render_decoration = [](const std::string& value) -> std::string {
        static constexpr std::string_view simple = "~`'^vu.HLMOPST";
        if (value.size() == 1 && simple.find(value[0]) != std::string::npos) { return value; }
        std::string wrapped = "!";
        wrapped += value;
        wrapped.push_back('!');
        return wrapped;
    };

    for (const auto& deco : note.decorations) { token += render_decoration(deco); }
    for (const auto& chord : note.chord_symbols) {
        token.push_back('"');
        token += chord;
        token.push_back('"');
    }

    if (note.has_symbolic_info) {
        token += note.accidental;
        token.push_back(note.letter);

        int base_octave = (note.letter >= 'a' && note.letter <= 'z') ? 4 : 3;
        int diff        = note.octave - base_octave;
        while (diff > 0) {
            token.push_back('\'');
            diff--;
        }
        while (diff < 0) {
            token.push_back(',');
            diff++;
        }

        token += render_length_suffix(note.duration_factor, Fraction{1, 1});
    } else {
        token += parts.accidental;
        token.push_back(parts.letter);
        token += parts.octave_mod;
        token += render_length_suffix(note.length, unit);
    }

    if (note.tie_continue) { token.push_back('-'); }
    return token;
}

std::string render_chord_event(
    const ChordEvent& chord, const Fraction unit, const std::array<int, 7>& key_table
) {
    std::string token;
    for (const auto& symbol : chord.chord_symbols) {
        token.push_back('"');
        token += symbol;
        token.push_back('"');
    }
    token.push_back('[');
    for (const auto& note : chord.notes) { token += render_note_event(note, unit, key_table); }
    token.push_back(']');
    return token;
}

std::string render_bar(const BarEvent& bar) {
    if (bar.final_bar && bar.repeat_end) { return ":|]"; }
    if (bar.final_bar) { return "|]"; }
    if (bar.repeat_start && bar.repeat_end) { return ":|:"; }
    if (bar.repeat_start) { return "|:"; }
    if (bar.repeat_end) { return ":|"; }
    if (bar.double_bar) { return "||"; }
    return "|";
}

std::string render_voice_line(
    const Voice& voice, const Fraction unit, const std::array<int, 7>& key_table
) {
    std::vector<std::string> tokens;
    tokens.reserve(voice.elements.size());
    bool        in_grace = false;
    std::string grace_block;
    auto        flush_grace = [&]() {
        if (!in_grace) { return; }
        grace_block.push_back('}');
        tokens.push_back(grace_block);
        grace_block.clear();
        in_grace = false;
    };

    for (const auto& element : voice.elements) {
        if (const auto* note = std::get_if<NoteEvent>(&element)) {
            auto rendered = render_note_event(*note, unit, key_table);
            if (note->is_grace) {
                if (!in_grace) {
                    in_grace    = true;
                    grace_block = "{";
                } else {
                    grace_block.push_back(' ');
                }
                grace_block += rendered;
            } else {
                flush_grace();
                tokens.push_back(std::move(rendered));
            }
        } else if (const auto* rest = std::get_if<RestEvent>(&element)) {
            flush_grace();
            auto        suffix = render_length_suffix(rest->length, unit);
            std::string token(1, rest->symbol);
            token += suffix;
            tokens.push_back(std::move(token));
        } else if (const auto* bar = std::get_if<BarEvent>(&element)) {
            flush_grace();
            tokens.push_back(render_bar(*bar));
        } else if (const auto* chord = std::get_if<ChordEvent>(&element)) {
            flush_grace();
            tokens.push_back(render_chord_event(*chord, unit, key_table));
        }
    }
    flush_grace();

    std::ostringstream oss;
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        if (i) { oss << ' '; }
        oss << tokens[i];
    }
    return oss.str();
}

std::string render_document(const Document& document) {
    std::ostringstream oss;
    oss << "X: " << document.header.reference_number << '\n';
    if (!document.header.title.empty()) { oss << "T: " << document.header.title << '\n'; }
    oss << "M: " << render_meter(document.header.meter) << '\n';
    oss << "L: " << fraction_to_text(document.header.unit_note_length) << '\n';
    oss << "Q: " << render_tempo(document.header.tempo) << '\n';
    oss << "K: " << render_key(document.header.key) << '\n';
    for (const auto& directive : document.directives) {
        if (!directive.empty()) { oss << directive << '\n'; }
    }

    const bool multi_voice = document.voices.size() > 1;
    auto       key_table   = build_key_signature(document.header.key);

    for (std::size_t idx = 0; idx < document.voices.size(); ++idx) {
        const auto& voice = document.voices[idx];
        if (multi_voice) { oss << "V: V" << (idx + 1) << '\n'; }
        oss << render_voice_line(voice, document.header.unit_note_length, key_table) << '\n';
        for (const auto& line : voice.lyrics) {
            if (line.empty()) { continue; }
            oss << "w: " << line << '\n';
        }
        if (idx + 1 < document.voices.size()) { oss << '\n'; }
    }
    return oss.str();
}

}   // namespace miniabc
