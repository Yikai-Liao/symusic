#include "notation.h"

#include <cmath>
#include <limits>

#include "lanes.h"
#include "playback.h"

namespace symusic::abc::detail {

std::string_view read_mark(std::string_view spelling) {
    if (spelling.size() >= 2 && (spelling.front() == '!' || spelling.front() == '+')
        && spelling.back() == spelling.front())
        return spelling.substr(1, spelling.size() - 2);
    return spelling;
}

bool Ornament::* ornament_flag(std::string_view spelling) {
    const auto name = read_mark(spelling);
    if (spelling.size() != 1 && name.size() == 1) return name == ">" ? &Ornament::accent : nullptr;
    struct Feature {
        std::string_view name;
        bool Ornament::* flag;
    };
    constexpr std::array kFeatures{
        Feature{".", &Ornament::staccato},
        Feature{"'", &Ornament::staccato},
        Feature{"breath", &Ornament::staccato},
        Feature{"accent", &Ornament::accent},
        Feature{"emphasis", &Ornament::accent},
        Feature{"sfz", &Ornament::accent},
        Feature{"L", &Ornament::accent},
        Feature{"tenuto", &Ornament::tenuto},
        Feature{"arpeggio", &Ornament::arpeggio},
        Feature{"fermata", &Ornament::fermata},
        Feature{"invertedfermata", &Ornament::fermata},
        Feature{"H", &Ornament::fermata},
        Feature{"trill", &Ornament::trill},
        Feature{"T", &Ornament::trill},
        // ABC 2.1 names '~' as !roll!; abc2midi's 'R' is a separate expansion.
        Feature{"roll", &Ornament::cut},
        Feature{"~", &Ornament::cut},
        Feature{"R", &Ornament::roll}
    };
    const auto feature = std::ranges::find(kFeatures, name, &Feature::name);
    return feature == kFeatures.end() ? nullptr : feature->flag;
}

Alteration Accidentals::resolve(Pitch pitch, const Key& key, AccidentalScope scope) {
    const auto index = letter_index(pitch.letter);
    // ABC 2.1 sections 4.6 and 11.3: changing octave is equivalent to writing
    // an octave mark on the note. Local propagation uses the resulting octave.
    const auto local = std::pair{pitch.letter, key.effective_octave(pitch.octave)};
    if (pitch.accidental) {
        if (scope == AccidentalScope::Pitch) {
            by_letter[index] = pitch.accidental;
            by_octave.erase(
                by_octave.lower_bound({pitch.letter, std::numeric_limits<i32>::min()}),
                by_octave.upper_bound({pitch.letter, std::numeric_limits<i32>::max()})
            );
        } else if (scope == AccidentalScope::Octave) {
            by_octave.insert_or_assign(local, *pitch.accidental);
        }
        return *pitch.accidental;
    }
    if (const auto slot = by_octave.find(local); slot != by_octave.end()) return slot->second;
    return by_letter[index].value_or(key.alterations[index]);
}

bool WrittenState::change_key(std::string_view value, Diagnostics& diagnostics, Location at) {
    const auto next = key(value, fields.tone, diagnostics, at);
    if (!next) {
        diagnostics.warn(at, "invalid-inline-key", "ignored an unrecognized key change");
        return false;
    }
    fields.tone = next->state;
    fields.tone.apply(next->pitch);
    if (next->sets_signature) accidentals.reset();
    return next->sets_signature;
}

std::optional<HarpAttacks> harp_attacks(const Prepared& note) {
    if (!note.harp_cut()) return {};
    const auto length = note.cut->length;
    return detail::harp_attacks(
        note.ornament.fermata ? fermata_length(length, note.lengths.fermata) : length
    );
}

namespace {

class WrittenTuplet {
public:
    void                 start(Tuplet group, Meter meter, Location at, Diagnostics& diagnostics);
    [[nodiscard]] Length scale() const { return remaining ? ratio : Length{}; }
    Length               take() {
        if (!remaining) return {};
        --remaining;
        return ratio;
    }
    void finish(Diagnostics& diagnostics) const {
        if (remaining)
            diagnostics.warn(opening, "unfinished-tuplet", "tuplet has fewer notes than declared");
    }

private:
    i32      remaining = 0;
    Length   ratio;
    Location opening;
};

i32 default_tuplet(i32 count, Meter meter) {
    if (count == 2 || count == 4 || count == 8) return 3;
    if (count == 3 || count == 6) return 2;
    // Simple triple time has three beats. Compound meters group more than three in threes.
    const bool compound = !meter.free && meter.numerator > 3 && meter.numerator % 3 == 0;
    return compound ? 3 : 2;
}

void WrittenTuplet::start(Tuplet group, Meter meter, Location at, Diagnostics& diagnostics) {
    if (group.count < 2 || group.count > 1024 || group.in_time_of > 1024 || group.notes > 1024)
        diagnostics.error(
            at, "invalid-tuplet", "tuplet count must be between 2 and 1024, with at most 1024 notes"
        );
    if (remaining)
        diagnostics.warn(at, "nested-tuplet", "new tuplet interrupted the previous tuplet");
    remaining = group.notes ? group.notes : group.count;
    ratio = {group.in_time_of ? group.in_time_of : default_tuplet(group.count, meter), group.count};
    opening = at;
}

// Marks can revise the previous note until another note, bar or overlay intervenes.
// Even a skipped left operand can set the length of a played right operand.
class AdjacentNotes {
public:
    AdjacentNotes(const Meter& opening_meter, Diagnostics& diagnostics) :
        opening_meter(opening_meter), diagnostics(diagnostics) {}
    void append(Prepared right, Prepared* destination, Length tuplet, f64 phase);
    void boundary();
    void broken(const Token& token);
    void tie(Location at);
    void close_slur();
    void grace() { starts_swing = false; }
    void part() { crosses_part = true; }

private:
    const Meter& opening_meter;
    Diagnostics& diagnostics;
    Prepared     skipped{};
    Prepared*    left         = nullptr;
    const Token* mark         = nullptr;
    bool         starts_swing = false;
    bool         crosses_part = false;

