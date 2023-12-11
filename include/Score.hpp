#ifndef SCORE_HPP
#define SCORE_HPP
#include <map>
#include <queue>
#include <algorithm>
#include <cmath>
#include <functional>
#include <sstream>
#include <string>
#include <iosfwd>
#include <limits>
#include "pdqsort.h"
#include "MiniMidi.hpp"
#include "MetaMacro.h"
#include "zpp_bits.h"
namespace score {

typedef uint8_t     u8;
typedef int8_t      i8;
typedef uint16_t    u16;
typedef uint32_t    u32;
typedef int32_t     i32;
typedef float       f32;
typedef double      f64;
#define vec std::vector

#define SS_PRECISION 2

using std::to_string;
using std::ostream;
namespace message= minimidi::message;

namespace utils {
inline i8 safe_add(const i8 a, const i8 b) {
    const int ans = a + b;
    if (ans > 127 || ans < 0)
        throw std::range_error("Overflow while adding " + std::to_string(a) + " and " + std::to_string(b));
    return static_cast<i8>(ans);
}
// NOLINTBEGIN
inline std::string strip_non_utf_8(const std::string *str) {
    const size_t f_size = str->size();
    // unsigned char c2{};
    std::string to;
    to.reserve(f_size);
    for (size_t i = 0; i < f_size; i++) {
        //control char
        const u8 c = static_cast<u8>((*str)[i]);
        if (c < 32) {
            //allow only \t \n \r
            if (c == 9 || c == 10 || c == 13) {
                to.append(1, c);
            } continue;
        }

        if (c < 127) {  //normal ASCII
            to.append(1, c); 
            continue;
        } 
        //control char (nothing should be defined here either ASCII, ISO_8859-1 or UTF8, so skipping)
        if (c < 160) {
            //fix microsoft mess, add euro
            if (c == 128) {
                to.append(1, -30);  // 226 for unsigned
                to.append(1, -126); // 130 for unsigned
                to.append(1, -84);  // 172 for unsigned
            }
            //fix IBM mess, add NEL = \n\r
            if (c == 133) {
                to.append(1, 10);
                to.append(1, 13);
            } continue;
        } 
        //invalid for UTF8, converting ASCII
        if (c < 192) {
            to.append(1, (unsigned char) 194);
            to.append(1, c);
            continue;
        } 
        //invalid for UTF8, converting ASCII
        if (c < 194) {
            to.append(1, (unsigned char) 195);
            to.append(1, c - 64);
            continue;
        } 
        //possibly 2byte UTF8
        if (c < 224 && i + 1 < f_size) {
            const u8 c2 = static_cast<u8>((*str)[i + 1]);
            //valid 2byte UTF8
            if (c2 > 127 && c2 < 192) {
                //control char, skipping
                if (!(c == 194 && c2 < 160)) {
                    to.append(1, c);
                    to.append(1, c2);
                } i++; continue;
            }
        } 
        //possibly 3byte UTF8
        else if (c < 240 && i + 2 < f_size) {
            const u8 c2 = static_cast<u8> ((*str)[i + 1]),
                     c3 = static_cast<u8> ((*str)[i + 2]);
            //valid 3byte UTF8
            if (c2 > 127 && c2 < 192 && c3 > 127 && c3 < 192) {
                to.append(1, c);
                to.append(1, c2);
                to.append(1, c3);
                i += 2; continue;
            }
        } 
        //possibly 4byte UTF8
        else if (c < 245 && i + 3 < f_size) {

            const u8 c2 = static_cast<u8> ((*str)[i + 1]),
                     c3 = static_cast<u8> ((*str)[i + 2]),
                     c4 = static_cast<u8> ((*str)[i + 3]);
            //valid 4byte UTF8
            if (c2 > 127 && c2 < 192 && c3 > 127 && c3 < 192 && c4 > 127 && c4 < 192) {
                to.append(1, c);
                to.append(1, c2);
                to.append(1, c3);
                to.append(1, c4);
                i += 3; continue;
            }
        }
        //invalid UTF8, converting ASCII (c>245 || string too short for multibyte)
        to.append(1, (unsigned char) 195);
        to.append(1, c - 64);
    } return to;
} // NOLINTEND
} // namespace utils end

namespace pianoroll {

#define PITCH_NUM 128
typedef float pianoroll_t;

#define PIANOROLL_MODES                            \
    PIANOROLL_MODE(std::string("onset"), 1)        \
    PIANOROLL_MODE(std::string("frame"), 2)        \
    PIANOROLL_MODE(std::string("both"), 3)         \

uint8_t mode_flag(const std::string& mode) {
    #define PIANOROLL_MODE(name, flag)             \
        if(name == mode) return flag;
        PIANOROLL_MODES
    #undef PIANOROLL_MODE

    throw std::invalid_argument("The mode of pianoroll must be \"onset\", \"frame\" or \"both\"!");
}
#undef PIANOROLL_MODES

class TrackPianoRoll {
public:
    size_t channel_dim;
    size_t pitch_dim;
    size_t time_dim;
    pianoroll_t *data;

    TrackPianoRoll() = default;

    TrackPianoRoll(size_t channel_dim, size_t time_dim, size_t pitch_dim=PITCH_NUM) {
        this->channel_dim = channel_dim;
        this->pitch_dim = pitch_dim;
        this->time_dim = time_dim;
        this->data = new pianoroll_t[channel_dim * pitch_dim * time_dim];

        this->clear();
    };

    void set(size_t channel, size_t start, size_t duration, size_t pitch) {
        std::fill_n(this->data + channel * this->pitch_dim * this->time_dim + pitch * this->time_dim + start,
                    duration,
                    1);
    };

    void clear() {
        std::fill_n(this->data,
                    this->channel_dim * this->pitch_dim * this->time_dim,
                    0);
    };
};

class ScorePianoRoll {
public:
    size_t channel_dim;
    size_t track_dim;
    size_t pitch_dim;
    size_t time_dim;
    pianoroll_t *data;

    ScorePianoRoll() = default;

    ScorePianoRoll(size_t channel_dim, size_t track_dim, size_t time_dim, size_t pitch_dim=PITCH_NUM) {
        this->channel_dim = channel_dim;
        this->track_dim = track_dim;
        this->pitch_dim = pitch_dim;
        this->time_dim = time_dim;
        this->data = new pianoroll_t[channel_dim * track_dim * pitch_dim * time_dim];

        this->clear();
    };

