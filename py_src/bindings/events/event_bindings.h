#pragma once

#include <new>
#include <optional>
#include <stdexcept>

#include "symusic/detail/event_fields.h"
#include "symusic/soa.h"
#include "utils/vector_bindings.h"

namespace symusic::py {

constexpr const char* kEventShiftTimeDoc
    = R"pbdoc(Add *offset* to ``time``.)pbdoc";
constexpr const char* kEventVecNumpyDoc
    = R"pbdoc(Return a dict with one NumPy array per field. The arrays are copies.)pbdoc";
constexpr const char* kEventVecFromNumpyDoc
    = R"pbdoc(Build a list from one NumPy array per field; the arrays must have equal length.)pbdoc";
constexpr const char* kEventTextMetaNumpyDoc
    = R"pbdoc(Not supported for text events; raises ``RuntimeError``.)pbdoc";

constexpr const char* kEventNoteEmptyDoc
    = R"pbdoc(Return whether the duration or the velocity is 0 or less.)pbdoc";
constexpr const char* kEventEndDoc
    = R"pbdoc(``time + duration``. Setting it changes ``duration``.)pbdoc";
constexpr const char* kEventNoteShiftPitchDoc    = R"pbdoc(
Add *offset* to ``pitch``. Raises ``ValueError`` when
the result leaves 0..127.
)pbdoc";
constexpr const char* kEventNoteShiftVelocityDoc = R"pbdoc(
Add *offset* to ``velocity``. Raises ``ValueError``
when the result leaves 0..127.
)pbdoc";

constexpr const char* kEventTempoInitDoc = R"pbdoc(
Create a tempo change from *qpm* or *microseconds_per_quarter*; *qpm* wins when both are given. Raises ``ValueError``
when neither is given.
)pbdoc";
constexpr const char* kEventTempoQpmDoc  = R"pbdoc(
Quarter notes per minute, computed from ``microseconds_per_quarter``. Setting it rounds ``microseconds_per_quarter`` to the nearest
microsecond.
)pbdoc";

template<TimeEvent E, size_t I>
constexpr bool time_field = I == 0 || (HashDuration<E> && I == 1);

template<template<class> class Event, size_t I>
using EventArg = std::conditional_t<
    std::same_as<detail::EventField<Event<Tick>, I>, std::string>, std::string, nb::handle
>;

template<TimeEvent E, size_t I>
auto event_input(const auto& value) {
    using F = detail::EventField<E, I>;
    const auto name = std::get<I>(detail::event_fields<E>).first;
    if constexpr (std::same_as<F, bool>) {
        if (!PyBool_Check(value.ptr())) throw nb::type_error("minor must be a bool");
        return value.ptr() == Py_True;
    } else if constexpr (std::integral<F>) {
        return detail::checked_event_field<E, I>(read_integer(value, name));
    } else if constexpr (std::floating_point<F>) {
        double number;
        if (!nb::try_cast(value, number)) throw nb::type_error("Expected a numeric time");
        return detail::checked_event_field<E, I>(number);
    } else {
        return value;
    }
}

template<TimeEvent E, size_t I>
const std::string& event_field_doc() {
    using F = detail::EventField<E, I>;
    static const auto doc = [] {
        if constexpr (std::same_as<F, bool>) {
            return std::string("True for minor and False for major.");
        } else if constexpr (std::same_as<F, std::string>) {
            return std::string("Text stored in the event.");
        } else if constexpr (I == 0) {
            return std::string("Event time in the object's unit, within its finite scalar range.");
        } else if constexpr (HashDuration<E> && I == 1) {
            return std::string("Non-negative finite duration in the event's time unit.");
        } else {
            const auto [low, high] = detail::field_range<E, I>();
            return std::string("Allowed values are ") + std::to_string(low) + ".."
                + std::to_string(high) + (detail::power_of_two_field<E, I>
                    ? ", restricted to powers of two." : ".");
        }
    }();
    return doc;
}

// Native Py_tp_getset descriptor for Note.time/duration (measured ~0.89x of
// generic nanobind property access in tests/bench). A native descriptor already
// checks the receiver type; check nanobind's construction state before accessing
// its C++ value.
template<size_t I>
struct NoteTimeField {
    using Value = UnitValue<Note>;

    static PyObject* get(PyObject* self, void*) noexcept {
        const auto* note = ready_note(self);
        if (!note) return nullptr;
        return note->visit([](const auto& ptr) {
            using T          = UnitOf<decltype(ptr)>;
            const auto value = (*ptr).*std::get<I>(detail::event_fields<Note<T>>).second;
            if constexpr (std::same_as<T, Tick>)
                return PyLong_FromLong(value);
            else
                return PyFloat_FromDouble(value);
        });
    }

