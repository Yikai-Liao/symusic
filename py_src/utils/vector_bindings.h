#pragma once

#include <algorithm>

#include <nanobind/typing.h>
#include <nanobind/stl/bind_vector.h>
#include <timsort.hpp>

#include "utils/python_helpers.h"

namespace symusic::py {

template<template<class> class Event>
struct EventSequence {
    template<TType T>
    using Native = pyvec<Event<T>>;
    using Value  = UnitValue<Native>;
};

template<TType T>
using TrackSequence = vec<shared<Track<T>>>;
using TrackList     = UnitValue<TrackSequence>;

template<class Vector>
constexpr bool is_event_sequence = requires(Vector& list) { list.pbegin(); };

template<class Vector>
auto list_item(Vector& list, Py_ssize_t index) {
    if constexpr (is_event_sequence<Vector>)
        return list.getitem(index);
    else
        return list[nb::detail::wrap(index, list.size())];
}

template<class Vector, class Item>
void append_item(Vector& list, const Item& item) {
    if constexpr (is_event_sequence<Vector>)
        list.append(item);
    else
        list.push_back(item);
}

template<class List, class Item, class Vector>
shared<Vector> read_list(nb::handle source) {
    using T = typename NativeUnit<Vector>::type;
    if (nb::isinstance<List>(source)) return nb::cast<const List&>(source).template get<T>();
    auto result = std::make_shared<Vector>();
    for (nb::handle item : nb::cast<nb::iterable>(source)) {
        if (!nb::isinstance<Item>(item)) throw nb::type_error("Invalid item type for this list");
        const auto& handle = nb::cast<const Item&>(item).template get<T>();
        if constexpr (is_event_sequence<Vector>)
            result->push_back(*handle);
        else
            result->push_back(handle);
    }
    return result;
}

template<class Vector>
shared<Vector> clone_list(Vector& list, bool deep) {
    if constexpr (is_event_sequence<Vector>) {
        return std::make_shared<Vector>(deep ? list.deepcopy() : list.copy());
    } else {
        if (!deep) return std::make_shared<Vector>(list);
        auto result = std::make_shared<Vector>();
        result->reserve(list.size());
        for (const auto& item : list) {
            using Track = typename Vector::value_type::element_type;
            result->push_back(std::make_shared<Track>(item->deepcopy()));
        }
        return result;
    }
}

template<class List, class Item>
struct ListIterator {
    List   list;
    Py_ssize_t index = 0;