    void set(size_t channel, size_t track, size_t start, size_t duration, size_t pitch) {
        std::fill_n(this->data + channel * this->track_dim * this->pitch_dim * this->time_dim + track * this->pitch_dim * this->time_dim + pitch * this->time_dim + start,
                    duration,
                    1);
    };

    void clear() {
        std::fill_n(this->data,
                    this->channel_dim * this->track_dim * this->pitch_dim * this->time_dim,
                    0);
    };
};
} // namespace pianoroll end

template<typename T>
struct TimeUnit {
    typedef T unit;
};

#define TIME_UNIT_TYPE(UNIT_NAME, UNIT_TYPE)                                \
    struct UNIT_NAME: public TimeUnit<UNIT_TYPE> {};                        \
    inline std::string to_string(const UNIT_NAME) {                         \
        return std::string(#UNIT_NAME);                                     \
    }                                                                       \
    inline std::ostream& operator<<(std::ostream& out, const UNIT_NAME) {   \
        out << #UNIT_NAME; return out;                                      \
    }

TIME_UNIT_TYPE(Tick,    i32)
TIME_UNIT_TYPE(Quarter, f32)
TIME_UNIT_TYPE(Second,  f32)

template<typename T>
struct TimeStamp {
    typedef T ttype;
    typedef typename T::unit unit;
    unit time = 0;

    TimeStamp() = default;
    TimeStamp(const TimeStamp &) = default;
    explicit TimeStamp(const unit &time) : time(time) {};
};

#define TIME_EVENT                                                          \
    template<typename T>                                                    \
    struct CLASS_NAME : public TimeStamp<T>

// NOLINTBEGIN
#define DEFAULT_METHODS                                                     \
    typedef T ttype;                                                        \
    typedef typename T::unit unit;                                          \
    CLASS_NAME() = default;                                                 \
    CLASS_NAME(const CLASS_NAME &) = default;                               \
    [[nodiscard]] inline CLASS_NAME copy() const { return {*this}; };       \
    CLASS_NAME & shift_time_inplace(unit offset) {                          \
        this->time += offset; return *this;                                 \
    }                                                                       \
    [[nodiscard]] CLASS_NAME shift_time(unit offset) const {                \
        return copy().shift_time_inplace(offset);                           \
    }                                                                       \
    bool operator!=(const CLASS_NAME &other) const {                        \
        return !(*this == other);                                           \
    }

#define ZPP_INIT(ATTR_NUM)                                                  \
    using serialize = zpp::bits::members<ATTR_NUM>;                         \
    // explicit CLASS_NAME(auto && ...) {}                                  \


#define SHIFT_I8(ATTR)                                                      \
    CLASS_NAME & shift_##ATTR##_inplace (unit offset) {                     \
        ATTR = utils::safe_add(ATTR, offset); return *this;                 \
    }                                                                       \
    [[nodiscard]] CLASS_NAME shift_##ATTR (unit offset) {                   \
        return copy().shift_##ATTR##_inplace(offset);                       \
    }
// NOLINTEND

#define CLASS_NAME Note
TIME_EVENT {
    DEFAULT_METHODS
    unit duration;
    i8 pitch{}, velocity{};

    constexpr static auto serialize(auto & archive, auto & event){
        return archive(event.time, event.duration, event.pitch, event.velocity);
    }

    bool operator==(const Note &other) const {
        return this->time == other.time && this->duration == other.duration &&
               this->pitch == other.pitch && this->velocity == other.velocity;
    }

    Note(unit time, unit duration, i8 pitch, i8 velocity) :
        TimeStamp<T>(time), duration(duration), pitch(pitch), velocity(velocity) {};

    template<typename U>
    Note(unit time, const Note<U>& other):
        TimeStamp<T>(time), duration(other.duration), pitch(other.pitch), velocity(other.velocity) {};

    template<typename U>
    Note(unit time, unit duration, const Note<U>& other):
        TimeStamp<T>(time), duration(duration), pitch(other.pitch), velocity(other.velocity) {};

    SHIFT_I8(pitch)

    SHIFT_I8(velocity)

    [[nodiscard]] bool empty() const {
        return duration <= 0 || velocity <= 0 || this->time <= 0;
    }

    [[nodiscard]] unit start() const { return this->time; }

    [[nodiscard]] unit end() const { return this->time + duration; }

    [[nodiscard]] std::string to_string() const {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(SS_PRECISION);
        ss << "Note"
           << "(time="      << this->time
           << ", duration=" << duration
           << ", pitch="    << static_cast<i32>(pitch)
           << ", velocity=" << static_cast<i32>(velocity)
           << ", ttype="    << ttype()
           << ")";
        return ss.str();
    }
};
#undef CLASS_NAME // Note

#define CLASS_NAME ControlChange
TIME_EVENT{
    DEFAULT_METHODS
    u8 number{}, value{};

    static constexpr auto serialize(auto & archive, auto & event){
        return archive(event.time, event.number, event.value);
    }

    bool operator==(const ControlChange &other) const {
        return this->time == other.time && this->number == other.number &&
               this->value == other.value;
    }

    ControlChange(unit time, u8 number, u8 value) :
        TimeStamp<T>(time), number(number), value(value) {};

    template<typename U>
    ControlChange(unit time, const ControlChange<U>& other):
        TimeStamp<T>(time), number(other.number), value(other.value) {};

    [[nodiscard]] std::string to_string() const {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(SS_PRECISION);
        ss << "ControlChange"
           << "(time="      << this->time
           << ", number="   << static_cast<i32>(number)
           << ", value="    << static_cast<i32>(value)
           << ", ttype="    << ttype()
           << ")";
        return ss.str();
    }
};
#undef CLASS_NAME // ControlChange

#define CLASS_NAME TimeSignature
TIME_EVENT{
    DEFAULT_METHODS
    u8 numerator{}, denominator{};

    constexpr static auto serialize(auto & archive, auto & event){
        return archive(event.time, event.numerator, event.denominator);
    }

    bool operator==(const TimeSignature &other) const {
        return this->time == other.time && this->numerator == other.numerator &&
               this->denominator == other.denominator;
    }

    TimeSignature(const unit time, const u8 numerator, const u8 denominator) :
        TimeStamp<T>(time), numerator(numerator), denominator(denominator) {};

    TimeSignature(const unit time, const minimidi::message::TimeSignature msg) :
        TimeStamp<T>(time), numerator(msg.numerator), denominator(msg.denominator) {};

    template<typename U>
    TimeSignature(const unit time, const TimeSignature<U>& other):
        TimeStamp<T>(time), numerator(other.numerator), denominator(other.denominator) {};

    [[nodiscard]] std::string to_string() const {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(SS_PRECISION);
        ss << "TimeSignature"
           << "(time="          << this->time
           << ", numerator="    << static_cast<i32>(numerator)
           << ", denominator="  << static_cast<i32>(denominator)
           << ", ttype="        << ttype()
           << ")";
        return ss.str();
    }
};
#undef CLASS_NAME // TimeSignature


