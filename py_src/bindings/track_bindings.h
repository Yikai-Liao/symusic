//
// Track bindings and related container helpers.
//

#pragma once

#include <algorithm>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <nanobind/nanobind.h>
#include <nanobind/ndarray.h>
#include <nanobind/stl/vector.h>
#include <fmt/format.h>

#include "../utils/python_helpers.h"
#include "../utils/vector_bindings.h"
#include "binding_common.h"

namespace symusic {

namespace nb = nanobind;

/// Deep copy a vector of shared pointers to keep Python level semantics clear.
template<typename T>
shared<vec<shared<T>>> clone_track_vector(const shared<vec<shared<T>>>& self) {
    auto ans = std::make_shared<vec<shared<T>>>();
    ans->reserve(self->size());
    for (const auto& item : *self) {
        ans->push_back(std::make_shared<T>(std::move(item->deepcopy())));
    }
    return ans;
}

template<TType T>
auto bind_track(nb::module_& m, const std::string& name_) {
    using unit    = typename T::unit;
    using self_t  = shared<Track<T>>;
    using track_t = Track<T>;
    using vec_t   = shared<vec<self_t>>;

    const auto name      = "Track" + name_;
    const auto track_sig = fmt::format("class {}(Track[{}])", name, name_);

    auto copy_func = [](const self_t& self) { return std::make_shared<track_t>(*self); };
    auto deepcopy_func
        = [](const self_t& self) { return std::make_shared<track_t>(std::move(self->deepcopy())); };

    // clang-format off
    auto track = nb::class_<shared<Track<T>>>(m, name.c_str(), nb::sig(track_sig.c_str()))
        .def("__init__", &pyutils::pyinit<Track<T>>)
        .def("__init__", &pyutils::pyinit<Track<T>, std::string, u8, const bool>,
            nb::arg("name"), nb::arg("program") = 0, nb::arg("is_drum") = false)
        .def("__init__", [](self_t *self, const self_t& other) {
            new (self) std::shared_ptr<track_t>(std::make_shared<track_t>(other->deepcopy()));
        }, "Copy constructor", nb::arg("other"))
        .def("copy", [&](const self_t &self, const bool deep) {
            if (deep) return deepcopy_func(self);
            return copy_func(self);
        }, nb::arg("deep") = true, nb::rv_policy::copy)
        .def("__copy__", copy_func, nb::rv_policy::copy)
        .def("__deepcopy__", [&](const self_t& self, const nb::handle, const nb::handle) {
            return deepcopy_func(self);
        }, nb::arg("memo")=nb::none(), nb::arg("_nil")=nb::none(), nb::rv_policy::copy)
        .def("__repr__", [](const self_t& self) { return self->to_string(); })
        .def("__getstate__", [](const self_t& self) {
            const auto native = to_native(*self);
            const vec<unsigned char> data = dumps<DataFormat::ZPP>(native);
            return nb::bytes(reinterpret_cast<const char*>(data.data()), data.size());
        })
        .def("__setstate__", [](self_t& self, const nb::bytes& bytes) {
            const auto      data = std::string_view(bytes.c_str(), bytes.size());
            const std::span span(reinterpret_cast<const unsigned char*>(data.data()), data.size());
            auto ans = symusic::parse<symusic::DataFormat::ZPP, TrackNative<T>>(span);
            new (&self) self_t(std::make_shared<Track<T>>(std::move(to_shared(std::move(ans)))));
        })
        .def_prop_ro("ttype", [](const self_t&) { return T(); })
        .def("__use_count", [](const self_t& self) { return self.use_count(); })
        .def_prop_rw(RW_COPY(shared<pyvec<Note<T>>>, "notes", notes))
        .def_prop_rw(RW_COPY(shared<pyvec<ControlChange<T>>>, "controls", controls))
        .def_prop_rw(RW_COPY(shared<pyvec<Pedal<T>>>, "pedals", pedals))
        .def_prop_rw(RW_COPY(shared<pyvec<PitchBend<T>>>, "pitch_bends", pitch_bends))
        .def_prop_rw(RW_COPY(shared<pyvec<TextMeta<T>>>, "lyrics", lyrics))
        .def_prop_rw(RW_COPY(bool, "is_drum", is_drum))
        .def_prop_rw(RW_COPY(u8, "program", program))
        .def_prop_rw(RW_COPY(std::string, "name", name))
        .def("__eq__", [](const self_t& self, const self_t& other) { return self == other || *self == *other; })
        .def("__eq__", [](const self_t&, nb::handle) { return false; })
        .def("__ne__", [](const self_t& self, const self_t& other) { return self != other && *self != *other; })
        .def("__ne__", [](const self_t&, nb::handle) { return true; })
        .def("end", [](const self_t& self) { return self->end(); })
        .def("start", [](const self_t& self) { return self->start(); })
        .def("note_num", [](const self_t& self) { return self->note_num(); })
        .def("empty", [](const self_t& self) { return self->empty(); })
        .def("clip", [](self_t& self, const unit start, const unit end, const bool clip_end, const bool inplace) {
            self_t ans = inplace ? self : std::make_shared<track_t>(std::move(self->deepcopy()));
            ans->clip_inplace(start, end, clip_end);
            return ans;
        }, nb::arg("start"), nb::arg("end"), nb::arg("clip_end") = false, nb::arg("inplace") = false)
        .def("trim", [](self_t& self, const unit start, const unit end, const unit min_overlap, const std::string &start_mode, const std::string &end_mode, const bool inplace) {
            self_t ans = inplace ? self : std::make_shared<track_t>(std::move(self->deepcopy()));
            ans->trim_inplace(start, end, min_overlap, start_mode, end_mode);
            return ans;
        }, nb::arg("start"), nb::arg("end"), nb::arg("min_overlap") = 0, nb::arg("start_mode") = "remove", nb::arg("end_mode") = "remove", nb::arg("inplace") = false)
        .def("sort", [](self_t& self, const bool reverse, const bool inplace) {
            self_t ans = inplace ? self : std::make_shared<track_t>(std::move(self->deepcopy()));
            ans->sort_inplace(reverse);
            return ans;
        }, nb::arg("reverse") = false, nb::arg("inplace") = true)
        .def("adjust_time", [](self_t& self, const vec<unit>& original_times, const vec<unit>& new_times, const bool inplace) {
            self_t ans = inplace ? self : std::make_shared<track_t>(std::move(self->deepcopy()));
            ops::adjust_time_inplace(*ans, original_times, new_times);
            return ans;
        }, nb::arg("original_times"), nb::arg("new_times"), nb::arg("inplace") = false)
        .def("shift_time", [](self_t& self, const unit offset, const bool inplace) {
            self_t ans = inplace ? self : std::make_shared<track_t>(std::move(self->deepcopy()));
            ans->shift_time_inplace(offset);
            return ans;
        }, nb::arg("offset"), nb::arg("inplace") = false)
        .def("shift_pitch", [](self_t& self, const i8 offset, const bool inplace) {
            self_t ans = inplace ? self : std::make_shared<track_t>(std::move(self->deepcopy()));
            ans->shift_pitch_inplace(offset);
            return ans;
        }, nb::arg("offset"), nb::arg("inplace") = false)
        .def("shift_velocity", [](self_t& self, const i8 offset, const bool inplace) {
            self_t ans = inplace ? self : std::make_shared<track_t>(std::move(self->deepcopy()));
            ans->shift_velocity_inplace(offset);
            return ans;
        }, nb::arg("offset"), nb::arg("inplace") = false)
    ;

    if constexpr (std::is_same_v<T, Tick>) {
        track.def("pianoroll", [](
            const self_t&           self,
            const vec<std::string>& modes,
            const std::pair<i64, i64> pitch_range,
            const bool              encode_velocity) {
                vec<PianorollMode> mode_enums(modes.size());
                for (size_t i = 0; i < modes.size(); ++i) {
                    mode_enums[i] = str_to_pianoroll_mode(modes[i]);
                }
                auto pianoroll = TrackPianoroll::from_track(*self, mode_enums, validate_pitch_range(pitch_range), encode_velocity);
                auto* data = const_cast<u8*>(pianoroll.release());
                nb::capsule owner(data, [](void* d) noexcept { delete[] static_cast<u8*>(d); });
                return nb::ndarray<nb::numpy, pianoroll_t>{
                    data, {
                        std::get<0>(pianoroll.dims()),
                        std::get<1>(pianoroll.dims()),
                        std::get<2>(pianoroll.dims()),
                    }, owner
                };
            },
            nb::arg("modes")           = vec<std::string>{"frame", "onset"},
            nb::arg("pitch_range")     = std::pair<i64, i64>(0, 128),
            nb::arg("encode_velocity") = false);
    }

    auto track_vec = pyutils::bind_shared_vector_copy<vec<self_t>>(m, (name + "List").c_str())
        .def_prop_ro("ttype", [](const vec_t&) { return T(); })
        .def("filter", [](const vec_t& self, const nb::object & func, const bool inplace) {
            auto ans = inplace ? self : std::make_shared<vec<self_t>>(self->begin(), self->end());
            auto it = std::remove_if(ans->begin(), ans->end(), [&](const self_t& t) {
                return !nb::cast<bool>(nb::cast<nb::callable>(func)(t));
            });
            ans->erase(it, ans->end());
            return ans;
        }, nb::arg("function"), nb::arg("inplace") = true)
        .def("sort", [](vec_t& self, const nb::object& key, const bool reverse,  const bool inplace) {
            auto ans = inplace ? self : std::make_shared<vec<self_t>>(self->begin(), self->end());
            if(key.is_none()) {
                auto cmp = [](const self_t& a, const self_t& b) { return a->default_key() < b->default_key(); };
                if (reverse) gfx::timsort(ans->rbegin(), ans->rend(), cmp);
                else gfx::timsort(ans->begin(), ans->end(), cmp);
            } else {
                auto key_ = nb::cast<nb::callable>(key);
                auto cmp = [&](const self_t& a, const self_t& b) { return key_(a) < key_(b); };
                if (reverse) gfx::timsort(ans->rbegin(), ans->rend(), cmp);
                else gfx::timsort(ans->begin(), ans->end(), cmp);
            }   return ans;
        }, nb::arg("key") = nb::none(), nb::arg("reverse") = false, nb::arg("inplace") = true)
        .def("is_sorted", [](const vec_t& self, const nb::object& key, const bool reverse) {
            if(key.is_none()) {
                auto cmp = [](const self_t& a, const self_t& b) { return a->default_key() < b->default_key(); };
                if (reverse) return std::is_sorted(self->rbegin(), self->rend(), cmp);
                else return std::is_sorted(self->begin(), self->end(), cmp);
            } else {
                auto key_ = nb::cast<nb::callable>(key);
                auto cmp = [&](const self_t& a, const self_t& b) { return key_(a) < key_(b); };
                if (reverse) return std::is_sorted(self->rbegin(), self->rend(), cmp);
                else return std::is_sorted(self->begin(), self->end(), cmp);
            }
        }, nb::arg("key") = nb::none(), nb::arg("reverse") = false)
        .def("adjust_time", [](vec_t& self, const vec<unit>& original_times, const vec<unit>& new_times, const bool inplace) {
            auto ans = inplace ? self : clone_track_vector(self);
            for (auto& t : *ans) ops::adjust_time_inplace(*t, original_times, new_times);
            return ans;
        }, nb::arg("original_times"), nb::arg("new_times"), nb::arg("inplace") = false)
        .def("copy",          [&](const vec_t& self, const bool deep) {
            return deep?clone_track_vector(self):std::make_shared<vec<self_t>>(self->begin(), self->end());
        }, nb::arg("deep") = true, nb::rv_policy::copy)
        .def("__copy__",      [](const vec_t& self) { return std::make_shared<vec<self_t>>(self->begin(), self->end()); })
        .def("__deepcopy__",  [](const vec_t& self, const nb::handle, const nb::handle) { return clone_track_vector(self); },
            nb::arg("memo")=nb::none(), nb::arg("_nil")=nb::none(), nb::rv_policy::move)
        .def("__getstate__",  [](const vec_t& self) {
            vec<TrackNative<T>> native;
            native.reserve(self->size());
            for (const auto& item : *self) { native.emplace_back(std::move(to_native(*item))); }
            const vec<unsigned char> data = dumps<DataFormat::ZPP>(native);
            return nb::bytes(reinterpret_cast<const char*>(data.data()), data.size());
        })
        .def("__setstate__",  [](vec_t& self, const nb::bytes& bytes) {
            const auto      data = std::string_view(bytes.c_str(), bytes.size());
            const std::span span(reinterpret_cast<const unsigned char*>(data.data()), data.size());
            auto ans = symusic::parse<symusic::DataFormat::ZPP, vec<TrackNative<T>>>(span);
            auto tracks = std::make_shared<vec<self_t>>();
            tracks->reserve(ans.size());
            for (auto& item : ans) { tracks->emplace_back(std::make_shared<Track<T>>(std::move(to_shared(std::move(item))))); }
            new (&self) vec_t(std::move(tracks));
        })
    ;
    // clang-format on

    return std::make_tuple(track, track_vec);
}

}   // namespace symusic
