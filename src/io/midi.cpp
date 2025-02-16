//
// Created by lyk on 23-12-25.
//

#include <map>
#include <queue>

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

#include "minimidi/MiniMidi.hpp"
#include "pdqsort.h"
#include "MetaMacro.h"

#include "symusic/score.h"
#include "symusic/ops.h"
#include "symusic/utils.h"
#include "symusic/conversion.h"

namespace symusic {

namespace details {
// define some utils

template<typename T>
void sort_by_time(vec<T>& data) {
    pdqsort_branchless(data.begin(), data.end(), [](const auto& a, const auto& b) {
        return (a.time) < (b.time);
    });
}

// 修改TrackHandler，移除NoteManager相关部分
template<typename T>
struct TrackHandler {
    TrackNative<T> track;

    // 移除NoteManager相关成员函数
    TrackHandler() = default;
    TrackHandler(const std::string& name, uint8_t program, bool is_drum) :
        track(name, program, is_drum) {}
};

// 修改NoteManager结构
template<typename T>
struct NoteManager {
    struct Slot {
        uint32_t              count = 0;
        uint32_t              index = 0;
        std::vector<Note<T>>* notes = nullptr;
    };

    // 16 channels * 128 pitches
    std::array<Slot, 16 * 128>                                                           slots{};
    std::unordered_map<uint16_t, std::queue<std::pair<uint32_t, std::vector<Note<T>>*>>> overflow;

    void add(
        const uint8_t         channel,
        uint8_t               pitch,
        typename T::unit      time,
        int8_t                velocity,
        std::vector<Note<T>>& notes
    ) {
        const uint16_t key = channel * 128 + pitch;
        notes.emplace_back(time, -1, pitch, velocity);
        const auto idx = notes.size() - 1;

        if (Slot& slot = slots[key]; slot.count++ == 0) {
            slot.index = idx;
            slot.notes = &notes;
        } else {
            overflow.try_emplace(key).first->second.emplace(idx, &notes);
        }
    }

    bool end(const uint8_t channel, const uint8_t pitch, typename T::unit end_time) {
        const uint16_t key  = channel * 128 + pitch;
        Slot&          slot = slots[key];
        if (slot.count == 0 || !slot.notes) return false;

        auto& note    = (*slot.notes)[slot.index];
        note.duration = end_time - note.time;

        if (--slot.count > 0) {
            auto& [new_idx, new_notes] = overflow[key].front();
            slot.index                 = new_idx;
            slot.notes                 = new_notes;
            overflow[key].pop();
        }
        return true;
    }
};

struct TrackKey {
    uint8_t channel;
    uint8_t program;

    bool operator==(const TrackKey& other) const { return channel == other.channel && program == other.program; }
    auto operator<=>(const TrackKey& other) const { return std::tie(channel, program) <=> std::tie(other.channel, other.program); }
};



// 修改TrackManager
template<typename T>
class TrackManager {
    NoteManager<T>                      noteManager;
    std::map<TrackKey, TrackHandler<T>> formalTracks;
    std::array<TrackHandler<T>, 16>     stragglers;   // 使用 std::array 替代 C 风格数组
    TrackHandler<T>*                    lastTrack = nullptr;
    TrackKey                            lastKey{255, 255};
    size_t                              msg_num = 0;
    std::array<uint8_t, 16>             cur_instr{};   // 新增当前乐器状态数组

    // 新增内部方法获取notes引用
    std::vector<Note<T>>& get_notes(uint8_t channel) { return get<false>(channel).track.notes; }

public:
    explicit TrackManager(const size_t msg_num) : msg_num(msg_num) {}

    // 新增设置program的方法
    void set_program(uint8_t channel, uint8_t program) { cur_instr[channel] = program; }