#define CLASS_NAME KeySignature
TIME_EVENT{
    DEFAULT_METHODS
    i8 key{};
    u8 tonality{};

    static constexpr auto serialize(auto & archive, auto & event){
        return archive(event.time, event.key, event.tonality);
    }

    bool operator==(const KeySignature &other) const {
        return this->time == other.time && this->key == other.key &&
               this->tonality == other.tonality;
    }

    KeySignature(const unit time, const i8 key, const u8 tonality) :
        TimeStamp<T>(time), key(key), tonality(tonality) {};

    KeySignature(const unit time, const minimidi::message::KeySignature msg) :
        TimeStamp<T>(time), key(msg.key), tonality(msg.tonality) {};

    template<typename U>
    KeySignature(unit time, const KeySignature<U>& other):
        TimeStamp<T>(time), key(other.key), tonality(other.tonality) {};

    [[nodiscard]] std::string to_string() const {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(SS_PRECISION);
        ss << "KeySignature"
           << "(time="          << this->time
           << ", key="          << static_cast<i32>(key)
           << ", tonality="     << static_cast<i32>(tonality)
           << ", ttype="        << ttype()
           << ")";
        return ss.str();
    }

    uint8_t degree() const {
        return (key * 5) % 12 + tonality * 12;
    }
};
#undef CLASS_NAME // KeySignature

#define CLASS_NAME Tempo
TIME_EVENT{
    DEFAULT_METHODS
    f32 qpm{};

    static constexpr auto serialize(auto & archive, auto & event){
        return archive(event.time, event.qpm);
    }

    bool operator==(const Tempo &other) const {
        return this->time == other.time && this->qpm == other.qpm;
    }

    Tempo(const unit time, const f32 qpm) : TimeStamp<T>(time), qpm(qpm) {};

    template<typename U>
    Tempo(const unit time, const Tempo<U>& other): TimeStamp<T>(time), qpm(other.qpm) {};

    [[nodiscard]] std::string to_string() const {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(SS_PRECISION);
        ss << "Tempo"
           << "(time="      << this->time
           << ", qpm="      << qpm
           << ", ttype="    << ttype()
           << ")";
        return ss.str();
    }
};
#undef CLASS_NAME // Tempo

#define CLASS_NAME PitchBend
TIME_EVENT{
    DEFAULT_METHODS
    i32 value{};

    static constexpr auto serialize(auto & archive, auto & event){
        return archive(event.time, event.value);
    }

    bool operator==(const PitchBend &other) const {
        return this->time == other.time && this->value == other.value;
    }

    PitchBend(unit time, i32 value) : TimeStamp<T>(time), value(value) {};

    template<typename U>
    PitchBend(unit time, const PitchBend<U>& other): TimeStamp<T>(time), value(other.value) {};

    [[nodiscard]] std::string to_string() const {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(SS_PRECISION);
        ss << "PitchBend"
           << "(time="      << this->time
           << ", value="    << value
           << ", ttype="    << ttype()
           << ")";
        return ss.str();
    }
};
#undef CLASS_NAME // PitchBend

#define CLASS_NAME Pedal
TIME_EVENT{
    DEFAULT_METHODS
    unit duration;

    static constexpr auto serialize(auto & archive, auto & event){
        return archive(event.time, event.duration);
    }

    bool operator==(const Pedal &other) const {
        return this->time == other.time && this->duration == other.duration;
    }

    Pedal(unit time, unit duration) : TimeStamp<T>(time), duration(duration) {};

    template<typename U>
    Pedal(unit time, const Pedal<U>& other): TimeStamp<T>(time), duration(other.duration) {};

    template<typename U>
    Pedal(unit time, unit duration, const Pedal<U>& other): TimeStamp<T>(time), duration(duration) {};

    [[nodiscard]] unit end() const { return this->time + duration; }

    [[nodiscard]] std::string to_string() const {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(SS_PRECISION);
        ss << "Pedal"
           << "(time="      << this->time
           << ", duration=" << duration
           << ", ttype="    << ttype()
           << ")";
        return ss.str();
    }
};
#undef CLASS_NAME // Pedal

#define CLASS_NAME TextMeta
TIME_EVENT{
    DEFAULT_METHODS
    std::string text{};

    static constexpr auto serialize(auto & archive, auto & event){
        return archive(event.time, event.text);
    }

    bool operator==(const TextMeta &other) const {
        return this->time == other.time && this->text == other.text;
    }

    TextMeta(unit time, std::string text) : TimeStamp<T>(time), text(std::move(text)) {};

    template<typename U>
    TextMeta(unit time, const TextMeta<U>& other): TimeStamp<T>(time), text(other.text) {};

    [[nodiscard]] std::string to_string() const {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(SS_PRECISION);
        ss << "TextMeta"
           << "(time="      << this->time
           << ", text=\""   << text
           << "\", ttype="  << ttype()
           << ")";
        return ss.str();
    }
};
#undef CLASS_NAME // TextMeta
#undef TIME_EVENT
#undef DEFAULT_METHODS
#undef ZPP_INIT