    void scale_pair(Prepared& right, Broken rhythm);
};

void AdjacentNotes::boundary() {
    if (mark)
        diagnostics.warn(
            mark->at, "unfinished-broken-rhythm", "broken rhythm did not reach a following note"
        );
    mark         = nullptr;
    left         = nullptr;
    starts_swing = crosses_part = false;
}

void AdjacentNotes::broken(const Token& token) {
    if (left && !mark)
        mark = &token;
    else
        diagnostics.warn(
            token.at,
            "unexpected-broken-rhythm",
            "ignored a broken rhythm mark without a preceding note"
        );
}

void AdjacentNotes::tie(Location at) {
    if (left)
        left->tied = true;
    else
        diagnostics.warn(at, "unexpected-tie", "ignored tie without a preceding note");
}

void AdjacentNotes::close_slur() {
    if (left && left->slur == SlurPosition::Inside && !left->atom->chord)
        left->slur = SlurPosition::Closing;
}

void AdjacentNotes::scale_pair(Prepared& right, Broken rhythm) {
    const auto harp        = harp_attacks(*left);
    const auto last_attack = harp ?
                                 harp->last * left->unit * left->rhythm_scale * left->last_scale :
                                 left->played_duration();
    // A right-hand harp cut includes the preceding attack in abc2midi's range.
    const bool equal = std::abs(last_attack - right.duration) <= kTickTolerance;
    if (!equal && !right.harp_cut()) {
        if (mark)
            diagnostics.warn(
                mark->at,
                "unequal-broken-rhythm",
                "ignored broken rhythm between notes of unequal length"
            );
        return;
    }
    const auto& ratio  = right.atom->rhythm;
    const auto  first  = rhythm.first_factor(2.0 * ratio.shorter / (ratio.longer + ratio.shorter));
    right.rhythm_scale = 2 - first;
    // A part label separates the harp cut's opening from the preceding attack.
    auto& left_scale = harp ? left->last_scale : left->rhythm_scale;
    left_scale *= first * (right.harp_cut() && !crosses_part ? right.rhythm_scale : 1);
}

void AdjacentNotes::append(Prepared right, Prepared* destination, Length tuplet, f64 phase) {
    const auto& atom = *right.atom;
    const bool  swingable
        = atom.length.numerator == 1 && tuplet.numerator == 1 && !atom.chord && !atom.notes.empty();
    if (mark)
        scale_pair(right, std::get<Broken>(mark->value));
    else if (
        starts_swing && swingable && atom.rhythm.swing
        && std::abs(left->duration - right.duration) < kTickTolerance
    )
        scale_pair(right, Broken{1, 1});
    starts_swing = swingable && starts_hornpipe_pair(opening_meter, phase, right.duration);
    crosses_part = false;
    mark         = nullptr;
    left         = destination ? destination : &skipped;
    *left        = right;
}

class WrittenVoice : private OverlayNotation {
public:
    WrittenVoice(
        WrittenFields       opening,
        const TuneSettings& settings,
        Diagnostics&        diagnostics,
        WrittenNavigation&  result
    ) :
        OverlayNotation(opening, diagnostics), settings(settings), diagnostics(diagnostics),
        result(result), adjacent(settings.meter, diagnostics) {}
    void read(const VoicePlan& plan);

private:
    const TuneSettings&                            settings;
    Diagnostics&                                   diagnostics;
    WrittenNavigation&                             result;
    decltype(WrittenNavigation::entries)::iterator next_entry;
    bool                                           visited = false;
    // Unmodified written time since the barline. A split starts its secondary at zero.
    f64                                primary_phase = 0, secondary_phase = 0;
    std::array<size_t, kMaxVoiceLanes> slurs{};
    WrittenTuplet                      tuplet;
    AdjacentNotes                      adjacent;
    Ornament                           ornament;