    template<bool create_new>
    TrackHandler<T>& get(uint8_t channel) {
        const uint8_t  program = cur_instr[channel];
        const TrackKey key{channel, program};

        // 使用结构化绑定简化代码
        if (lastTrack && lastKey == key) { return *lastTrack; }


        // 使用 if-init 语句简化查找
        if (auto it = formalTracks.find(key); it != formalTracks.end()) {
            lastKey = key;
            lastTrack = &it->second;
            return *lastTrack;
        }

        // 统一 straggler 访问方式
        auto& straggler = stragglers[channel];
        if constexpr (!create_new) return straggler;

        // 准备新轨道属性
        straggler.track.program = program;
        straggler.track.is_drum = channel == 9;

        // 使用 try_emplace 优化插入逻辑
        auto [iter, inserted] = formalTracks.try_emplace(key, std::move(straggler));

        // 重置 straggler 状态
        stragglers[channel] = TrackHandler<T>();

        auto& newTrack = iter->second;
        newTrack.track.notes.reserve(msg_num / 2 + 1);
        lastKey = key;
        lastTrack = &newTrack;
        return *lastTrack;
    }

    // 修改音符处理方法
    void add_note(uint8_t channel, uint8_t pitch, typename T::unit time, int8_t velocity) {
        auto& track = get<true>(channel).track;
        noteManager.add(channel, pitch, time, velocity, track.notes);
    }

    bool end_note(uint8_t channel, uint8_t pitch, typename T::unit end_time) {
        return noteManager.end(channel, pitch, end_time);
    }