namespace utils {

template<typename T>
bool time_cmp(const T &a, const T &b) {
        return a.time < b.time;
}

template<typename T>
void sort_by_time(vec<T> &events) {
    pdqsort_branchless(events.begin(), events.end(), time_cmp<T>);
}

inline void sort_msgs(message::Messages & msgs) {
    pdqsort_branchless(msgs.begin(), msgs.end(), [](const auto &a, const auto &b) {
        return a.get_time() < b.get_time();
    });
}

template<typename T>
vec<T> filter(const vec<T> &data, const std::function<bool(const T &)> & _filter) {
    if (data.empty()) return {};
    vec<T> new_data;
    new_data.reserve(data.size());
    std::copy_if(
        data.begin(), data.end(), std::back_inserter(new_data), _filter
    );
    vec<T>(new_data).swap(new_data);
    return new_data;
}

template<typename T>
vec<T> clip_by_time(const vec<T> &data, typename T::unit start, typename T::unit end) {
    std::function<bool(const T &)> func = [start, end](const T &event) {
        return (event.time >= start) && (event.time < end);
    };
    return filter(data, func);
}

template<typename T>
vec<Note<T>> clip_notes(
    const vec<Note<T>> &notes,
    typename T::unit start, typename T::unit end, bool clip_end) {
    if (clip_end) {
        std::function<bool(const Note<T> &)> func = [start, end](const Note<T> &note) {
            return (note.time >= start) && (note.time + note.duration <= end);
        };
        return utils::filter(notes, func);
    }   return clip_by_time(notes, start, end);
}

template<typename T>
vec<T> &shift_time_inplace(vec<T> &data, typename T::unit offset) {
    for (auto &event: data)
        event.shift_time_inplace(offset);
    return data;
}

template<typename T>
vec<T> shift_time(const vec<T> &data, typename T::unit offset) {
    vec<T> new_data;
    new_data.reserve(data.size());
    for (auto &event: data)
        new_data.emplace_back(event.shift_time(offset));
    return new_data;
}
} // namespace utils end

// overload operator<< and to_string for vec<T>
#define STRING_METHODS(i, CLASS_NAME)                                   \
    template <typename T>                                               \
    std::string to_string(const CLASS_NAME<T> & data) {                 \
        return data.to_string();                                        \
    }                                                                   \
    template <typename T>                                               \
    ostream& operator<<(ostream& out, const CLASS_NAME<T>& data) {      \
             out <<data.to_string(); return out;                        \
    }                                                                   \
    template <typename T>                                               \
    ostream& operator<<(ostream& out, const vec<CLASS_NAME<T>>& data) { \
        if(data.empty()){                                               \
            out << "[]"; return out;                                    \
        } size_t j = 0; out << "[";                                     \
        for(; j<data.size()-1; ++j){                                    \
            out << data[j] << ", ";                                     \
        }   out << data[j] << "]";                                      \
        return out;                                                     \
    }                                                                   \
    template<typename T>                                                \
    std::string to_string(const vec<CLASS_NAME<T>> & data){             \
        std::stringstream ss;                                           \
        ss << data;                                                     \
        return ss.str();                                                \
    }

// avoid overload other types
REPEAT_ON(
    STRING_METHODS,
    Note, ControlChange, TimeSignature, KeySignature, Tempo, PitchBend, Pedal, TextMeta
)
// overload operator<< and to_string for vec<T> end

template <typename T>
struct Track{
    typedef typename T::unit unit;

    std::string name;
    u8 program = 0;
    bool is_drum = false;
    vec<Note<T>> notes;
    vec<ControlChange<T>> controls;
    vec<PitchBend<T>> pitch_bends;
    vec<Pedal<T>> pedals;

    static constexpr auto serialize(auto & archive, auto & track){
        return archive(
            track.name, track.program, track.is_drum, track.notes,
            track.controls, track.pitch_bends, track.pedals
        );
    }

    bool operator==(const Track &other) const {
        return this->name == other.name && this->program == other.program &&
               this->is_drum == other.is_drum && this->notes == other.notes &&
               this->controls == other.controls && this->pitch_bends == other.pitch_bends &&
               this->pedals == other.pedals;
    }

    Track() = default;
    Track(const Track&) = default;

    Track(std::string name,
          const u8 program,
          const bool is_drum,
          vec<Note<T>> notes,
          vec<ControlChange<T>> controls,
          vec<PitchBend<T>> pitch_bends,
          vec<Pedal<T>> pedals):
        name(std::move(name)),
        program(program),
        is_drum(is_drum),
        notes(std::move(notes)),
        controls(std::move(controls)),
        pitch_bends(std::move(pitch_bends)),
        pedals(std::move(pedals)) {};

    Track(std::string name, u8 program, bool is_drum) :
        name(std::move(name)), program(program), is_drum(is_drum) {};

    [[nodiscard]] Track copy() const { return {*this}; }

    Track& sort_inplace() {
        utils::sort_by_time(notes);
        utils::sort_by_time(controls);
        utils::sort_by_time(pitch_bends);
        utils::sort_by_time(pedals);
        return *this;
    }

    [[nodiscard]] Track sort() const { return copy().sort_inplace(); }

    [[nodiscard]] unit end() const {
        unit t = 0;
        for (auto const &note: notes)
            t = std::max(t, note.end());
        return t;
    }

    [[nodiscard]] unit start() const {
        unit t = std::numeric_limits<unit>::max();
        for (auto const &note: notes)
            t = std::min(t, note.start());
        return t;
    }

    [[nodiscard]] size_t note_num() const { return notes.size(); }

    [[nodiscard]] bool empty() const {
        return notes.empty() && controls.empty() && pitch_bends.empty();
    }

    [[nodiscard]] Track clip(unit start, unit end, bool clip_end = false) const {
        Track new_track;
        // copy some attributes
        new_track.name = name;
        new_track.program = program;
        new_track.is_drum = is_drum;
        // clip notes
        new_track.notes = utils::clip_notes(notes, start, end, clip_end);
        // clip others
        new_track.controls = utils::clip_by_time(controls, start, end);
        new_track.pitch_bends = utils::clip_by_time(pitch_bends, start, end);
        new_track.pedals = utils::clip_by_time(pedals, start, end);
        return new_track;
    }

    [[nodiscard]] std::string to_string() const {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(SS_PRECISION);
        ss << "Track"
           << "(name=\""        << name
           << "\", program="    << static_cast<i32>(program)
           << ", is_drum="      << (is_drum ? "True" : "False")
           << ", ttype="        << T()
           << ", notes="        << notes
           << ", controls="     << controls
           << ", pitch_bends="  << pitch_bends
           << ", pedals="       << pedals
           << ")";
        return ss.str();
    }

    Track& shift_time_inplace(unit offset) {
        for(auto &note: notes)
            note.shift_time_inplace(offset);
        for(auto &control: controls)
            control.shift_time_inplace(offset);
        for(auto &pitch_bend: pitch_bends)
            pitch_bend.shift_time_inplace(offset);
        for(auto &pedal: pedals)
            pedal.shift_time_inplace(offset);
        return *this;
    }

