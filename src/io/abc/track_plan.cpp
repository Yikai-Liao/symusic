#include "playback.h"

#include <algorithm>
#include <bitset>
#include <utility>

#include "lanes.h"

namespace symusic::abc::detail {

namespace {
// A synthetic bar or part entry can close an implicit overlay even when the
// corresponding written token is skipped. Repeat entry alone leaves it open.
size_t select_lane(OverlaySelector& selector, Flow flow, const Token& token) {
    switch (flow) {
    case Flow::Token:
        if (const auto* overlay = std::get_if<Overlay>(&token.value))
            selector.control(*overlay, token.at);
        else if (const auto* bar = std::get_if<Bar>(&token.value); bar && bar->resets_measure)
            selector.close_implicit();
        break;
    case Flow::EnterRepeat:
    case Flow::EnterImplicitRepeat: break;
    case Flow::EnterPart:
    case Flow::JumpRepeat:
    case Flow::SoftBar:
    case Flow::InlineEnding:
    case Flow::HardBar: selector.close_implicit(); break;
    }
    return selector.id();
}

LanePlan& lane_at(vec<LanePlan>& lanes, size_t index, Location declaration) {
    // Splits introduce consecutive lanes, including splits reached through navigation.
    if (index == lanes.size()) lanes.push_back({.declaration = declaration});
    return lanes[index];
}

}   // namespace

void TrackSettingsChange::record(const MidiTrackCommand& command) {
    if (const auto* transpose = std::get_if<MidiTransposition>(&command)) {
        // Every incoming shift maps to clamp(incoming + shift, lowest, highest).
        // Once the endpoints coincide even the incoming shift is immaterial.
        lowest  = transpose->apply(lowest);
        highest = transpose->apply(highest);
        shift   = lowest == highest ? 0 : shift + transpose->amount;
    } else {
        const auto& spacing                   = std::get<ChordAttackChange>(command);
        (spacing.random ? random_limit : gap) = spacing.ticks;
    }
}

MidiTrackSettings TrackSettingsChange::closing(MidiTrackSettings opening) const {
    opening.shift               = std::clamp(opening.shift + shift, lowest, highest);
    opening.attack.gap          = gap.value_or(opening.attack.gap);
    opening.attack.random_limit = random_limit.value_or(opening.attack.random_limit);
    return opening;
}

VoicePlan plan_voice(
    vec<Token>&& written_voice, PlayOrder&& order, Location declaration, Diagnostics& diagnostics
) {
    VoicePlan plan{.source = std::move(written_voice), .order = std::move(order)};
    plan.lanes.push_back({.declaration = declaration});
    OverlaySelector written(diagnostics);
    for (const auto& token : plan.source) {
        const auto* field = std::get_if<Field>(&token.value);
        const auto  flow  = field && field->kind == 'P' ? Flow::EnterPart : Flow::Token;
        const auto  index = select_lane(written, flow, token);
        auto&       lane  = lane_at(plan.lanes, index, token.at);
        if (const auto* overlay = std::get_if<Overlay>(&token.value);
            overlay && *overlay == Overlay::Split) {
            lane.declared_channel = plan.lanes[index - 1].declared_channel;
            plan.uses_conductor   = true;
        }
        discover_backing(token, lane.backing);
        plan.uses_conductor = plan.uses_conductor || lane.backing.any();
        Instruction command(token);
        if (command.midi == "channel" && !lane.declared_channel) {
            const auto channel = integer(word(command.args));
            if (channel && *channel >= 1 && *channel <= kMidiChannels)
                lane.declared_channel = channel;
        }
    }

    OverlaySelector performed(diagnostics);
    for (const auto& range : plan.order) {
        for (const auto& token : range.tokens(std::span(plan.source))) {
            auto& lane = lane_at(plan.lanes, select_lane(performed, range.flow, token), token.at);
            if (range.flow != Flow::Token) continue;
            if (const auto* change = std::get_if<MidiTrackCommand>(&token.value)) {
                lane.settings_change.record(*change);
            } else if (const auto* drums = std::get_if<DrumMapChange>(&token.value)) {
                if (drums->route) lane.drum_changes.apply(*drums->route);
            } else if (const auto* request = std::get_if<ChordChannelRequest>(&token.value)) {
                lane.chord_channels = std::min(kMidiChannels, lane.chord_channels + request->count);
            } else if (Instruction(token).midi == "expand") {
                plan.expands_releases = true;
            }
        }
    }
    return plan;
}

void assign_midi_tracks(std::span<VoicePlan> voices, bool separate_tracks, DrumMap file_drums) {
    vec<LanePlan*> tracks;
    for (auto& voice : voices)
        for (auto& lane : voice.lanes) tracks.push_back(&lane);
    std::ranges::stable_sort(tracks, {}, [](const LanePlan* lane) {
        return lane->declaration.written;
    });

    MidiTrackSettings settings;
    if (separate_tracks && !tracks.empty()) {
        const auto& conductor = voices.front().lanes.front();
        settings              = conductor.settings_change.closing(settings);
        file_drums.apply(conductor.drum_changes);
    }

    std::bitset<kMidiChannels + 1> used;
    used.set(kMidiDrumChannel);
    const auto take_melodic_channel = [&] {
        for (i32 channel = 1; channel <= kMidiChannels; ++channel) {
            if (used[channel]) continue;
            used.set(channel);
            return channel;
        }
        return 0;
    };
    for (auto* lane : tracks) {
        const auto start_pass
            = [&] { return std::exchange(settings, lane->settings_change.closing(settings)); };
        lane->note_settings = start_pass();
        lane->opening_drums = file_drums;
        // Each backing pass repeats the same map assignments as its melody.
        // Applying them once supplies the opening for all backing and the next lane.
        file_drums.apply(lane->drum_changes);
        if (lane->backing.chords) {
            const auto opening                = start_pass();
            lane->backing_opening.chord_shift = opening.shift;
            lane->backing_opening.chord_gap   = opening.attack.gap;
        }
        if (lane->backing.drums) {
            lane->backing_opening.drum_gap = start_pass().attack.gap;
            lane->backing_opening.drums    = file_drums;
        }
        if (lane->backing.drone) lane->backing_opening.drone_shift = start_pass().shift;
        if (separate_tracks && lane->declared_channel) {
            lane->channels.initial = *lane->declared_channel;
            used.set(lane->channels.initial);
        } else if (const i32 channel = take_melodic_channel()) {
            // Score can still store independent melody tracks when MIDI numbers run out.
            lane->channels.initial = channel;
        }
        for (i32 i = 0; i < lane->chord_channels; ++i) {
            const i32 channel = take_melodic_channel();
            if (channel == 0) break;
            lane->channels.extras.push_back(channel);
        }

        // abc2midi writes each lane's backing next. Bass and chords use two
        // channels, the drone one, and drums use reserved channel 10. Each
        // backing track also executes that lane's chord-channel directives.
        const auto backing = lane->backing;
        const i32  backing_channels
            = 2 * backing.chords + backing.drone
              + (backing.chords + backing.drone + backing.drums) * lane->chord_channels;
        for (i32 i = 0; i < backing_channels && take_melodic_channel() != 0; ++i) {}
    }
}

}   // namespace symusic::abc::detail