    Item next() {
        return list.visit([&](const auto& ptr) {
            if (index < 0 || static_cast<size_t>(index) >= ptr->size()) {
                index = -1;
                throw nb::stop_iteration();
            }
            return Item(list_item(*ptr, index++));
        });
    }
};

template<template<class> class Native, class Item>
void bind_list_ops(nb::class_<UnitValue<Native>>& type) {
    using List     = UnitValue<Native>;
    using Iterator = ListIterator<List, Item>;
    nb::class_<Iterator>(type, "_Iterator")
        .def(
            "__iter__", [](Iterator& self) -> Iterator& { return self; }, nb::rv_policy::reference
        )
        .def("__next__", &Iterator::next);
    const auto copy = [](const List& self, bool deep) {
        return self.visit([&](const auto& ptr) { return List(clone_list(*ptr, deep)); });
    };
    const auto equal = [](const List& self, nb::handle other) {
        if (nb::isinstance<List>(other)) {
            const auto& rhs = nb::cast<const List&>(other);
            return self.visit([&](const auto& ptr) {
                const auto* match = rhs.template find<UnitOf<decltype(ptr)>>();
                if (!match || ptr->size() != (*match)->size()) return false;
                if constexpr (is_event_sequence<Native<UnitOf<decltype(ptr)>>>) {
                    return *ptr == **match;
                } else {
                    return std::equal(
                        ptr->begin(),
                        ptr->end(),
                        (*match)->begin(),
                        [](const auto& left, const auto& right) { return *left == *right; }
                    );
                }
            });
        }
        if (!nb::isinstance<nb::list>(other)) return false;
        auto rhs = nb::borrow<nb::list>(other);
        return self.visit([&](const auto& ptr) {
            if (ptr->size() != rhs.size()) return false;
            for (size_t i = 0; i < ptr->size(); ++i) {
                nb::handle object = rhs[i];
                if (!nb::isinstance<Item>(object)) return false;
                const auto& value = nb::cast<const Item&>(object);
                const auto* match = value.template find<UnitOf<decltype(ptr)>>();
                if (!match || *list_item(*ptr, i) != **match) return false;
            }
            return true;
        });
    };
    const auto count = [](const List& self, const Item& value) -> size_t {
        return self.visit([&](const auto& ptr) -> size_t {
            const auto* match = value.template find<UnitOf<decltype(ptr)>>();
            if (!match) return 0;
            if constexpr (is_event_sequence<Native<UnitOf<decltype(ptr)>>>)
                return ptr->count(*match);
            else
                return std::ranges::count_if(*ptr, [&](const auto& item) {
                    return *item == **match;
                });
        });
    };

    type.def(
            "__init__",
            [copy](List* self, const List& other) { new (self) List(copy(other, false)); }
    )
        .def(
            "__init__",
            [](List* self, nb::handle items, nb::handle marker) {
                new (self) List(with_unit(marker, [&](auto unit) {
                    using Vector = Native<decltype(unit)>;
                    if (items.is_none()) return List(std::make_shared<Vector>());
                    auto source = read_list<List, Item, Vector>(items);
                    return List(
                        nb::isinstance<List>(items) ? clone_list(*source, false) : std::move(source)
                    );
                }));
            },
            "items"_a = nb::none(),
            nb::kw_only(),
            "ttype"_a = nb::type<Tick>()
    )
        .def(
            "__len__",
            [](const List& self) { return self.visit([](const auto& ptr) { return ptr->size(); }); }
        )
        .def(
            "__bool__",
            [](const List& self) {
                return self.visit([](const auto& ptr) { return !ptr->empty(); });
            }
        )
        .def("__iter__", [](const List& self) { return Iterator{self}; })
        .def(
            "__repr__",
            [](nb::handle self) { return nb::steal<nb::str>(nb::detail::repr_list(self.ptr())); },
            nb::sig("def __repr__(self, /) -> str")
        )
        .def("__eq__", equal, "other"_a.none())
        .def("clear", [](const List& self) { self.visit([](const auto& ptr) { ptr->clear(); }); })
        .def(
            "__getitem__",
            [](const List& self, Py_ssize_t index) {
                return self.visit([&](const auto& ptr) { return Item(list_item(*ptr, index)); });
            },
            "index"_a
        )
        .def(
            "__getitem__",
            [](const List& self, const nb::slice& slice) {
                return self.visit([&](const auto& ptr) {
                    using Vector                           = Native<UnitOf<decltype(ptr)>>;
                    const auto [start, stop, step, length] = slice.compute(ptr->size());
                    if constexpr (is_event_sequence<Vector>) {
                        return List(std::make_shared<Vector>(ptr->getitem({start, stop, step})));
                    } else {
                        auto result = std::make_shared<Vector>();
                        result->reserve(length);
                        for (size_t i = 0; i < length; ++i) {
                            result->push_back((*ptr)[start + static_cast<Py_ssize_t>(i) * step]);
                        }
                        return List(std::move(result));
                    }
                });
            },
            "index"_a
        )
        .def(
            "append",
            [](const List& self, const Item& item) {
                self.visit([&](const auto& ptr) {
                    append_item(*ptr, item.template get<UnitOf<decltype(ptr)>>());
                });
            },
            "item"_a
        )
        .def(
            "insert",
            [](const List& self, Py_ssize_t index, const Item& item) {
                self.visit([&](const auto& ptr) {
                    using T           = UnitOf<decltype(ptr)>;
                    const auto& value = item.template get<T>();
                    if constexpr (is_event_sequence<Native<T>>)
                        ptr->insert(index, value);
                    else {
                        const auto size = static_cast<Py_ssize_t>(ptr->size());
                        if (index < 0) index += size;
                        if (index < 0 || index > size) throw nb::index_error();
                        ptr->insert(ptr->begin() + index, value);
                    }
                });
            },
            "index"_a,
            "item"_a
        )
        .def(
            "pop",
            [](const List& self, Py_ssize_t index) {
                return self.visit([&](const auto& ptr) {
                    if constexpr (is_event_sequence<Native<UnitOf<decltype(ptr)>>>)
                        return Item(ptr->pop(index));
                    else {
                        auto item = ptr->begin() + nb::detail::wrap(index, ptr->size());
                        Item out(*item);
                        ptr->erase(item);
                        return out;
                    }
                });
            },
            "index"_a = -1
        )
        .def(
            "extend",
            [](const List& self, const nb::iterable& items) {
                self.visit([&](const auto& ptr) {
                    using Vector = Native<UnitOf<decltype(ptr)>>;
                    auto source  = read_list<List, Item, Vector>(items);
                    if (source == ptr) source = clone_list(*source, false);
                    if constexpr (is_event_sequence<Vector>)
                        ptr->extend(*source);
                    else
                        ptr->insert(ptr->end(), source->begin(), source->end());
                });
            },
            "items"_a
        )
        .def(
            "__setitem__",
            [](const List& self, Py_ssize_t index, const Item& item) {
                self.visit([&](const auto& ptr) {
                    using T           = UnitOf<decltype(ptr)>;
                    const auto& value = item.template get<T>();
                    if constexpr (is_event_sequence<Native<T>>)
                        ptr->setitem(index, value);
                    else
                        (*ptr)[nb::detail::wrap(index, ptr->size())] = value;
                });
            },
            "index"_a,
            "item"_a
        )
        .def(
            "__setitem__",
            [](const List& self, const nb::slice& slice, const nb::iterable& items) {
                self.visit([&](const auto& ptr) {
                    using Vector = Native<UnitOf<decltype(ptr)>>;
                    auto source  = read_list<List, Item, Vector>(items);
                    if (source == ptr) source = clone_list(*source, false);
                    const auto [start, stop, step, length] = slice.compute(ptr->size());
                    if constexpr (is_event_sequence<Vector>)
                        ptr->setitem({start, stop, step}, *source);
                    else {
                        if (length != source->size()) throw nb::index_error("Slice size mismatch");
                        for (size_t i = 0; i < length; ++i) {
                            (*ptr)[start + static_cast<Py_ssize_t>(i) * step] = (*source)[i];
                        }
                    }
                });
            },
            "index"_a,
            "items"_a
        )
        .def(
            "__delitem__",
            [](const List& self, Py_ssize_t index) {
                self.visit([&](const auto& ptr) {
                    if constexpr (is_event_sequence<Native<UnitOf<decltype(ptr)>>>)
                        ptr->delitem(index);
                    else
                        ptr->erase(ptr->begin() + nb::detail::wrap(index, ptr->size()));
                });
            },
            "index"_a
        )
        .def(
            "__delitem__",
            [](const List& self, const nb::slice& slice) {
                self.visit([&](const auto& ptr) {
                    auto [start, stop, step, length] = slice.compute(ptr->size());
                    if (length == 0) return;
                    if constexpr (is_event_sequence<Native<UnitOf<decltype(ptr)>>>) {
                        ptr->delitem({start, stop, step});
                    } else {
                        if (step > 0) {
                            start += (static_cast<Py_ssize_t>(length) - 1) * step;
                            step = -step;
                        }
                        for (size_t i = 0; i < length; ++i) {
                            ptr->erase(ptr->begin() + (start + static_cast<Py_ssize_t>(i) * step));
                        }
                    }
                });
            },
            "index"_a
        )
        .def(
            "__contains__",
            [count](const List& self, nb::handle item) {
                return nb::isinstance<Item>(item) && count(self, nb::cast<const Item&>(item)) != 0;
            },
            "item"_a.none()
        )
        .def("count", count, "item"_a)
        .def(
            "remove",
            [](const List& self, const Item& value) {
                self.visit([&](const auto& ptr) {
                    using T          = UnitOf<decltype(ptr)>;
                    const auto& item = value.template get<T>();
                    if constexpr (is_event_sequence<Native<T>>)
                        ptr->remove(item);
                    else {
                        auto pos = std::ranges::find_if(*ptr, [&](const auto& element) {
                            return *element == *item;
                        });
                        if (pos == ptr->end()) throw std::invalid_argument("Item is not in list");
                        ptr->erase(pos);
                    }
                });
            },
            "item"_a
        )
        .def(
            "__deepcopy__",
            [copy](const List& self, nb::handle, nb::handle) { return copy(self, true); },
            "memo"_a = nb::none(),
            "_nil"_a = nb::none()
        );

    if constexpr (is_event_sequence<Native<Tick>>) {
        type.def(
                "copy", [copy](const List& self) { return copy(self, false); }
        ).def("__copy__", copy, "deep"_a = true);
    } else {
        type.def("copy", copy, "deep"_a = true).def("__copy__", [copy](const List& self) {
            return copy(self, false);
        });
    }
}

template<class List, class Item>
void bind_list_algorithms(nb::class_<List>& type) {
    const auto order = [](auto& list, const nb::object& key, bool reverse, bool sort) {
        const auto default_key = [](const auto& item) { return item.default_key(); };
        const auto python_key  = [&](const auto& ptr) { return key(Item(ptr)); };
        using Vector           = std::remove_reference_t<decltype(list)>;
        if constexpr (is_event_sequence<Vector>) {
            if (sort) {
                if (key.is_none())
                    list.sort(default_key, reverse);
                else
                    list.sort_shared(python_key, reverse);
                return true;
            }
            if (key.is_none()) return list.is_sorted(default_key, reverse);
            return list.is_sorted_shared(python_key, reverse);
        } else {
            const auto less = [&](const auto& left, const auto& right) {
                if (key.is_none())
                    return detail::track_sort_key(*left) < detail::track_sort_key(*right);
                return python_key(left) < python_key(right);
            };
            const auto apply = [&](auto first, auto last) {
                if (!sort) return std::is_sorted(first, last, less);
                gfx::timsort(first, last, less);
                return true;
            };
            return reverse ? apply(list.rbegin(), list.rend()) : apply(list.begin(), list.end());
        }
    };

    type.def(
            "sort",
            [order](nb::handle self, const nb::object& key, bool reverse, bool inplace) {
                return transform<List>(self, inplace, [&](const auto& ptr) {
                    auto result = inplace ? ptr : clone_list(*ptr, false);
                    order(*result, key, reverse, true);
                    return List(std::move(result));
                });
            },
            "key"_a     = nb::none(),
            "reverse"_a = false,
            "inplace"_a = true,
            nb::sig(
                "def sort(self, key: collections.abc.Callable | None = None, reverse: bool = "
                "False, inplace: bool = True) -> typing.Self"
            ),
            "Sort by the given key or by the default ordering."
    )
        .def(
            "is_sorted",
            [order](const List& self, const nb::object& key, bool reverse) {
                return self.visit([&](const auto& ptr) {
                    return order(*ptr, key, reverse, false);
                });
            },
            "key"_a     = nb::none(),
            "reverse"_a = false,
            nb::sig(
                "def is_sorted(self, key: collections.abc.Callable | None = None, reverse: bool = "
                "False) -> bool"
            ),
            "Check the ordering without modifying the list."
        )
        .def(
            "filter",
            [](nb::handle self, const nb::callable& function, bool inplace) {
                return transform<List>(self, inplace, [&](const auto& ptr) {
                    auto       result = inplace ? ptr : clone_list(*ptr, false);
                    const auto keep
                        = [&](const auto& item) { return nb::cast<bool>(function(Item(item))); };
                    using Vector = typename std::remove_cvref_t<decltype(ptr)>::element_type;
                    if constexpr (is_event_sequence<Vector>)
                        result->filter_shared(keep);
                    else
                        std::erase_if(*result, [&](const auto& item) { return !keep(item); });
                    return List(std::move(result));
                });
            },
            "function"_a,
            "inplace"_a = true,
            nb::sig(
                "def filter(self, function: collections.abc.Callable, inplace: bool = True) -> "
                "typing.Self"
            ),
            "Keep the items for which the function returns true."
        )
        .def(
            "adjust_time",
            [](nb::handle          self,
               const nb::iterable& original_times,
               const nb::iterable& new_times,
               bool                inplace) {
                return transform<List>(self, inplace, [&](const auto& ptr) {
                    using T                = UnitOf<decltype(ptr)>;
                    const auto original    = read_times<T>(original_times);
                    const auto replacement = read_times<T>(new_times);
                    auto       result      = inplace ? ptr : clone_list(*ptr, true);
                    using Vector = typename std::remove_cvref_t<decltype(ptr)>::element_type;
                    if constexpr (is_event_sequence<Vector>) {
                        ops::adjust_time_inplace<true, SharedStorage>(
                            *result, original, replacement
                        );
                    } else {
                        detail::adjust_track_times(*result, original, replacement);
                    }
                    return List(std::move(result));
                });
            },
            "original_times"_a,
            "new_times"_a,
            "inplace"_a = false,
            nb::sig(
                "def adjust_time(self, original_times: collections.abc.Iterable[int | float], "
                "new_times: collections.abc.Iterable[int | float], inplace: bool = False) -> "
                "typing.Self"
            ),
            "Map item times through paired anchors."
        );
}

template<class List, class Item, class Owner, class Field>
void bind_list_property(nb::class_<Owner>& type, const char* name, Field field, const char* doc) {
    type.def_prop_rw(
        name,
        [field](const Owner& self) {
            return self.visit([&](const auto& ptr) { return List(field(*ptr)); });
        },
        [field](const Owner& self, nb::handle items) {
            self.visit([&](const auto& ptr) {
                using Vector = typename std::remove_cvref_t<decltype(field(*ptr))>::element_type;
                field(*ptr)  = read_list<List, Item, Vector>(items);
            });
        },
        doc
    );
}

}   // namespace symusic::py