    [[nodiscard]] Track shift_time(unit offset) const {
        return copy().shift_time_inplace(offset);
    }

    Track& shift_pitch_inplace(i8 offset) {
        for(auto &note: notes)
            note.shift_pitch_inplace(offset);
        return *this;
    }

    [[nodiscard]] Track shift_pitch(const i8 offset) const {
        return copy().shift_pitch_inplace(offset);
    }

    Track& shift_velocity_inplace(const i8 offset) {
        for(auto &note: notes)
            note.shift_velocity_inplace(offset);
        return *this;
    }

    [[nodiscard]] Track shift_velocity(const i8 offset) const {
        return copy().shift_velocity_inplace(offset);
    }

    pianoroll::TrackPianoRoll pianoroll(float quantization,
                                        const std::string& mode=std::string("both")) const {
        uint8_t modeFlag = pianoroll::mode_flag(mode);
        pianoroll::TrackPianoRoll pianoroll(modeFlag == 3 ? 2 : 1, ceil(this->end() / quantization));

        if(modeFlag == 3)
            for(auto &note: notes) {
                uint32_t start = floor(note.time / quantization);
                pianoroll.set(0,
                            start,
                            1,
                            note.pitch);
                pianoroll.set(1,
                            start,
                            std::max(1, static_cast<int>(floor(note.duration / quantization))),
                            note.pitch);
            }
        else if(modeFlag == 1)
            for(auto &note: notes)
                pianoroll.set(0,
                            floor(note.time / quantization),
                            1,
                            note.pitch);
        else if(modeFlag == 2)
            for(auto &note: notes)
                pianoroll.set(0,
                            floor(note.time / quantization),
                            std::max(1, static_cast<int>(floor(note.duration / quantization))),
                            note.pitch);

        return pianoroll;
    }
};

// get to_string and overload operator<< for Track and vec<Track>
STRING_METHODS(0, Track)
#undef STRING_METHODS

#undef STRING_METHODS

template<typename T>
struct Score{
protected:

    template<typename U>
    inline void from(const Score<U>& other);

public:
    i32 ticks_per_quarter{};
    vec<Track<T>> tracks;
    vec<TimeSignature<T>> time_signatures;
    vec<KeySignature<T>> key_signatures;
    vec<Tempo<T>> tempos;
    vec<TextMeta<T>> lyrics, markers;

    static constexpr auto serialize(auto & archive, auto & score) {
        return archive(
            score.ticks_per_quarter, score.tracks, score.time_signatures,
            score.key_signatures, score.tempos, score.lyrics, score.markers
        );
    }

    bool operator==(const Score &other) const {
        return this->ticks_per_quarter == other.ticks_per_quarter &&
               this->tracks == other.tracks &&
               this->time_signatures == other.time_signatures &&
               this->key_signatures == other.key_signatures &&
               this->tempos == other.tempos &&
               this->lyrics == other.lyrics &&
               this->markers == other.markers;
    }

    Score() = default;
    Score(const Score&) = default;
    Score(Score&&)  noexcept = default;
    explicit Score(const i32 tpq) : ticks_per_quarter(tpq) {};
    // explicit Score(auto && ...) {}
    [[nodiscard]] Score copy() const { return {*this}; }

    template<typename U>
    explicit Score(const Score<U> &other) { from(other); }

    explicit Score(const minimidi::file::MidiFile &midi);

    static Score from_file(const std::string &filename) {
        auto midi = minimidi::file::MidiFile::from_file(filename);
        return Score(midi);
    }

    static Score from_midi(minimidi::file::MidiFile &midi) {
        return Score(midi);
    }

    [[nodiscard]] minimidi::file::MidiFile to_midi() const;

    [[nodiscard]] minimidi::container::Bytes to_midi_bytes() const {
        return to_midi().to_bytes();
    };

    void dump_midi(const std::string &filename) const {
        to_midi().write_file(filename);
    }

    template<class target_ttype>
    typename target_ttype::unit convert_ttype(const typename T::unit &data) const;

    template<template<class> class Container, class target_ttype>
    Container<target_ttype> convert_ttype(const Container<T> &data) const;

    template<template<class> class Container, class target_ttype>
    vec<Container<target_ttype>> convert_ttype(const vec<Container<T>> &data) const;


    Score& sort_inplace() {
        utils::sort_by_time(time_signatures);
        utils::sort_by_time(key_signatures);
        utils::sort_by_time(tempos);
        utils::sort_by_time(lyrics);
        utils::sort_by_time(markers);
        for(auto &track: tracks) track.sort_inplace();
        return *this;
    }

    [[nodiscard]] Score sort() {
        return copy().sort_inplace();
    }

    [[nodiscard]] Score clip(typename T::unit start, typename T::unit end, bool clip_end = false) const {
        Score new_score;
        new_score.ticks_per_quarter = ticks_per_quarter;
        new_score.time_signatures = utils::clip_by_time(time_signatures, start, end);
        new_score.key_signatures = utils::clip_by_time(key_signatures, start, end);
        new_score.tempos = utils::clip_by_time(tempos, start, end);
        new_score.lyrics = utils::clip_by_time(lyrics, start, end);
        new_score.markers = utils::clip_by_time(markers, start, end);
        new_score.tracks.reserve(tracks.size());
        for(auto const &track: tracks)
            new_score.tracks.emplace_back(track.clip(start, end, clip_end));
        return new_score;
    }

    // shift time pitch velocity
    Score& shift_time_inplace(typename T::unit offset) {
        utils::shift_time_inplace(time_signatures, offset);
        utils::shift_time_inplace(key_signatures, offset);
        utils::shift_time_inplace(tempos, offset);
        utils::shift_time_inplace(lyrics, offset);
        utils::shift_time_inplace(markers, offset);
        for(auto &track: tracks) track.shift_time_inplace(offset);
        return *this;
    }

    [[nodiscard]] Score shift_time(typename T::unit offset) const {
        return copy().shift_time_inplace(offset);
    }

    Score& shift_pitch_inplace(i8 offset) {
        for(auto &track: tracks) track.shift_pitch_inplace(offset);
        return *this;
    }

    [[nodiscard]] Score shift_pitch(i8 offset) const {
        return copy().shift_pitch_inplace(offset);
    }

    Score& shift_velocity_inplace(i8 offset) {
        for(auto &track: tracks) track.shift_velocity_inplace(offset);
        return *this;
    }

