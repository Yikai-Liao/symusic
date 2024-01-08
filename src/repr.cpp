//
// Created by lyk on 23-12-14.
//

#include "fmt/core.h"

#include "symusic/event.h"
#include "symusic/track.h"
#include "symusic/score.h"
// #include "symusic/note_arr.h"
#include "symusic/repr.h"

namespace symusic {

// Implement to_string for simple events using std::format
// And explicitly instantiate them
#define EVENT_TO_STRING(__COUNT, NAME)                      \
    template<TType T>                                       \
    std::string NAME<T>::to_string() const {                \
        return fmt::format("{:d}", *this);                  \
    }                                                       \
    template std::string NAME<Tick>::to_string() const;     \
    template std::string NAME<Quarter>::to_string() const;  \
    template std::string NAME<Second>::to_string() const;

REPEAT_ON(
    EVENT_TO_STRING,
    Note, Pedal, ControlChange, TimeSignature, KeySignature, Tempo, PitchBend, TextMeta
)
// template <TType T>
// std::string Note<T>::to_string() const { return fmt::format("{:d}", *this); }
//
// template std::string Note<Tick>::to_string() const;
// template std::string Note<Quarter>::to_string() const;
// template std::string Note<Second>::to_string() const;

// #undef EVENT_TO_STRING

// Implement summary for Track, NoteArr and Score
template<TType T>
std::string Track<T>::summary() const {
    return fmt::format(
        "Track(ttype={}, program={}, is_drum={}, name={}, notes={})",
        T(), program, is_drum, name, notes.size()
    );
}

// template<TType T>
// std::string NoteArr<T>::summary() const {
//     return fmt::format(
//         "NoteArr(ttype={}, program={}, is_drum={}, name={}, notes={})",
//         T(), program, is_drum, name, pitch.size()
//     );
// }

template<TType T>
std::string Score<T>::summary() const {
    return fmt::format(
        "Score(ttype={}, tpq={}, begin={}, end={}, tracks={}, notes={}, time_sig={}, key_sig={}, markers={}, lyrics={})",
        T(), ticks_per_quarter, start(), end(), tracks.size(), note_num(),
        time_signatures.size(), key_signatures.size(), markers.size(), lyrics.size()
    );
}

// Explicitly instantiate them
#define INSTANTIATE_SUMMARY(_COUNT, NAME)                           \
    template std::string NAME<Tick>::summary() const;               \
    template std::string NAME<Quarter>::summary() const;            \
    template std::string NAME<Second>::summary() const;             \
    template<TType T>                                               \
    std::string NAME<T>::to_string() const { return summary(); }    \
    template std::string NAME<Tick>::to_string() const;             \
    template std::string NAME<Quarter>::to_string() const;          \
    template std::string NAME<Second>::to_string() const;

REPEAT_ON(
    INSTANTIATE_SUMMARY,
    Track, Score
)

#undef INSTANTIATE_SUMMARY

}

// Implement ostream operator for all the classes and corresponding vectors
// And explicitly instantiate them
#define OSTREAMEABLE(__COUNT, STRUCT_NAME)                                              \
    template<symusic::TType T>                                                          \
    std::ostream &operator<<(std::ostream &os, const symusic::STRUCT_NAME<T> &data){    \
        return os << data.to_string();                                                  \
    }                                                                                   \
    template std::ostream & operator<< <symusic::Tick>(                                 \
        std::ostream &os, const symusic::STRUCT_NAME<symusic::Tick> &data               \
    );                                                                                  \
    template std::ostream & operator<< <symusic::Quarter>(                              \
        std::ostream &os, const symusic::STRUCT_NAME<symusic::Quarter> &data            \
    );                                                                                  \
    template std::ostream & operator<< <symusic::Second>(                               \
        std::ostream &os, const symusic::STRUCT_NAME<symusic::Second> &data             \
    );

REPEAT_ON(
    OSTREAMEABLE,
    Note, Pedal, ControlChange, TimeSignature, KeySignature, Tempo, PitchBend, TextMeta,
    Track, Score
)
