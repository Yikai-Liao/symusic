#pragma once

#include "utils/python_helpers.h"

namespace symusic::py {

template<class Value, class Field>
void bind_scalar_property(nb::class_<Value>& type, const char* name, Field field, const char* doc) {
    type.def_prop_rw(
        name,
        [field](const Value& self) {
            return self.visit([&](const auto& ptr) { return field(*ptr); });
        },
        [field](
            const Value& self,
            const std::remove_cvref_t<decltype(field(*std::declval<Value>().template get<Tick>()))>&
                value
        ) { self.visit([&](const auto& ptr) { field(*ptr) = value; }); },
        doc
    );
}

template<class Roll, class Value>
void bind_pianoroll(nb::class_<Value>& type, const char* doc) {
    type.def(
        "pianoroll",
        [](const Value&            self,
           const vec<std::string>& modes,
           std::pair<i64, i64>     range,
           bool                    encode_velocity) {
            const auto& ptr = self.template get<Tick>();
            if (range.first < 0 || range.first >= range.second || range.second > 128) {
                throw std::invalid_argument("pitch_range must be within [0, 128] with low < high");
            }
            vec<PianorollMode> mode_enums;
            mode_enums.reserve(modes.size());
            for (const auto& mode : modes) mode_enums.push_back(str_to_pianoroll_mode(mode));
            const auto pitch_range
                = std::pair(static_cast<u8>(range.first), static_cast<u8>(range.second));
            auto roll = [&]() {
                if constexpr (std::same_as<Roll, ScorePianoroll>) {
                    return Roll::from_score(*ptr, mode_enums, pitch_range, encode_velocity);
                } else {
                    return Roll::from_track(*ptr, mode_enums, pitch_range, encode_velocity);
                }
            }();
            return owned_array(
                std::make_unique<Roll>(std::move(roll)),
                [](const auto& data, const nb::capsule& owner) {
                    return std::apply(
                        [&](auto... dims) {
                            return nb::ndarray<nb::numpy, pianoroll_t>(
                                const_cast<pianoroll_t*>(data.data()), {dims...}, owner
                            );
                        },
                        data.dims()
                    );
                }
            );
        },
        "modes"_a           = vec<std::string>{"frame", "onset"},
        "pitch_range"_a     = std::pair<i64, i64>(0, 128),
        "encode_velocity"_a = false,
        doc
    );
}

template<class Value>
void bind_transformations(
    nb::class_<Value>& type,
    const char*        sort_doc,
    const char*        clip_doc,
    const char*        adjust_doc,
    const char*        time_doc,
    const char*        pitch_doc,
    const char*        velocity_doc
) {
    type.def(
            "sort",
            [](const Value& self, bool reverse) {
                self.visit([&](const auto& ptr) { ptr->sort(reverse); });
            },
            "reverse"_a = false,
            nb::sig("def sort(self, reverse: bool = False) -> None"),
            sort_doc
    )
        .def(
            "clip",
            [](const Value& self, nb::handle start, nb::handle end, bool clip_end) {
                return self.visit([&](const auto& ptr) {
                    using T = UnitOf<decltype(ptr)>;
                    return Value(
                        std::make_shared<typename std::remove_cvref_t<decltype(ptr)>::element_type>(
                            ptr->clip(read_time<T>(start), read_time<T>(end), clip_end)
                        )
                    );
                });
            },
            "start"_a,
            "end"_a,
            "clip_end"_a = false,
            nb::sig(
                "def clip(self, start: int | float, end: int | float, clip_end: bool = False) -> "
                "typing.Self"
            ),
            clip_doc
        )
        .def(
            "adjust_time",
            [](const Value&        self,
               const nb::iterable& original_times,
               const nb::iterable& new_times) {
                self.visit([&](const auto& ptr) {
                    using T = UnitOf<decltype(ptr)>;
                    ops::adjust_time(*ptr, read_times<T>(original_times), read_times<T>(new_times));
                });
            },
            "original_times"_a,
            "new_times"_a,
            nb::sig(
                "def adjust_time(self, original_times: collections.abc.Iterable[int | float], "
                "new_times: collections.abc.Iterable[int | float]) -> None"
            ),
            adjust_doc
        )
        .def(
            "shift_time",
            [](const Value& self, nb::handle offset) {
                self.visit([&](const auto& ptr) {
                    ptr->shift_time(read_shift_offset<UnitOf<decltype(ptr)>>(offset));
                });
            },
            "offset"_a,
            nb::sig("def shift_time(self, offset: int | float) -> None"),
            time_doc
        )
        .def(
            "shift_pitch",
            [](const Value& self, nb::handle offset) {
                self.visit([&](const auto& ptr) {
                    ptr->shift_pitch(read_int32(offset, "offset"));
                });
            },
            "offset"_a,
            nb::sig("def shift_pitch(self, offset: int) -> None"),
            pitch_doc
        )
        .def(
            "shift_velocity",
            [](const Value& self, nb::handle offset) {
                self.visit([&](const auto& ptr) {
                    ptr->shift_velocity(read_int32(offset, "offset"));
                });
            },
            "offset"_a,
            nb::sig("def shift_velocity(self, offset: int) -> None"),
            velocity_doc
        );
}

template<class Value>
void bind_queries(
    nb::class_<Value>& type, const char* start_doc, const char* end_doc, const char* empty_doc
) {
    type.def(
            "start",
            [](const Value& self) {
                return self.visit([](const auto& ptr) { return TimeNumber(ptr->start()); });
            },
            start_doc
    )
        .def(
            "end",
            [](const Value& self) {
                return self.visit([](const auto& ptr) { return TimeNumber(ptr->end()); });
            },
            end_doc
        )
        .def(
            "empty",
            [](const Value& self) {
                return self.visit([](const auto& ptr) { return ptr->empty(); });
            },
            empty_doc
        );
}

}   // namespace symusic::py