    [[nodiscard]] Score shift_velocity(i8 offset) const {
        return copy().shift_velocity_inplace(offset);
    }

    [[nodiscard]] typename T::unit end() const {
        typename T::unit t = 0;
        for(auto const &track: tracks)
            t = std::max(t, track.end());
        return t;
    }

    [[nodiscard]] typename T::unit start() const {
        typename T::unit t = std::numeric_limits<typename T::unit>::max();
        for(auto const &track: tracks)
            t = std::min(t, track.start());
        return t;
    }

    [[nodiscard]] size_t note_num() const {
        size_t num = 0;
        for(auto const &track: tracks)
            num += track.note_num();
        return num;
    }

    [[nodiscard]] bool empty() const {
        bool ans = time_signatures.empty() && key_signatures.empty() &&
                   tempos.empty() && lyrics.empty() && markers.empty();
        for (auto const &track: tracks)
            ans = ans && track.empty();
        return ans;
    }

    [[nodiscard]] std::string to_string() const {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(SS_PRECISION);
        ss << "Score"
           << "(ticks_per_quarter=" << ticks_per_quarter
           << ", ttype="            << T()
           << ", time_signatures="  << time_signatures
           << ", key_signatures="   << key_signatures
           << ", tempos="           << tempos
           << ", lyrics="           << lyrics
           << ", markers="          << markers
           << ", tracks="           << tracks
           << ")";
        return ss.str();
    }

    pianoroll::ScorePianoRoll pianoroll(float quantization,
                                        const std::string& mode=std::string("both")) const {
        uint8_t modeFlag = pianoroll::mode_flag(mode);
        pianoroll::ScorePianoRoll pianoroll(modeFlag == 3 ? 2 : 1,
                                            tracks.size(),
                                            ceil(this->end() / quantization));

        if(modeFlag == 3)
            for(auto track_idx = 0; track_idx < tracks.size(); ++track_idx)
                for(auto &note: tracks[track_idx].notes) {
                    uint32_t start = floor(note.time / quantization);
                    pianoroll.set(0,
                                track_idx,
                                start,
                                1,
                                note.pitch);
                    pianoroll.set(1,
                                track_idx,
                                start,
                                std::max(1, static_cast<int>(floor(note.duration / quantization))),
                                note.pitch);
                }
        else if(modeFlag == 1)
            for(auto track_idx = 0; track_idx < tracks.size(); ++track_idx)
                for(auto &note: tracks[track_idx].notes)
                    pianoroll.set(0,
                                track_idx,
                                floor(note.time / quantization),
                                1,
                                note.pitch);
        else if(modeFlag == 2)
            for(auto track_idx = 0; track_idx < tracks.size(); ++track_idx)
                for(auto &note: tracks[track_idx].notes)
                    pianoroll.set(0,
                                track_idx,
                                floor(note.time / quantization),
                                std::max(1, static_cast<int>(floor(note.duration / quantization))),
                                note.pitch);

        return pianoroll;
    }

}; // Score end

namespace utils {
template<typename T>
struct NoteOn: public TimeStamp<T> {
    typedef typename T::unit unit;
    i8 velocity = 0;
    NoteOn() = default;
    NoteOn(unit time, i8 velocity) : TimeStamp<T>(time), velocity(velocity) {};
    [[nodiscard]] inline bool empty() const {return velocity == 0;}
    inline void reset() { this->time=0; velocity=0; }
};

template<typename T>
struct NoteOnQueue {
private:
    typedef typename T::unit unit;
    std::queue<NoteOn<T>> queue;
    NoteOn<T> _front;

public:
    NoteOnQueue() = default;
    [[nodiscard]] inline size_t size() const {
        return _front.empty() ? 0 : queue.size() + 1;
    }

    [[nodiscard]] inline bool empty() const { return _front.empty();}

    inline void emplace(unit time, i8 velocity) {
        if(_front.empty()) _front = NoteOn<T>{time, velocity};
        else queue.emplace(time, velocity);
    }

    inline void pop() {
        if (queue.empty()) _front.reset();
        else {
            _front = queue.front();
            queue.pop();
        }
    }

    [[nodiscard]] inline const NoteOn<T> &front() const { return _front; }
};

typedef std::tuple<u8, u8> TrackIdx;

template <typename T>
Track<T> &get_track(
    std::map<TrackIdx, Track<T>> &track_map,
    Track<T> (& stragglers)[16],
    const uint8_t channel, const uint8_t program,
    const size_t message_num, const bool create_new) {
    auto track_idx = std::make_tuple(channel, program);
    const auto & entry = track_map.find(track_idx);
    if (entry == track_map.end()) {
        auto &track = stragglers[channel];
        if (!create_new) return track;
        Track<T> new_track;
        if (!track.empty()) {
            new_track = Track(track); // copy from stragglers
        }
        new_track.program = program;
        new_track.is_drum = channel == 9;
        new_track.notes.reserve(message_num / 2 + 10);
        return track_map[track_idx] = new_track;
    }
    return entry->second;
}
} // namespace utils end

// Define all the convert_ttype functions in Score<T>
template<> template<> // tick -> quarter
inline Quarter::unit Score<Tick>::convert_ttype<Quarter>(const Tick::unit &data) const {
    return static_cast<f32>(data) / static_cast<f32>(this->ticks_per_quarter);
}

template<> template<> // quarter -> tick
inline Tick::unit Score<Quarter>::convert_ttype<Tick>(const Quarter::unit &data) const {
    return static_cast<i32>(std::round(data * static_cast<f32>(this->ticks_per_quarter)));
}

template<> template<> // tick -> tick
inline Tick::unit Score<Tick>::convert_ttype<Tick>(const Tick::unit &data) const {return data;}

template<> template<> // quarter -> quarter
inline Quarter::unit Score<Quarter>::convert_ttype<Quarter>(const Quarter::unit &data) const {return data;}

template<> template<> // special case for Note
[[maybe_unused]] inline Note<Quarter> Score<Tick>::convert_ttype<Note, Quarter>(const Note<Tick> &data) const {
    return {convert_ttype<Quarter>(data.time), convert_ttype<Quarter>(data.duration), data};
}

template<> template<> // special case for Note
[[maybe_unused]] inline Note<Tick> Score<Quarter>::convert_ttype<Note, Tick>(const Note<Quarter> &data) const {
    return {convert_ttype<Tick>(data.time), convert_ttype<Tick>(data.duration), data};
}

