#include "../core/binding_prelude.h"

#include "event_bindings.h"

namespace symusic::py {

void bind_controller_family_types(nb::module_& m) {
    bind_event<Tempo>(
        m,
        "Tempo",
        "TempoList",
        "Tempo(time, qpm=None, microseconds_per_quarter=None, *, ttype=Tick)\n\nA tempo change.",
        event_indices<Tempo>,
        [](auto& type) {
            type.def(
                    "__init__",
                    [](UnitValue<Tempo>*  self,
                       nb::handle         time,
                       std::optional<f64> qpm,
                       nb::handle         microseconds_per_quarter,
                       nb::handle         marker) {
                        new (self) UnitValue<Tempo>(with_unit(marker, [&](auto unit) {
                            using T    = decltype(unit);
                            using E = Tempo<T>;
                            const auto start = event_input<E, 0>(time);
                            if (!qpm && microseconds_per_quarter.is_none())
                                throw std::invalid_argument("qpm or microseconds_per_quarter must be specified");
                            auto event = std::make_shared<E>(qpm ? E::from_qpm(start, *qpm)
                                : detail::make_event<E>(start, event_input<E, 1>(microseconds_per_quarter)));
                            return UnitValue<Tempo>(std::move(event));
                        }));
                    },
                    "time"_a,
                    "qpm"_a  = nb::none(),
                    "microseconds_per_quarter"_a = nb::none(),
                    nb::kw_only(),
                    "ttype"_a = nb::type<Tick>(),
                    kEventTempoInitDoc
            )
                .def_prop_rw(
                    "qpm",
                    [](const UnitValue<Tempo>& self) {
                        return self.visit([](const auto& ptr) { return ptr->qpm(); });
                    },
                    [](const UnitValue<Tempo>& self, f64 value) {
                        self.visit([&](const auto& ptr) { ptr->set_qpm(value); });
                    },
                    kEventTempoQpmDoc
                );
        }
    );
    bind_event<ControlChange>(
        m,
        "ControlChange",
        "ControlChangeList",
        "ControlChange(time, number, value, *, ttype=Tick)\n\nA MIDI control change.",
        "time"_a,
        "number"_a,
        "value"_a
    );
    bind_event<Pedal>(
        m,
        "Pedal",
        "PedalList",
        "Pedal(time, duration, *, ttype=Tick)\n\nA sustain pedal region.",
        "time"_a,
        "duration"_a
    );
    bind_event<PitchBend>(
        m,
        "PitchBend",
        "PitchBendList",
        "PitchBend(time, value, *, ttype=Tick)\n\nA MIDI pitch bend.",
        "time"_a,
        "value"_a
    );
}

}   // namespace symusic::py
