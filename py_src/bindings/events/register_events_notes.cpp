//
// Note, key signature, time signature and text bindings; see register_events.cpp.
//
#include "../core/binding_prelude.h"

#include "../core/binding_common.h"
#include "event_bindings.h"

namespace nb = nanobind;

namespace symusic::py {

void bind_note_family_types(nb::module_& m) {
    bind_units(m, []<TType T>(nb::module_& module, const std::string& name) {
        bind_note<T>(module, name);
    });
    bind_units(m, []<TType T>(nb::module_& module, const std::string& name) {
        bind_keysig<T>(module, name);
    });
    bind_units(m, []<TType T>(nb::module_& module, const std::string& name) {
        bind_timesig<T>(module, name);
    });
    bind_units(m, []<TType T>(nb::module_& module, const std::string& name) {
        bind_textmeta<T>(module, name);
    });

}

}   // namespace symusic::py