template<> template<> // special case for Pedal
[[maybe_unused]] inline Pedal<Quarter> Score<Tick>::convert_ttype<Pedal, Quarter>(const Pedal<Tick> &data) const {
    return {convert_ttype<Quarter>(data.time), convert_ttype<Quarter>(data.duration), data};
}

template<> template<> // special case for Pedal
[[maybe_unused]] inline Pedal<Tick> Score<Quarter>::convert_ttype<Pedal, Tick>(const Pedal<Quarter> &data) const {
    return {convert_ttype<Tick>(data.time), convert_ttype<Tick>(data.duration), data};
}

template<typename raw_ttype> // general case
template<template<class> class Container, class target_ttype>
inline Container<target_ttype> Score<raw_ttype>::convert_ttype(
    const Container<raw_ttype> &data) const {
    return Container<target_ttype>(convert_ttype<raw_ttype>(data.time), data);
}

template<typename raw_ttype> // vector general case
template<template<class> class Container, class target_ttype>
inline vec<Container<target_ttype>> Score<raw_ttype>::convert_ttype(
    const vec<Container<raw_ttype>> &data) const {
    vec<Container<target_ttype>> new_data;
    new_data.reserve(data.size());
    for (auto &event: data)
        new_data.emplace_back(convert_ttype<Container, target_ttype>(event));
    return new_data;
}
// convert_ttype end


// read midi file using Score<Tick>
// TODO this may not work for Second
template<typename T>
Score<T>::Score(const minimidi::file::MidiFile &midi) {
    size_t track_num = midi.track_num();
    ticks_per_quarter = midi.get_tick_per_quarter();
    // create a helper for converting tick to T
    const Score<Tick> helper(ticks_per_quarter);

    using std::cout, std::endl;
    for(const auto &midi_track: midi.tracks) {
        // (channel, program) -> Track
        std::map<utils::TrackIdx, Track<T>> track_map;
        // channel -> Track
        Track<T> stragglers[16];
        // channel -> program
        uint8_t cur_instr[16] = {0};
        std::string cur_name;
        // (channel, pitch) -> (start, velocity)
        utils::NoteOnQueue<Tick> last_note_on[16][128] = {};
        // channel -> pedal_on
        i32 last_pedal_on[16] = {-1};
        // iter midi messages in the track
        size_t message_num = midi_track.message_num();
        for(const auto &msg: midi_track.messages) {
            auto cur_tick = static_cast<i32>(msg.get_time());
            typename T::unit cur_time = helper.convert_ttype<T>(cur_tick);

            switch (msg.get_type()) {
                case (message::MessageType::NoteOn): {
                    if (uint8_t velocity = msg.get_velocity(); velocity != 0) {
                        auto pitch = msg.get_pitch();
                        if (pitch >= 128) throw std::range_error(
                            "Get pitch=" + std::to_string(static_cast<int>(pitch)));
                        if (velocity >= 128) throw std::range_error(
                            "Get velocity=" + std::to_string(static_cast<int>(velocity)));
                        last_note_on[msg.get_channel()][pitch].emplace(
                            cur_tick, static_cast<i8>(velocity));
                        break;
                    } // if velocity is zero, turn to note off case
                }
                case (message::MessageType::NoteOff): {
                    uint8_t channel = msg.get_channel();
                    uint8_t pitch = msg.get_pitch();
                    if (pitch >= 128) throw std::range_error(
                        "Get pitch=" + std::to_string((int) pitch));
                    auto &note_on_queue = last_note_on[channel][pitch];
                    auto &track = utils::get_track(
                        track_map, stragglers, channel,
                        cur_instr[channel], message_num,true);
                    if ((!note_on_queue.empty()) && (cur_tick > note_on_queue.front().time)) {
                        auto const &note_on = note_on_queue.front();
                        uint32_t duration = cur_tick - note_on.time;
                        track.notes.emplace_back(
                            // note_on.time, duration,
                            helper.convert_ttype<T>(note_on.time), 
                            helper.convert_ttype<T>(duration),
                            pitch, note_on.velocity
                        );
                        note_on_queue.pop();
                    }
                    break;
                }
                case (message::MessageType::ProgramChange): {
                    uint8_t channel = msg.get_channel();
                    uint8_t program = msg.get_program();
                    if (program >= 128) throw std::range_error(
                        "Get program=" + std::to_string((int) program));
                    cur_instr[channel] = program;
                    break;
                }
                case (message::MessageType::ControlChange): {
                    uint8_t channel = msg.get_channel();
                    uint8_t program = cur_instr[channel];
                    auto &track = utils::get_track(
                        track_map, stragglers, channel, program, message_num, false);
                    uint8_t control_number = msg.get_control_number();
                    uint8_t control_value = msg.get_control_value();
                    if (control_number >= 128) throw std::range_error(
                        "Get control_number=" + std::to_string((int) control_number));
                    if (control_value >= 128) throw std::range_error(
                        "Get control_value=" + std::to_string((int) control_value));
                    track.controls.emplace_back(
                        cur_tick, msg.get_control_number(), msg.get_control_value());
                    // Pedal Part
                    if (control_number == 64) {
                        if (control_value >= 64) {
                            if (last_pedal_on[channel] < 0) last_pedal_on[channel] = cur_tick;
                        } else {
                            if (last_pedal_on[channel] >= 0) {
                                track.pedals.emplace_back(
                                    // last_pedal_on[channel], cur_tick - last_pedal_on[channel]
                                    helper.convert_ttype<T>(last_pedal_on[channel]),
                                    helper.convert_ttype<T>(cur_tick - last_pedal_on[channel])
                                );
                                last_pedal_on[channel] = -1;
                            }
                        }
                    }
                    break;
                }
                case (message::MessageType::PitchBend): {
                    uint8_t channel = msg.get_channel();
                    uint8_t program = cur_instr[channel];
                    auto &track = utils::get_track(
                        track_map, stragglers, channel, program, message_num, false);
                    auto value = msg.get_pitch_bend();
                    if (value < minimidi::message::MIN_PITCHBEND || value > minimidi::message::MAX_PITCHBEND)
                        throw std::range_error("Get pitch_bend=" + std::to_string((int) value));
                    track.pitch_bends.emplace_back(
                        // cur_tick, value
                        cur_time, value
                    );
                    break;
                }
                    // Meta Message
                case (message::MessageType::Meta): {
                    switch (msg.get_meta_type()) {
                        case (message::MetaType::TrackName): {
                            auto data = msg.get_meta_value();
                            auto tmp = std::string(data.begin(), data.end());
                            cur_name = utils::strip_non_utf_8(&tmp);
                            break;
                        }
                        case (message::MetaType::TimeSignature): {
                            time_signatures.emplace_back(cur_time, msg.get_time_signature());
                            break;
                        }
                        case (message::MetaType::SetTempo): {
                            tempos.emplace_back(cur_time, 60000000.f / static_cast<float>(msg.get_tempo()));
                            break;
                        }
                        case (message::MetaType::KeySignature): {
                            key_signatures.emplace_back(cur_time, msg.get_key_signature());
                            break;
                        }
                        case (message::MetaType::Lyric): {
                            auto data = msg.get_meta_value();
                            auto tmp = std::string(data.begin(), data.end());
                            auto text = utils::strip_non_utf_8(&tmp);
                            if (text.empty()) break;
                            lyrics.emplace_back(cur_time, std::move(text));
                            break;
                        }
                        case (message::MetaType::Marker): {
                            auto data = msg.get_meta_value();
                            auto tmp = std::string(data.begin(), data.end());
                            auto text = utils::strip_non_utf_8(&tmp);
                            if (text.empty()) break;
                            markers.emplace_back(cur_time, std::move(text));
                            break;
                        }
                        default:
                            break;
                    }
                    break;
                }
                default:
                    break;
            }
        }
        for (auto [_, track]: std::move(track_map)) {
            if (track.empty()) continue;
            track.name = cur_name;
            track.notes.shrink_to_fit();
            tracks.push_back(std::move(track));
        }
    }
    utils::sort_by_time(time_signatures);
    utils::sort_by_time(key_signatures);
    utils::sort_by_time(tempos);
    utils::sort_by_time(lyrics);
    utils::sort_by_time(markers);
}