    static int set(PyObject* self, PyObject* value, void*) noexcept {
        const auto* note = ready_note(self);
        if (!note) return -1;
        if (!value) {
            PyErr_SetString(PyExc_AttributeError, "Cannot delete a note time field");
            return -1;
        }
        try {
            note->visit([&](const auto& ptr) {
                using T                                                   = UnitOf<decltype(ptr)>;
                (*ptr).*std::get<I>(detail::event_fields<Note<T>>).second = event_input<Note<T>, I>(nb::handle(value));
            });
            return 0;
        } catch (nb::python_error& error) {
            error.restore();
        } catch (const nb::builtin_exception& error) {
            PyErr_SetString(builtin_exception_type(error.type()), error.what());
        } catch (const std::invalid_argument& error) {
            PyErr_SetString(PyExc_ValueError, error.what());
        } catch (const std::overflow_error& error) {
            PyErr_SetString(PyExc_OverflowError, error.what());
        } catch (const std::bad_alloc&) { PyErr_NoMemory(); } catch (const std::exception& error) {
            PyErr_SetString(PyExc_RuntimeError, error.what());
        } catch (...) { PyErr_SetString(PyExc_RuntimeError, "Unexpected C++ exception"); }
        return -1;
    }

private:
    static PyObject* builtin_exception_type(nb::exception_type kind) noexcept {
        switch (kind) {
        case nb::exception_type::type_error: return PyExc_TypeError;
        case nb::exception_type::value_error: return PyExc_ValueError;
        case nb::exception_type::index_error: return PyExc_IndexError;
        case nb::exception_type::key_error: return PyExc_KeyError;
        case nb::exception_type::attribute_error: return PyExc_AttributeError;
        default: return PyExc_RuntimeError;
        }
    }

    static const Value* ready_note(PyObject* self) noexcept {
        if (nb::inst_ready(self)) return nb::inst_ptr<Value>(self);
        PyErr_SetString(PyExc_TypeError, "Note is not initialized");
        return nullptr;
    }
};

template<size_t I>
constexpr PyGetSetDef note_time_field{
    std::get<I>(detail::event_fields<Note<Tick>>).first,
    NoteTimeField<I>::get,
    NoteTimeField<I>::set,
    const_cast<char*>(I == 0 ? "Event time in the object's unit." : "Non-negative finite duration in the event's time unit."),
    nullptr,
};

template<template<class> class Event, size_t I>
void bind_event_field(
    nb::class_<UnitValue<Event>>& type,
    const char*                   name = std::get<I>(detail::event_fields<Event<Tick>>).first
) {
    using Value = UnitValue<Event>;
    type.def_prop_rw(
        name,
        [](const Value& self) {
            return self.visit([](const auto& ptr) {
                using E    = Event<UnitOf<decltype(ptr)>>;
                auto value = (*ptr).*std::get<I>(detail::event_fields<E>).second;
                if constexpr (time_field<E, I>)
                    return TimeNumber(value);
                else
                    return value;
            });
        },
        [](const Value& self, const EventArg<Event, I>& value) {
            self.visit([&](const auto& ptr) {
                using T                                                    = UnitOf<decltype(ptr)>;
                (*ptr).*std::get<I>(detail::event_fields<Event<T>>).second = event_input<Event<T>, I>(value);
            });
        },
        event_field_doc<Event<Tick>, I>().c_str()
    );
}

template<template<class> class Event, size_t... I, class... Args>
void bind_event_init(nb::class_<UnitValue<Event>>& type, std::index_sequence<I...>, Args... args) {
    using Value = UnitValue<Event>;
    type.def(
        "__init__",
        [](Value* self, EventArg<Event, I>... values, nb::handle marker) {
            new (self) Value(with_unit(marker, [&](auto unit) {
                using T = decltype(unit);
                using E = Event<T>;
                return Value(std::make_shared<E>(detail::make_event<E>(event_input<E, I>(values)...)));
            }));
        },
        args...,
        nb::kw_only(),
        "ttype"_a = nb::type<Tick>()
    );
}

template<class Scalar>
using Column = nb::ndarray<Scalar, nb::ndim<1>, nb::c_contig, nb::device::cpu>;

template<class Scalar>
struct InputColumn {
    Column<Scalar> native;
    vec<Scalar> converted;

    size_t size() const { return native.is_valid() ? native.size() : converted.size(); }
    Scalar operator()(size_t row) const {
        return native.is_valid() ? native(row) : converted[row];
    }
};

