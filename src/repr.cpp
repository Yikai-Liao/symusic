#include "fmt/core.h"

#include "symusic/event.h"
#include "symusic/track.h"
#include "symusic/score.h"
#include "symusic/repr.h"

namespace symusic {

// Implement to_string for simple events using std::format
// And explicitly instantiate them
#define EVENT_TO_STRING(__COUNT, NAME)                     \
    template<TType T>                                      \
    std::string NAME<T>::to_string() const {               \
        return fmt::format("{:d}", *this);                 \
    }                                                      \
    template std::string NAME<Tick>::to_string() const;    \
    template std::string NAME<Quarter>::to_string() const; \
    template std::string NAME<Second>::to_string() const;

REPEAT_ON(
    EVENT_TO_STRING,
    Note,
    Pedal,
    ControlChange,
    TimeSignature,
    KeySignature,
    Tempo,
    PitchBend,
    TextMeta
)

}   // namespace symusic

// Implement ostream operator for all the classes and corresponding vectors
// And explicitly instantiate them
#define OSTREAMEABLE(__COUNT, STRUCT_NAME)                                            \
    template<symusic::TType T>                                                        \
    std::ostream& operator<<(std::ostream& os, const symusic::STRUCT_NAME<T>& data) { \
        return os << data.to_string();                                                \
    }                                                                                 \
    template std::ostream& operator<< <symusic::Tick>(                                \
        std::ostream & os, const symusic::STRUCT_NAME<symusic::Tick>& data            \
    );                                                                                \
    template std::ostream& operator<< <symusic::Quarter>(                             \
        std::ostream & os, const symusic::STRUCT_NAME<symusic::Quarter>& data         \
    );                                                                                \
    template std::ostream& operator<< <symusic::Second>(                              \
        std::ostream & os, const symusic::STRUCT_NAME<symusic::Second>& data          \
    );

REPEAT_ON(
    OSTREAMEABLE,
    Note,
    Pedal,
    ControlChange,
    TimeSignature,
    KeySignature,
    Tempo,
    PitchBend,
    TextMeta,
    Track,
    Score
)
