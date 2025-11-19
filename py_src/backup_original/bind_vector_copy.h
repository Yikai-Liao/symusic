/*
    nanobind/stl/bind_vector.h: Automatic creation of bindings for vector-style containers

    Copyright (c) 2022 Wenzel Jakob

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

#pragma once
#include <nanobind/stl/bind_vector.h>
#include <memory>
#include <pyvec.hpp>

namespace pyutils {

template <typename Vector, typename... Args>
nanobind::class_<Vector> bind_vector_copy(nanobind::handle scope, const char *name, Args &&...args) {
    using namespace nanobind;

    using ValueRef = typename detail::iterator_access<typename Vector::iterator>::result_type;
    using Value = std::decay_t<ValueRef>;

    static_assert(
        !detail::is_base_caster_v<detail::make_caster<Value>> || detail::is_copy_constructible_v<Value>,
        "bind_vector(): the value type must be copy-constructible."
    );

    handle cl_cur = type<Vector>();
    if (cl_cur.is_valid()) {
        // Binding already exists, don't re-create
        return borrow<class_<Vector>>(cl_cur);
    }

    auto cl = class_<Vector>(scope, name, std::forward<Args>(args)...)
        .def(init<>(), "Default constructor")

        .def("__len__", [](const Vector &v) { return v.size(); })

        .def("__bool__",
             [](const Vector &v) { return !v.empty(); },
             "Check whether the vector is nonempty")

        .def("__repr__",
             [](handle_t<Vector> h) {
                return steal<str>(detail::repr_list(h.ptr()));
             })

        .def("__iter__",
             [](Vector &v) {
                 return make_iterator(type<Vector>(), "Iterator",
                                      v.begin(), v.end());
             }, keep_alive<0, 1>())

        .def("__getitem__",
             [](Vector &v, Py_ssize_t i) -> ValueRef {
                 return v[detail::wrap(i, v.size())];
             }, std::is_pointer_v<Value> ? rv_policy::take_ownership : rv_policy::copy)

        .def("clear", [](Vector &v) { v.clear(); },
             "Remove all items from list.");

    if constexpr (detail::is_copy_constructible_v<Value>) {
        cl.def(init<const Vector &>(),
               "Copy constructor");

        cl.def("__init__", [](Vector *v, typed<iterable, Value> &seq) {
            new (v) Vector();
            v->reserve(len_hint(seq));
            for (handle h : seq)
                v->push_back(cast<Value>(h));
        }, "Construct from an iterable object");

        implicitly_convertible<iterable, detail::self_t>();

        cl.def("append",
               [](Vector &v, const Value &value) { v.push_back(value); },
               "Append `arg` to the end of the list.")

          .def("insert",
               [](Vector &v, Py_ssize_t i, const Value &x) {
                   if (i < 0)
                       i += (Py_ssize_t) v.size();
                   if (i < 0 || (size_t) i > v.size())
                       throw index_error();
                   v.insert(v.begin() + i, x);
               },
               "Insert object `arg1` before index `arg0`.")

           .def("pop",
                [](Vector &v, Py_ssize_t i) {
                    size_t index = detail::wrap(i, v.size());
                    Value result = std::move(v[index]);
                    v.erase(v.begin() + index);
                    return result;
                },
                arg("index") = -1,
                "Remove and return item at `index` (default last).")

          .def("extend",
               [](Vector &v, const Vector &src) {
                   v.insert(v.end(), src.begin(), src.end());
               },
               "Extend `self` by appending elements from `arg`.")

          .def("__setitem__",
               [](Vector &v, Py_ssize_t i, const Value &value) {
                   v[detail::wrap(i, v.size())] = value;
               })

          .def("__delitem__",
               [](Vector &v, Py_ssize_t i) {
                   v.erase(v.begin() + detail::wrap(i, v.size()));
               })

          .def("__getitem__",
               [](const Vector &v, const slice &slice) -> Vector * {
                   auto [start, stop, step, length] = slice.compute(v.size());
                   auto *seq = new Vector();
                   seq->reserve(length);

                   for (size_t i = 0; i < length; ++i) {
                       seq->push_back(v[start]);
                       start += step;
                   }

                   return seq;
               })

          .def("__setitem__",
               [](Vector &v, const slice &slice, const Vector &value) {
                   auto [start, stop, step, length] = slice.compute(v.size());

                   if (length != value.size())
                       throw index_error(
                           "The left and right hand side of the slice "
                           "assignment have mismatched sizes!");

                   for (size_t i = 0; i < length; ++i) {
                       v[start] = value[i];
                       start += step;
                    }
               })

          .def("__delitem__",
               [](Vector &v, const slice &slice) {
                   auto [start, stop, step, length] = slice.compute(v.size());
                   if (length == 0)
                       return;

                   stop = start + (length - 1) * step;
                   if (start > stop) {
                       std::swap(start, stop);
                       step = -step;
                   }

                   if (step == 1) {
                       v.erase(v.begin() + start, v.begin() + stop + 1);
                   } else {
                       for (size_t i = 0; i < length; ++i) {
                           v.erase(v.begin() + stop);
                           stop -= step;
                       }
                   }
               });
    }

    if constexpr (detail::is_equality_comparable_v<Value>) {
        cl.def(self == self)
          .def(self != self)

          .def("__contains__",
               [](const Vector &v, const Value &x) {
                   return std::find(v.begin(), v.end(), x) != v.end();
               })

          .def("__contains__", // fallback for incompatible types
               [](const Vector &, handle) { return false; })

          .def("count",
               [](const Vector &v, const Value &x) {
                   return std::count(v.begin(), v.end(), x);
               }, "Return number of occurrences of `arg`.")

          .def("remove",
               [](Vector &v, const Value &x) {
                   auto p = std::find(v.begin(), v.end(), x);
                   if (p != v.end())
                       v.erase(p);
                   else
                       throw value_error();
               },
               "Remove first occurrence of `arg`.");
    }

    return cl;
}

template <typename Vector, typename... Args>
nanobind::class_<std::shared_ptr<Vector>> bind_shared_vector_copy(nanobind::handle scope, const char *name, Args &&...args) {
    using namespace nanobind;

    using ValueRef = typename detail::iterator_access<typename Vector::iterator>::result_type;
    using Value = std::decay_t<ValueRef>;
    using self_t = std::shared_ptr<Vector>;

    static_assert(
        !detail::is_base_caster_v<detail::make_caster<Value>> || detail::is_copy_constructible_v<Value>,
        "bind_vector(): the value type must be copy-constructible."
    );

    handle cl_cur = type<std::shared_ptr<Vector>>();
    if (cl_cur.is_valid()) {
        // Binding already exists, don't re-create
        return borrow<class_<std::shared_ptr<Vector>>>(cl_cur);
    }

    auto cl = class_<self_t>(scope, name, std::forward<Args>(args)...)
        .def("__init__", [](self_t *self) { new (self) self_t(std::move(std::make_shared<Vector>())); })

        .def("__len__", [](const self_t &v) { return v->size(); })

        .def("__bool__",
             [](const self_t &v) { return !v->empty(); },
             "Check whether the vector is nonempty")

        .def("__repr__",
             [](handle_t<self_t> h) {
                return steal<str>(detail::repr_list(h.ptr()));
             })

        .def("__iter__",
             [](self_t &v) {
                 return make_iterator(type<self_t>(), "Iterator",
                                      v->begin(), v->end());
             }, keep_alive<0, 1>())

        .def("__getitem__",
             [](self_t &v, Py_ssize_t i) -> ValueRef {
                 return v->operator[](detail::wrap(i, v->size()));
             }, std::is_pointer_v<Value> ? rv_policy::take_ownership : rv_policy::copy)

        .def("clear", [](self_t &v) { v->clear(); },
             "Remove all items from list.");

    if constexpr (detail::is_copy_constructible_v<Value>) {
        cl.def("__init__", [](self_t *self, const self_t &other) {
            new (self) self_t(std::move(std::make_shared<Vector>(*other)));
        }, "Shallow Copy constructor");

        cl.def("__init__", [](self_t &v, typed<iterable, Value> &seq) {
            new (&v) self_t(std::move(std::make_shared<Vector>()));
            v->reserve(len_hint(seq));
            for (handle h : seq)
                v->push_back(cast<Value>(h));
        }, "Construct from an iterable object");

        implicitly_convertible<iterable, self_t>();

        cl.def("append",
               [](self_t &v, const Value &value) { v->push_back(value); },
               "Append `arg` to the end of the list.")

          .def("insert",
               [](self_t &v, Py_ssize_t i, const Value &x) {
                   if (i < 0)
                       i += (Py_ssize_t) v->size();
                   if (i < 0 || (size_t) i > v->size())
                       throw index_error();
                   v->insert(v->begin() + i, x);
               },
               "Insert object `arg1` before index `arg0`.")

           .def("pop",
                [](self_t &v, Py_ssize_t i) {
                    size_t index = detail::wrap(i, v->size());
                    Value result = std::move(v->operator[](index));
                    v->erase(v->begin() + index);
                    return result;
                },
                arg("index") = -1,
                "Remove and return item at `index` (default last).")

          .def("extend",
               [](self_t &v, const self_t &src) {
                   v->insert(v->end(), src->begin(), src->end());
               },
               "Extend `self` by appending elements from `arg`.")

          .def("__setitem__",
               [](self_t &v, Py_ssize_t i, const Value &value) {
                   v->operator[](detail::wrap(i, v->size())) = value;
               })

          .def("__delitem__",
               [](self_t &v, Py_ssize_t i) {
                   v->erase(v->begin() + detail::wrap(i, v->size()));
               })

          .def("__getitem__",
               [](const self_t &v, const slice &slice) -> self_t {
                   auto [start, stop, step, length] = slice.compute(v->size());
                   auto seq = std::make_shared<Vector>();
                   seq->reserve(length);

                   for (size_t i = 0; i < length; ++i) {
                       seq->push_back(v->operator[](start));
                       start += step;
                   }

                   return seq;
               })

          .def("__setitem__",
               [](self_t &v, const slice &slice, const self_t &other) {
                   auto [start, stop, step, length] = slice.compute(v->size());

                   if (length != other->size())
                       throw index_error(
                           "The left and right hand side of the slice "
                           "assignment have mismatched sizes!");

                   for (size_t i = 0; i < length; ++i) {
                       v->operator[](start) = other->operator[](i);
                       start += step;
                    }
               })

          .def("__delitem__",
               [](self_t &v, const slice &slice) {
                   auto [start, stop, step, length] = slice.compute(v->size());
                   if (length == 0)
                       return;

                   stop = start + (length - 1) * step;
                   if (start > stop) {
                       std::swap(start, stop);
                       step = -step;
                   }

                   if (step == 1) {
                       v->erase(v->begin() + start, v->begin() + stop + 1);
                   } else {
                       for (size_t i = 0; i < length; ++i) {
                           v->erase(v->begin() + stop);
                           stop -= step;
                       }
                   }
               });
    }

    if constexpr (detail::is_equality_comparable_v<Value>) {
        auto equal = [](const self_t &v, const self_t &other) {
            if(!v) return !other;
            if(v->size() != other->size()) return false;
            for(size_t i = 0; i < v->size(); i++) {
                const auto& a = v->operator[](i);
                const auto& b = other->operator[](i);
                if(a!=b && *a!=*b) return false;
            }   return true;
        };
        auto eq_list = [](const self_t &v, const list &other){
            if (!v) return false;
            if (v->size() != len(other)) return false;
            for (size_t i = 0; i < v->size(); i++) {
                if (*v->operator[](i) != *cast<Value>(other[i]))
                    return false;
            }   return true;
        };
        cl.def("__eq__", equal)
          .def("__eq__", [](const self_t &, const object &) { return false; })
          .def("__eq__", eq_list)
          .def("__ne__", [&](const self_t &v, const self_t &other) { return !equal(v, other); })
          .def("__ne__", [](const self_t &, const object &) { return true; })
          .def("__ne__", [&](const self_t &v, const list &other) { return !eq_list(v, other); })

          .def("__contains__",
               [](const self_t &v, const Value &x) {
                   for(size_t i = 0; i < v->size(); i++) {
                       if(*v->operator[](i) == *x) return true;
                   }   return false;
               })

          .def("__contains__", // fallback for incompatible types
               [](const self_t &, handle) { return false; })

          .def("count",
               [](const self_t &v, const Value &x) {
                     size_t count = 0;
                     for(size_t i = 0; i < v->size(); i++) {
                         const auto &a = v->operator[](i);
                         if(a == x || *a == *x) count++;
                     }   return count;
               }, "Return number of occurrences of `arg`.")

          .def("remove",
               [](self_t &v, const Value &x) {
                   for(size_t i = 0; i < v->size(); i++) {
                       const auto &a = v->operator[](i);
                       if(a == x || *a == *x) {
                           v->erase(v->begin() + i);
                           return;
                       }
                   }
                   throw value_error();
               },
               "Remove first occurrence of `arg`.");
    }

    return cl;
}

// template <typename Vector, typename... Args>
template<typename T, typename ...Args>
nanobind::class_<std::shared_ptr<pycontainer::pyvec<T>>> bind_shared_pyvec(nanobind::handle scope, const char *name, Args &&...args) {
    using namespace nanobind;
    using namespace pycontainer;

    using Vector = pyvec<T>;
    using ValueRef = std::shared_ptr<T>;
    using self_t = std::shared_ptr<Vector>;

    handle cl_cur = type<self_t>();
    if (cl_cur.is_valid()) {
        // Binding already exists, don't re-create
        return borrow<class_<self_t>>(cl_cur);
    }

    auto cl = class_<self_t>(scope, name, std::forward<Args>(args)...)
        .def("__init__", [](self_t *self) { new (self) self_t(std::move(std::make_shared<Vector>())); })

        .def("__len__", [](const self_t &v) { return v->size(); })

        .def("__bool__",
             [](const self_t &v) { return !v->empty(); },
             "Check whether the vector is nonempty")

        // .def("__repr__",
        //      [](handle_t<self_t> h) {
        //         return steal<str>(detail::repr_list(h.ptr()));
        //      })

        .def("__iter__",
             [](self_t &v) {
                 return make_iterator(type<self_t>(), "Iterator", v->sbegin(), v->send());
             }, keep_alive<0, 1>())

        .def("__getitem__",
             [](self_t &v, Py_ssize_t i) -> ValueRef {
                 return v->getitem(i);
             }, rv_policy::copy)

        .def("clear", [](self_t &v) { v->clear(); },
             "Remove all items from list.");

        cl.def("__init__", [](self_t *self, const self_t &other) {
            new (self) self_t(std::move(std::make_shared<Vector>(*other)));
        }, "Shallow Copy constructor");

        cl.def("__init__", [](self_t &v, typed<iterable, ValueRef> &seq) {
            new (&v) self_t(std::move(std::make_shared<Vector>()));
            v->reserve(len_hint(seq));
            for (handle h : seq)
                v->push_back(*cast<ValueRef>(h));
        }, "Construct from an iterable object");

        implicitly_convertible<iterable, self_t>();

        cl.def("append",
               [](self_t &v, const ValueRef &value) { v->append(value); },
               "Append `arg` to the end of the list.")

        .def("insert",
            [](self_t &v, Py_ssize_t i, const ValueRef &x) {
                v->insert(i, x);
            },  "Insert object `arg1` before index `arg0`.")

        .def("pop",
            [](self_t &v, Py_ssize_t i) { return v->pop(i); },
            arg("index") = -1,
            "Remove and return item at `index` (default last).")

      .def("extend",
           [](self_t &v, const self_t &src) {
               v->extend(*src);
           },
           "Extend `self` by appending elements from `arg`.")

      .def("__setitem__",
           [](self_t &v, Py_ssize_t i, const ValueRef &value) {
               v->setitem(i, value);
           })

      .def("__delitem__",
           [](self_t &v, Py_ssize_t i) {
                v->delitem(i);
           })

      .def("__getitem__",
           [](const self_t &v, const nanobind::slice &slice) -> self_t {
               auto [start, stop, step, length] = slice.compute(v->size());
               return std::make_shared<Vector>(std::move(v->getitem({start, stop, step})));
           })

      .def("__setitem__",
           [](self_t &v, const nanobind::slice &slice, const self_t &other) {
               auto [start, stop, step, length] = slice.compute(v->size());
               v->setitem({start, stop, step}, *other);
           })

      .def("__delitem__",
           [](self_t &v, const nanobind::slice &slice) {
               auto [start, stop, step, length] = slice.compute(v->size());
               if (length == 0) return;
               v->delitem({start, stop, step});
           })

    .def("copy", [](const self_t &v) { return std::make_shared<Vector>(std::move(v->copy())); })
    .def("__copy__", [](const self_t &v, const bool deep) {
        if(deep) return std::make_shared<Vector>(std::move(v->deepcopy()));
        return std::make_shared<Vector>(std::move(v->copy()));
    }, arg("deep")=true)
    .def("__deepcopy__", [](const self_t &v, const handle&, const handle&) {
        return std::make_shared<Vector>(std::move(v->deepcopy()));
    }, arg("memo")=none(), arg("_nil")=none())

    .def("__use_count", [](const self_t &v) { return v.use_count(); });



    if constexpr (detail::is_equality_comparable_v<T>) {
        auto eq_list = [](const self_t &v, const list &other){
            if (v->size() != len(other)) return false;
            for (size_t i = 0; i < v->size(); i++) {
                if (*v->operator[](i) != *cast<ValueRef>(other[i]))
                    return false;
            }   return true;
        };

        cl.def("__eq__", [](const self_t &v, const self_t &other) { return *v == *other; })
          .def("__eq__", eq_list)
          .def("__eq__", [](const self_t &, handle) { return false; })
          .def("__ne__", [](const self_t &v, const self_t &other) { return *v != *other; })
          .def("__ne__", [&](const self_t &v, const list &other) { return !eq_list(v, other); })
          .def("__ne__", [](const self_t &, handle) { return true; })

          .def("__contains__",
               [](const self_t &v, const ValueRef &x) {
                   return v->contains(x);
               })

          .def("__contains__", // fallback for incompatible types
               [](const self_t &, handle) { return false; })

          .def("count",
               [](const self_t &v, const ValueRef &x) {
                   return v->count(x);
               }, "Return number of occurrences of `arg`.")

          .def("remove",
               [](self_t &v, const ValueRef &x) {
                   v->remove(x);
               },
               "Remove first occurrence of `arg`.");
    }

    return cl;
}

}  // namespace pyutils