template<TimeEvent E, size_t I>
auto read_column(nb::handle input) {
    using F = detail::EventField<E, I>;
    InputColumn<F> column;
    if (nb::try_cast(input, column.native, false)) return column;

    // Coercing directly to the target NumPy dtype could wrap an integer before validation.
    using Array = nb::ndarray<nb::ndim<1>, nb::device::cpu>;
    const auto source = nb::cast<Array>(input, false);
    column.converted.reserve(source.size());
    for (nb::handle item : nb::borrow<nb::iterable>(input)) {
        if constexpr (std::integral<F> && !std::same_as<F, bool>) {
            if (PyIndex_Check(item.ptr())) {
                column.converted.push_back(event_input<E, I>(item));
            } else {
                f64 number;
                if (!nb::try_cast(item, number)) throw nb::type_error("Expected a numeric column");
                column.converted.push_back(detail::checked_event_field<E, I>(number));
            }
        } else if constexpr (std::same_as<F, bool>) {
            const auto number = read_integer(item, std::get<I>(detail::event_fields<E>).first);
            if (number != 0 && number != 1) throw std::invalid_argument("minor must be boolean");
            column.converted.push_back(number == 1);
        } else {
            column.converted.push_back(event_input<E, I>(item));
        }
    }
    return column;
}

template<TimeEvent E, size_t... I>
auto event_list_from_numpy(const auto& arrays, std::index_sequence<I...>) {
    auto columns
        = std::make_tuple(read_column<E, I>(std::get<I>(arrays))...);
    const auto size = std::get<0>(columns).size();
    if (!((std::get<I>(columns).size() == size) && ...)) {
        throw std::invalid_argument("Event columns must have the same size");
    }
    vec<E> events;
    events.reserve(size);
    for (size_t row = 0; row < size; ++row) {
        events.push_back(detail::make_event<E>(detail::checked_event_field<E, I>(std::get<I>(columns)(row))...));
    }
    return SharedStorage::adopt(std::move(events));
}

template<template<class> class Event, class Bound, size_t... I>
void bind_from_numpy(nb::class_<Bound>& type, std::index_sequence<I...> fields) {
    using List = typename EventSequence<Event>::Value;
    type.def_static(
        "from_numpy",
        [fields](decltype((void(I), nb::handle()))... columns, nb::handle marker) {
            const auto arrays = std::tie(columns...);
            return with_unit(marker, [&](auto unit) {
                return List(event_list_from_numpy<Event<decltype(unit)>>(arrays, fields));
            });
        },
        nb::arg(std::get<I>(detail::event_fields<Event<Tick>>).first)...,
        nb::kw_only(),
        "ttype"_a = nb::type<Tick>(),
        kEventVecFromNumpyDoc
    );
}