    f64& phase() { return id() == 0 ? primary_phase : secondary_phase; }
    void remember(const Atom& atom, AccidentalScope scope);
    void enter(const Token& token);
    void visit(const Atom& atom, const Token& token);
    void visit(const Grace& grace, const Token& token);
    void visit(const Bar& bar, const Token& token);
    void visit(Overlay command, const Token& token);
    void visit(const Field& field, const Token& token);
    void visit(Slur slur, const Token& token);
    void visit(const Decoration& mark, const Token& token);
    void visit(Tuplet group, const Token& token) {
        tuplet.start(group, notation().fields.meter, token.at, diagnostics);
    }
    void visit(Broken, const Token& token) { adjacent.broken(token); }
    void visit(Tie, const Token& token) { adjacent.tie(token.at); }
    void visit(Spacer, const Token&) { ornament = {}; }
    void visit(const PitchChange& pitch, const Token&) { notation().fields.tone.apply(pitch); }
    void visit(const auto&, const Token&) {}
};

void WrittenVoice::enter(const Token& token) {
    // A part label inside a measure inherits even the accidentals from skipped
    // notes. An implicit overlay first gives the measure back to the primary lane.
    if (const auto* field = std::get_if<Field>(&token.value); field && field->kind == 'P') {
        if (close_implicit()) adjacent.boundary();
        adjacent.part();
    }
    if (next_entry != result.entries.end() && next_entry->first == &token) {
        next_entry->second = notation();
        ++next_entry;
    }
}

void WrittenVoice::remember(const Atom& atom, AccidentalScope scope) {
    auto& state = notation();
    for (const auto& note : atom.notes)
        if (note.pitch.accidental) state.accidentals.resolve(note.pitch, state.fields.tone, scope);
}

void WrittenVoice::visit(const Atom& atom, const Token& token) {
    const auto& written = notation().fields;
    const auto  scale   = tuplet.take();
    const auto  beat
        = (atom.measure_rest ? written.meter.measure() : written.unit) * scale.fraction();
    const auto duration = beat * atom.length.fraction();
    const auto cut      = settings.harp_cuts.for_atom(atom, ornament, token.at, scale);
    const auto slurred  = slurs[id()] && !(cut && cut->harp);
    Prepared   note{
        .atom        = &atom,
        .at          = token.at,
        .music_line  = token.music_line,
        .duration    = duration,
        .unit        = written.unit,
        .tuplet_unit = beat,
        .ornament    = std::exchange(ornament, {}),
        .lengths     = settings.note_lengths.at(token.at),
        .cut         = cut,
        .tied        = atom.tie,
        .slur        = slurred ? SlurPosition::Inside : SlurPosition::Outside
    };
    auto& position = phase();
    adjacent.append(note, visited ? &result.notes[&token] : nullptr, scale, position);
    position += duration;
    if (next_entry != result.entries.end()) remember(atom, settings.accidentals.at(token.at));
}

void WrittenVoice::visit(const Grace& grace, const Token& token) {
    adjacent.grace();
    if (visited) result.grace_units[&token] = notation().fields.unit * tuplet.scale().fraction();
    if (next_entry != result.entries.end()) {
        const auto scope = settings.accidentals.at(token.at);
        for (const auto& attack : grace.attacks) remember(attack.atom, scope);
    }
}

void WrittenVoice::visit(const Bar& bar, const Token&) {
    adjacent.boundary();
    ornament = {};
    if (bar.resets_measure) {
        close_implicit();
        notation().accidentals.reset();
        phase() = 0;
    }
}

void WrittenVoice::visit(Overlay command, const Token& token) {
    adjacent.boundary();
    ornament = {};
    if (control(command, token.at) && (command == Overlay::Split || command == Overlay::End))
        phase() = 0;
}

void WrittenVoice::visit(const Field& field, const Token& token) {
    auto&      state      = notation();
    const auto set_metric = [&](auto& setting, auto operand) {
        setting = operand;
        if (visited) result.metric_fields[&field] = operand;
    };
    switch (field.kind) {
    case 'K': state.change_key(field.value, diagnostics, token.at); break;
    case 'L': set_metric(state.fields.unit, unit_length(field.value, diagnostics, token.at)); break;
    case 'M': set_metric(state.fields.meter, meter(field.value, diagnostics, token.at)); break;
    default: break;
    }
}

void WrittenVoice::visit(Slur slur, const Token& token) {
    auto& depth = slurs[id()];
    if (slur.start)
        ++depth;
    else if (!depth)
        diagnostics.warn(token.at, "unmatched-slur", "ignored ')' without a matching slur");
    else if (--depth == 0)
        adjacent.close_slur();
}

void WrittenVoice::visit(const Decoration& mark, const Token& token) {
    const auto flag = ornament_flag(mark.spelling);
    if (!flag) return;
    if (mark.last_on_line) {
        diagnostics.warn(
            token.at, "orphan-decoration", "ornament at end of line has no note or rest"
        );
        ornament = {};
    } else
        ornament.*flag = true;
}

void WrittenVoice::read(const VoicePlan& plan) {
    // In written order, the farthest visited end covers every overlapping visit.
    std::map<size_t, size_t> visits;
    for (const auto& range : plan.order) {
        if (range.restore_written) result.entries.try_emplace(&plan.source[range.first]);
        if (range.flow == Flow::Token) {
            auto& end = visits[range.first];
            end       = std::max(end, range.last);
        }
    }
    next_entry        = result.entries.begin();
    auto   next_visit = visits.begin();
    size_t end        = 0;
    for (size_t i = 0; i < plan.source.size(); ++i) {
        if (next_visit != visits.end() && next_visit->first == i)
            end = std::max(end, (next_visit++)->second);
        visited           = i < end;
        const auto& token = plan.source[i];
        enter(token);
        std::visit([&](const auto& value) { visit(value, token); }, token.value);
    }
    adjacent.boundary();
    tuplet.finish(diagnostics);
}

}   // namespace

WrittenNavigation written_navigation(
    const VoicePlan&    plan,
    WrittenFields       opening,
    const TuneSettings& settings,
    Diagnostics&        diagnostics
) {
    WrittenNavigation result;
    WrittenVoice(opening, settings, diagnostics, result).read(plan);
    return result;
}

}   // namespace symusic::abc::detail