    void finalize(ScoreNative<T>& score, const std::string& name) {
        // 转移轨道
        for (auto& [_, handler] : formalTracks) {
            if (!handler.track.empty()) {
                auto& notes = handler.track.notes;
                notes.erase(
                    std::remove_if(
                        notes.begin(), notes.end(), [](const Note<T>& n) { return n.duration < 0; }
                    ),
                    notes.end()
                );

                handler.track.name = name;
                score.tracks.push_back(std::move(handler.track));
            }
        }
    }
};

template<TType T, typename Conv, typename Container>   // only works for Tick and Quarter
    requires(std::is_same_v<T, Tick> || std::is_same_v<T, Quarter>)
[[nodiscard]] Score<T> parse_midi(const minimidi::MidiFileView<Container>& midi, Conv tick2unit) {
    typedef typename T::unit unit;
    // remove this redundant copy in the future
    const u16      tpq = midi.ticks_per_quarter();
    ScoreNative<T> score(tpq);   // create a score with the given ticks per quarter
    int            count = -1;
    for (const minimidi::TrackView<Container>& midi_track : midi) {

        const size_t    message_num = midi_track.size / 3 + 100;
        TrackManager<T> trackManager(message_num);
        std::string     cur_name;
        count += 1;
        // channel -> pedal_on
        std::array<unit, 16> last_pedal_on{-1};
        // iter midi messages in the track

        for (const auto& msg : midi_track) {

            const auto cur_tick = static_cast<Tick::unit>(msg.time);
            const auto cur_time = tick2unit(cur_tick);
            switch (msg.type()) {
            case minimidi::MessageType::NoteOn: {
                const auto& note_on = msg.template cast<minimidi::NoteOn>();
                if (note_on.velocity() != 0) {
                    trackManager.add_note(
                        note_on.channel(),
                        note_on.pitch(),
                        cur_tick,
                        note_on.velocity()
                    );
                    break;
                }
                // 处理 velocity=0 的情况作为 NoteOff
            }
            case minimidi::MessageType::NoteOff: {
                const auto& note_off = msg.template cast<minimidi::NoteOff>();
                trackManager.end_note(note_off.channel(), note_off.pitch(), cur_tick);
                break;
            }
            case minimidi::MessageType::ProgramChange: {
                const auto&   program_change = msg.template cast<minimidi::ProgramChange>();
                const uint8_t channel        = program_change.channel();
                const uint8_t program        = program_change.program();
                if (program >= 128)
                    throw std::range_error("Get program=" + std::to_string(program));
                trackManager.set_program(channel, program);   // 改为调用TrackManager的方法
                break;
            }
            case minimidi::MessageType::ControlChange: {
                const auto&   control_change = msg.template cast<minimidi::ControlChange>();
                const uint8_t channel        = control_change.channel();

                auto& handler = trackManager.template get<false>(channel);
                auto& track   = handler.track;
                if (track.controls.capacity() < message_num / 2) [[unlikely]] {
                    track.controls.reserve(message_num / 2);
                }

                const uint8_t control_number = control_change.control_number();
                const uint8_t control_value  = control_change.control_value();

                if (control_number >= 128)
                    throw std::range_error("Get control_number=" + std::to_string(control_number));
                if (control_value >= 128)
                    throw std::range_error("Get control_value=" + std::to_string(control_value));
                track.controls.emplace_back(cur_time, control_number, control_value);
                // Pedal Part
                if (control_number == 64) {
                    if (control_value >= 64) {
                        if (last_pedal_on[channel] < 0) last_pedal_on[channel] = cur_time;
                    } else {
                        if (last_pedal_on[channel] >= 0) {
                            track.pedals.emplace_back(
                                last_pedal_on[channel], cur_time - last_pedal_on[channel]
                            );
                            last_pedal_on[channel] = -1;
                        }
                    }
                }
                break;
            }
            case minimidi::MessageType::PitchBend: {
                const auto& pitch_bend = msg.template cast<minimidi::PitchBend>();
                auto&       track = trackManager.template get<false>(pitch_bend.channel()).track;
                auto        value = pitch_bend.pitch_bend();
                if (value < minimidi::PitchBend<>::MAX_PITCH_BEND
                    || value > minimidi::PitchBend<>::MIN_PITCH_BEND)
                    throw std::range_error("Get pitch_bend=" + std::to_string(value));
                track.pitch_bends.emplace_back(cur_time, value);
                break;
            }
                // Meta Message
            case minimidi::MessageType::Meta: {
                switch (const auto& meta = msg.template cast<minimidi::Meta>(); meta.meta_type()) {
                case (minimidi::MetaType::TrackName): {
                    auto data = meta.meta_value();
                    auto tmp  = std::string(data.begin(), data.end());
                    cur_name  = strip_non_utf_8(tmp);
                    break;
                }
                case (minimidi::MetaType::TimeSignature): {
                    const auto& time_sig = meta.template cast<minimidi::TimeSignature>();
                    score.time_signatures.emplace_back(
                        tick2unit(cur_tick), time_sig.numerator(), time_sig.denominator()
                    );
                    break;
                }
                case (minimidi::MetaType::SetTempo): {
                    // store the raw tempo value(mspq) directly
                    // qpm is calculated when needed
                    score.tempos.emplace_back(
                        cur_time, meta.template cast<minimidi::SetTempo>().tempo()
                    );
                    break;
                }
                case (minimidi::MetaType::KeySignature): {
                    const auto& k_msg = meta.template cast<minimidi::KeySignature>();
                    score.key_signatures.emplace_back(cur_time, k_msg.key(), k_msg.tonality());
                    break;
                }
                case (minimidi::MetaType::Lyric): {
                    auto  data  = meta.meta_value();
                    auto& track = trackManager.template get<true>(meta.channel()).track;
                    auto  text  = strip_non_utf_8(std::string(data.begin(), data.end()));

                    if (text.empty()) break;
                    track.lyrics.emplace_back(cur_time, text);
                    break;
                }
                case (minimidi::MetaType::Marker): {
                    auto data = meta.meta_value();
                    auto tmp  = std::string(data.begin(), data.end());
                    auto text = strip_non_utf_8(tmp);
                    if (text.empty()) break;
                    score.markers.emplace_back(cur_time, text);
                    break;
                }
                default: break;
                }
                break;
            }
            default: break;
            }
        }
        trackManager.finalize(score, cur_name);
    }
    sort_by_time(score.time_signatures);
    sort_by_time(score.key_signatures);
    sort_by_time(score.tempos);
    sort_by_time(score.markers);
    return to_shared(std::move(score));
}

minimidi::MidiFile<> to_midi(const Score<Tick>& score) {
    minimidi::MidiFile<> midi{
        minimidi::MidiFormat::MultiTrack, 0, static_cast<u16>(score.ticks_per_quarter)
    };

    midi.tracks.reserve(score.tracks->size() + 1);
    minimidi::Messages<> init_msgs{};
    {   // add meta messages
        auto&        msgs        = init_msgs;
        const size_t message_num = score.time_signatures->size() + score.key_signatures->size()
                                   + score.tempos->size() + score.markers->size();
        msgs.reserve(message_num);
        // add time signatures
        for (const auto& time_signature : *score.time_signatures) {
            msgs.emplace_back(
                minimidi::TimeSignature(
                    time_signature->time, time_signature->numerator, time_signature->denominator
                )
            );
        }
        // add key signatures
        for (const auto& key_signature : *score.key_signatures) {
            msgs.emplace_back(
                minimidi::KeySignature(
                    key_signature->time, key_signature->key, key_signature->tonality
                )
            );
        }
        // add tempos
        for (const auto& tempo : *score.tempos) {
            msgs.emplace_back(minimidi::SetTempo(tempo->time, tempo->mspq));
        }
        // add markers
        for (const auto& marker : *score.markers) {
            msgs.emplace_back(minimidi::Marker(marker->time, marker->text));
        }
    }
    const std::array<u8, 15> valid_channel{0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 11, 12, 13, 14, 15};

    for (size_t idx = 0; const auto& track : *score.tracks) {
        minimidi::Messages<> msgs;
        if (!init_msgs.empty()) {
            msgs      = std::move(init_msgs);
            init_msgs = {};
        } else {
            msgs = {};
            msgs.reserve(
                msgs.size() + track->note_num() * 2 + track->controls->size()
                + track->pitch_bends->size() + track->lyrics->size() + 10
            );
        }
        const u8 channel = track->is_drum ? 9 : valid_channel[idx % 15];
        // add track name
        if (!track->name.empty()) msgs.emplace_back(minimidi::TrackName(0, track->name));
        // add program change
        msgs.emplace_back(minimidi::ProgramChange(0, channel, track->program));
        // Todo add check for Pedal
        // add control change
        for (const auto& control : *track->controls) {
            msgs.emplace_back(
                minimidi::ControlChange(control->time, channel, control->number, control->value)
            );
        }
        // add pitch bend
        for (const auto& pitch_bend : *track->pitch_bends) {
            msgs.emplace_back(
                minimidi::PitchBend(pitch_bend->time, channel, static_cast<i16>(pitch_bend->value))
            );
        }
        // add lyrics
        for (const auto& lyric : *track->lyrics) {
            msgs.emplace_back(minimidi::Lyric(lyric->time, lyric->text));
        }

        gfx::timsort(msgs.begin(), msgs.end(), [](const auto& a, const auto& b) {
            return (a.time) < (b.time);
        });
        // add notes
        const size_t note_size  = track->notes->size();
        const auto   note_begin = static_cast<ptrdiff_t>(msgs.size());

        // collect notes and make sure they are sorted
        vec<Note<Tick>> notes(track->notes->size());
        bool            sorted    = true;
        Tick::unit      prev_time = 0;
        for (size_t j = 0; const auto& note : *track->notes) {
            notes[j++] = note;
            sorted &= note.time >= prev_time;
            prev_time = note.time;
        }
        if (!sorted) { sort_by_time(notes); }
        // add notes to messages
        msgs.resize(msgs.size() + note_size * 2);
        for (size_t i = note_begin; const auto& note : notes) {
            if (note.duration > 0) {
                msgs[i] = minimidi::NoteOff(note.end(), channel, note.pitch, note.velocity);
                msgs[i + note_size]
                    = minimidi::NoteOn(note.time, channel, note.pitch, note.velocity);
            } else {   // empty note
                msgs[i + note_size]
                    = minimidi::NoteOff(note.time, channel, note.pitch, note.velocity);
                msgs[i] = minimidi::NoteOn(note.time, channel, note.pitch, note.velocity);
            }
            i += 1;
        }
        // insertion sort note_off
        pdqsort_detail::insertion_sort(
            msgs.begin() + note_begin,
            msgs.begin() + (note_begin + static_cast<ptrdiff_t>(note_size)),
            [](const auto& a, const auto& b) { return (a.time) < (b.time); }
        );
        // merge note on and note off
        gfx::timmerge(
            msgs.begin() + note_begin,
            msgs.begin() + note_begin + static_cast<ptrdiff_t>(note_size),
            msgs.end(),
            [](const auto& a, const auto& b) { return (a.time) < (b.time); }
        );
        // merge prev to (note on and note off)
        gfx::timmerge(
            msgs.begin(),
            msgs.begin() + note_begin,
            msgs.end(),
            [](const auto& a, const auto& b) { return (a.time) < (b.time); }
        );
        // messages will be sorted by time in minimidi
        if (!msgs.empty()) {
            // sort msgs by time using timsort
            midi.tracks.emplace_back(std::move(msgs));
            idx += 1;
        }
    }
    if (!init_msgs.empty()) { midi.tracks.emplace_back(std::move(init_msgs)); }
    return std::move(midi);
}

template<TType T>
Score<T> parse_midi(const std::span<const u8> bytes) {
    const minimidi::MidiFileView<std::span<const uint8_t>> midi{bytes.data(), bytes.size()};

    if constexpr (std::is_same_v<T, Tick>) {
        return parse_midi<Tick>(midi, [](const Tick::unit x) { return x; });
    } else if constexpr (std::is_same_v<T, Quarter>) {
        const auto tpq = static_cast<float>(midi.ticks_per_quarter());
        return parse_midi<Quarter>(midi, [tpq](const Tick::unit x) {
            return static_cast<float>(x) / tpq;
        });
    } else {
        return convert<Second>(parse_midi<Tick>(midi, [](const Tick::unit x) { return x; }));
    }
}
}   // namespace details

template<>
template<>
Score<Tick> Score<Tick>::parse<DataFormat::MIDI>(const std::span<const u8> bytes) {
    return details::parse_midi<Tick>(bytes);
}

template<>
template<>
Score<Quarter> Score<Quarter>::parse<DataFormat::MIDI>(const std::span<const u8> bytes) {
    return details::parse_midi<Quarter>(bytes);
}

template<>
template<>
Score<Second> Score<Second>::parse<DataFormat::MIDI>(const std::span<const u8> bytes) {
    return details::parse_midi<Second>(bytes);
}

template<>
template<>
vec<u8> Score<Tick>::dumps<DataFormat::MIDI>() const {
    return details::to_midi(*this).to_bytes_sorted();
}

template<>
template<>
vec<u8> Score<Quarter>::dumps<DataFormat::MIDI>() const {
    return details::to_midi(convert<Tick>(*this)).to_bytes_sorted();
}

template<>
template<>
vec<u8> Score<Second>::dumps<DataFormat::MIDI>() const {
    return details::to_midi(convert<Tick>(*this)).to_bytes_sorted();
}

#define INSTANTIATE_GLOBAL_FUNC(__COUNT, T)                                 \
    template<>                                                              \
    Score<T> parse<DataFormat::MIDI, Score<T>>(std::span<const u8> bytes) { \
        return Score<T>::parse<DataFormat::MIDI>(bytes);                    \
    }                                                                       \
    template<>                                                              \
    vec<u8> dumps<DataFormat::MIDI, Score<T>>(const Score<T>& data) {       \
        return data.dumps<DataFormat::MIDI>();                              \
    }

REPEAT_ON(INSTANTIATE_GLOBAL_FUNC, Tick, Quarter, Second)
#undef INSTANTIATE_GLOBAL_FUNC

}   // namespace symusic