template<template<class> class Event, size_t... I>
auto bind_event(
    nb::module_&              module,
    const char*               name,
    const char*               list_name,
    const char*               doc,
    std::index_sequence<I...> fields,
    auto                      bind_init
) {
    using Value = UnitValue<Event>;
    using List  = typename EventSequence<Event>::Value;
    auto type   = [&] {
        if constexpr (std::same_as<Event<Tick>, Note<Tick>>) {
            // The descriptors retain these definitions for the lifetime of the class.
            static PyGetSetDef fields[] = {note_time_field<0>, note_time_field<1>, {}};
            PyType_Slot        slots[]  = {{Py_tp_getset, fields}, {0, nullptr}};
            return unit_class<Value>(module, name, doc, nullptr, nb::type_slots(slots));
        } else {
            return unit_class<Value>(module, name, doc);
        }
    }();
    const auto list_doc
        = std::string(list_name)
          + "(items=None, *, ttype=Tick)\n\nA mutable list of events in one time unit.";
    const auto base = std::string("collections.abc.MutableSequence[") + name + "[_Unit]]";
    auto       list = unit_class<List>(module, list_name, list_doc.c_str(), base.c_str());
    (
        [&] {
            if constexpr (!std::same_as<Event<Tick>, Note<Tick>> || !time_field<Event<Tick>, I>)
                bind_event_field<Event, I>(type);
        }(),
        ...);
    type.attr("__match_args__")
        = nb::make_tuple(std::get<I>(detail::event_fields<Event<Tick>>).first...);
    bind_init(type);
    type.def(
        "__init__",
        [](Value* self, const Value& other) {
            new (self) Value(other.visit([](const auto& ptr) {
                using E = Event<UnitOf<decltype(ptr)>>;
                return Value(std::make_shared<E>(*ptr));
            }));
        },
        "other"_a
    );
    bind_object_ops(type, [](const auto& item) {
        return std::make_shared<std::decay_t<decltype(item)>>(item);
    });
    type.def(
        "shift_time",
        [](const Value& self, nb::handle offset) {
            self.visit( [&](const auto& ptr) {
                using T     = UnitOf<decltype(ptr)>;
                symusic::shift_time(*ptr, read_time<T>(offset));
            });
        },
        "offset"_a,
        nb::sig("def shift_time(self, offset: int | float) -> None"),
        kEventShiftTimeDoc
    );

    if constexpr (HashDuration<Event<Tick>>) {
        type.def_prop_rw(
            "end",
            [](const Value& self) {
                return self.visit([](const auto& ptr) { return TimeNumber(ptr->end()); });
            },
            [](const Value& self, nb::handle value) {
                self.visit([&](const auto& ptr) {
                    using E = Event<UnitOf<decltype(ptr)>>;
                    ptr->duration = detail::checked_event_field<E, 1>(
                        detail::sub_time(event_input<E, 0>(value), ptr->time)
                    );
                });
            },
            kEventEndDoc
        );
    }
    if constexpr (std::same_as<Event<Tick>, Note<Tick>>) {
        type.def(
            "empty",
            [](const Value& self) {
                return self.visit([](const auto& ptr) { return ptr->empty(); });
            },
            kEventNoteEmptyDoc
        );
        const auto bind_shift
            = [&](const char* name, auto operation, const char* signature, const char* doc) {
                  type.def(
                      name,
                      [operation](const Value& self, nb::handle offset) {
                          self.visit( [&](const auto& ptr) {
                              operation(*ptr, read_int32(offset, "offset"));
                          });
                      },
                      "offset"_a,
                      nb::sig(signature),
                      doc
                  );
              };
        bind_shift(
            "shift_pitch",
            [](auto& note, i32 offset) { note.shift_pitch(offset); },
            "def shift_pitch(self, offset: int) -> None",
            kEventNoteShiftPitchDoc
        );
        bind_shift(
            "shift_velocity",
            [](auto& note, i32 offset) { note.shift_velocity(offset); },
            "def shift_velocity(self, offset: int) -> None",
            kEventNoteShiftVelocityDoc
        );
    }

    bind_list_ops<EventSequence<Event>::template Native, Value>(list);
    bind_list_algorithms<List, Value>(list);
    bind_pickle(
        list,
        [](const auto& ptr) { return serialize(ptr->collect()); },
        [](auto unit, std::span<const u8> data) {
            return SharedStorage::adopt(deserialize<vec<Event<decltype(unit)>>>(data));
        }
    );
    list.def(
        "to_numpy",
        [](const List& self) {
            return self.visit([](const auto& ptr) -> nb::dict {
                using E = Event<UnitOf<decltype(ptr)>>;
                if constexpr (std::same_as<E, TextMeta<UnitOf<decltype(ptr)>>>) {
                    throw std::runtime_error("NumPy conversion is unsupported for text events");
                } else {
                    return owned_array(
                        std::make_unique<EventColumns<E>>(*ptr),
                        [](auto& data, const nb::capsule& owner) {
                            nb::dict result;
                            data.for_each([&](const char* field, auto& column) {
                                using Scalar =
                                    typename std::remove_reference_t<decltype(column)>::value_type;
                                static Scalar empty = 0;
                                auto*         ptr   = column.empty() ? &empty : column.data();
                                if constexpr (std::same_as<E, KeySignature<typename E::ttype>>
                                              && std::same_as<Scalar, u8>) {
                                    result[field] = nb::ndarray<nb::numpy, bool>(
                                        ptr, {column.size()}, owner
                                    );
                                } else {
                                    result[field] = nb::ndarray<nb::numpy, Scalar>(
                                        ptr, {column.size()}, owner
                                    );
                                }
                            });
                            return result;
                        }
                    );
                }
            });
        },
        kEventVecNumpyDoc
    );
    if constexpr (std::same_as<Event<Tick>, TextMeta<Tick>>) {
        list.def_static(
            "from_numpy",
            []() { throw std::runtime_error("TextMeta does not support from_numpy"); },
            kEventTextMetaNumpyDoc
        );
    } else {
        bind_from_numpy<Event>(list, fields);
    }
    return type;
}

template<template<class> class Event>
constexpr auto event_indices
    = std::make_index_sequence<std::tuple_size_v<decltype(detail::event_fields<Event<Tick>>)>>{};

template<template<class> class Event, class... Args>
void bind_event(
    nb::module_& module, const char* name, const char* list_name, const char* doc, Args... args
) {
    bind_event<Event>(module, name, list_name, doc, event_indices<Event>, [&](auto& type) {
        bind_event_init(type, event_indices<Event>, args...);
    });
}

}   // namespace symusic::py