// define score -> score
template<typename T>
template<typename U>
void Score<T>::from(const Score<U> &other) {
    if(other.ticks_per_quarter <= 0)
        throw std::invalid_argument("ticks_per_quarter must be positive, please specify it first!");

    this->ticks_per_quarter = other.ticks_per_quarter;
    this->time_signatures = other.template convert_ttype<TimeSignature, T>(other.time_signatures);
    this->key_signatures = other.template convert_ttype<KeySignature, T>(other.key_signatures);
    this->tempos = other.template convert_ttype<Tempo, T>(other.tempos);
    this->lyrics = other.template convert_ttype<TextMeta, T>(other.lyrics);
    this->markers = other.template convert_ttype<TextMeta, T>(other.markers);
    this->tracks.reserve(other.tracks.size());
    for (const Track<U> &track: other.tracks){
        this->tracks.emplace_back(
            track.name,
            track.program,
            track.is_drum,
            other.template convert_ttype<Note, T>(track.notes),
            other.template convert_ttype<ControlChange, T>(track.controls),
            other.template convert_ttype<PitchBend, T>(track.pitch_bends),
            other.template convert_ttype<Pedal, T>(track.pedals)
        );
    }
}

template<typename T>
minimidi::file::MidiFile Score<T>::to_midi() const {
    minimidi::file::MidiFile midi{
        minimidi::file::MidiFormat::MultiTrack, 0,
        static_cast<u16>(ticks_per_quarter)
    };
    midi.tracks.reserve(tracks.size() + 1);
    {   // add meta messages
        message::Messages msgs{};
        msgs.reserve(time_signatures.size() + key_signatures.size() + tempos.size() + lyrics.size() + markers.size() + 10);
        // add time signatures
        for(const auto &time_signature: time_signatures) {
            msgs.emplace_back(message::Message::TimeSignature(
                this->convert_ttype<Tick>(time_signature.time),
                time_signature.numerator, time_signature.denominator
            ));
        }
        // add key signatures
        for(const auto &key_signature: key_signatures) {
            msgs.emplace_back(message::Message::KeySignature(
                this->convert_ttype<Tick>(key_signature.time),
                key_signature.key, key_signature.tonality
            ));
        }
        // add tempos
        for(const auto &tempo: tempos) {
            msgs.emplace_back(message::Message::SetTempo(
                this->convert_ttype<Tick>(tempo.time),
                static_cast<u32>(60000000.f / tempo.qpm)
            ));
        }
        // add lyrics
        for(const auto &lyric: lyrics) {
            msgs.emplace_back(message::Message::Lyric(
                this->convert_ttype<Tick>(lyric.time), lyric.text
            ));
        }
        // add markers
        for(const auto &marker: markers) {
            msgs.emplace_back(message::Message::Marker(
                this->convert_ttype<Tick>(marker.time), marker.text
            ));
        }
        utils::sort_msgs(msgs);
        midi.tracks.emplace_back(std::move(msgs));
    }

    for(const auto &track: tracks) {
        message::Messages msgs{};
        msgs.reserve(track.note_num() * 2 + track.controls.size() + track.pitch_bends.size() + 10);
        u8 channel = track.is_drum ? 9 : 0;
        // add track name
        if(!track.name.empty())
            msgs.emplace_back(message::Message::TrackName(0, track.name));
        // add program change
        msgs.emplace_back(message::Message::ProgramChange(0, channel, track.program));
        // Todo add check for Pedal
        // add control change
        for(const auto &control: track.controls) {
            msgs.emplace_back(message::Message::ControlChange(
                this->convert_ttype<Tick>(control.time), channel,
                control.number, control.value
            ));
        }
        // add pitch bend
        for(const auto &pitch_bend: track.pitch_bends) {
            msgs.emplace_back(message::Message::PitchBend(
                this->convert_ttype<Tick>(pitch_bend.time), channel,
                pitch_bend.value
            ));
        }
        // add notes
        for(const auto &note: track.notes) {
            msgs.emplace_back(message::Message::NoteOn(
                this->convert_ttype<Tick>(note.time), channel,
                note.pitch, note.velocity
            ));
            msgs.emplace_back(message::Message::NoteOff(
                this->convert_ttype<Tick>(note.end()), channel,
                note.pitch, note.velocity
            ));
        }
        utils::sort_msgs(msgs);
        midi.tracks.emplace_back(std::move(msgs));
    }
    return midi;
} // Score<T>::to_midi end
} // namespace score end
#endif //SCORE_HPP