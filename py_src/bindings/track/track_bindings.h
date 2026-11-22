#pragma once

#include "utils/vector_bindings.h"
#include "../core/binding_common.h"

namespace symusic::py {

constexpr const char* kTrackCopyCtorDoc   = R"pbdoc(Create a deep copy of *other*.)pbdoc";
constexpr const char* kTrackCopyDoc       = R"pbdoc(
Return a deep copy, or with ``deep=False`` a new track that shares the event lists.
)pbdoc";
constexpr const char* kTrackClipDoc       = R"pbdoc(
Keep the events that start in ``[start, end)``; with *clip_end*, notes and pedals must also end by
*end*. Times are not shifted. Returns a deep copy unless *inplace* is true.
)pbdoc";
constexpr const char* kTrackTrimDoc       = R"pbdoc(
Keep notes and pedals that overlap ``[start, end)`` by at least *min_overlap*. At each boundary,
``"remove"`` drops crossing events, ``"truncate"`` cuts them at the boundary, and ``"retain"``
keeps them whole. Other events are clipped by time. Invalid modes raise ``ValueError``. Returns a
deep copy unless *inplace* is true.
)pbdoc";
constexpr const char* kTrackSortDoc       = R"pbdoc(
Sort every event list by its default sort key. Works in place unless *inplace* is false, which
returns a deep copy.
)pbdoc";
constexpr const char* kTrackAdjustTimeDoc = R"pbdoc(
Map times piecewise linearly through the anchor pairs ``(original_times[i], new_times[i])``. The
lists must be the same length and have at least two anchors. Original times must be strictly
increasing, and new times must be nondecreasing. Events outside the anchor range are dropped.
Returns a deep copy unless *inplace* is true.
)pbdoc";
constexpr const char* kTrackShiftTimeDoc
    = R"pbdoc(Add *offset* to every time. Returns a deep copy unless *inplace* is true.)pbdoc";
constexpr const char* kTrackShiftPitchDoc    = R"pbdoc(
Add *offset* to every note's pitch. Returns a deep copy unless *inplace* is true. Raises
``ValueError`` when a result leaves -128..127.
)pbdoc";
constexpr const char* kTrackShiftVelocityDoc = R"pbdoc(
Add *offset* to every note's velocity. Returns a deep copy unless *inplace* is true. Raises
``ValueError`` when a result leaves -128..127.
)pbdoc";
constexpr const char* kTrackStartDoc
    = R"pbdoc(Return the earliest event time, or 0 when the track has no events.)pbdoc";
constexpr const char* kTrackEndDoc
    = R"pbdoc(Return the latest event end, or 0 when the track has no events.)pbdoc";
constexpr const char* kTrackNoteNumDoc   = R"pbdoc(Return the number of notes.)pbdoc";
constexpr const char* kTrackEmptyDoc     = R"pbdoc(Return whether the track has no events.)pbdoc";
constexpr const char* kTrackPianorollDoc = R"pbdoc(
Return a ``uint8`` array of shape ``(modes, pitches, time)`` with one column per tick. *modes*
lists ``"frame"``, ``"onset"`` or ``"offset"``; *pitch_range* is the pitch interval ``[low,
high)``; *encode_velocity* writes velocities instead of 1. Raises ``ValueError`` for a note with
negative time or duration, or when the track ends before tick 0.
)pbdoc";
using TrackValue                         = UnitValue<Track>;

inline void bind_track(nb::module_& module) {
    auto type = unit_class<TrackValue>(
        module,
        "Track",
        "Track(name='', program=0, is_drum=False, notes=None, controls=None, pitch_bends=None, "
        "pedals=None, *, ttype=Tick)\n\nA musical part."
    );
    auto list = unit_class<TrackList>(
        module,
        "TrackList",
        "TrackList(items=None, *, ttype=Tick)\n\nA mutable list of tracks in one time unit."
    );
    type.def(
            "__init__",
            [](TrackValue*        self,
               const std::string& name,
               u8                 program,
               bool               is_drum,
               nb::handle         notes,
               nb::handle         controls,
               nb::handle         pitch_bends,
               nb::handle         pedals,
               nb::handle         marker) {
                new (self) TrackValue(with_unit(marker, [&](auto unit) {
                    using T           = decltype(unit);
                    auto       result = std::make_shared<Track<T>>(name, program, is_drum);
                    const auto read =
                        [&]<template<class> class E>(shared<pyvec<E<T>>>& field, nb::handle input) {
                            if (!input.is_none())
                                field = read_list<
                                    typename EventSequence<E>::Value,
                                    UnitValue<E>,
                                    pyvec<E<T>>>(input);
                        };
                    read.template operator()<Note>(result->notes, notes);
                    read.template operator()<ControlChange>(result->controls, controls);
                    read.template operator()<PitchBend>(result->pitch_bends, pitch_bends);
                    read.template operator()<Pedal>(result->pedals, pedals);
                    return TrackValue(std::move(result));
                }));
            },
            "name"_a        = "",
            "program"_a     = 0,
            "is_drum"_a     = false,
            "notes"_a       = nb::none(),
            "controls"_a    = nb::none(),
            "pitch_bends"_a = nb::none(),
            "pedals"_a      = nb::none(),
            nb::kw_only(),
            "ttype"_a = nb::type<Tick>()
    )
        .def(
            "__init__",
            [](TrackValue* self, const TrackValue& other) {
                new (self) TrackValue(other.visit([](const auto& ptr) {
                    return TrackValue(
                        std::make_shared<Track<UnitOf<decltype(ptr)>>>(ptr->deepcopy())
                    );
                }));
            },
            "other"_a,
            kTrackCopyCtorDoc
        );
    bind_object_ops(type, [](const auto& track) {
        return std::make_shared<std::decay_t<decltype(track)>>(track.deepcopy());
    });
    bind_scalar_property(
        type, "name", [](auto& track) -> auto& { return track.name; }, "Track name."
    );
    bind_scalar_property(
        type, "program", [](auto& track) -> auto& { return track.program; }, "General MIDI program."
    );
    bind_scalar_property(
        type,
        "is_drum",
        [](auto& track) -> auto& { return track.is_drum; },
        "Whether the track plays on the drum channel."
    );
    const auto bind_events = [&]<template<class> class E>(
                                 const char* name, auto field, const char* doc
                             ) {
        bind_list_property<typename EventSequence<E>::Value, UnitValue<E>>(type, name, field, doc);
    };
    bind_events.template operator()<Note>(
        "notes", [](auto& track) -> auto& { return track.notes; }, "The note events."
    );
    bind_events.template operator()<ControlChange>(
        "controls", [](auto& track) -> auto& { return track.controls; }, "The control changes."
    );
    bind_events.template operator()<PitchBend>(
        "pitch_bends", [](auto& track) -> auto& { return track.pitch_bends; }, "The pitch bends."
    );
    bind_events.template operator()<Pedal>(
        "pedals", [](auto& track) -> auto& { return track.pedals; }, "The sustain pedal regions."
    );
    bind_events.template operator()<TextMeta>(
        "lyrics", [](auto& track) -> auto& { return track.lyrics; }, "The lyric events."
    );

    bind_queries(type, kTrackStartDoc, kTrackEndDoc, kTrackNoteNumDoc, kTrackEmptyDoc);
    bind_transformations(
        type,
        kTrackSortDoc,
        kTrackClipDoc,
        kTrackAdjustTimeDoc,
        kTrackShiftTimeDoc,
        kTrackShiftPitchDoc,
        kTrackShiftVelocityDoc
    );
    type.def(
        "trim",
        [](nb::handle         self,
           nb::handle         start,
           nb::handle         end,
           nb::handle         min_overlap,
           const std::string& start_mode,
           const std::string& end_mode,
           bool               inplace) {
            return edit<TrackValue>(self, inplace, [&](auto& track, auto unit) {
                using T = decltype(unit);
                track.trim_inplace(
                    read_time<T>(start),
                    read_time<T>(end),
                    read_time<T>(min_overlap),
                    start_mode,
                    end_mode
                );
            });
        },
        "start"_a,
        "end"_a,
        "min_overlap"_a = 0,
        "start_mode"_a  = "remove",
        "end_mode"_a    = "remove",
        "inplace"_a     = false,
        nb::sig(
            "def trim(self, start: int | float, end: int | float, min_overlap: int | float = 0, "
            "start_mode: str = 'remove', end_mode: str = 'remove', inplace: bool = False) -> "
            "typing.Self"
        ),
        kTrackTrimDoc
    );
    bind_pianoroll<TrackPianoroll>(type, kTrackPianorollDoc);

    bind_list_ops<TrackSequence, TrackValue>(list);
    bind_list_algorithms<TrackList, TrackValue>(list);
    bind_pickle(
        list,
        [](const auto& ptr) {
            return detail::dump_track_list<UnitOf<decltype(ptr)>, SharedStorage>(ptr);
        },
        [](auto unit, std::span<const u8> data) {
            using T     = decltype(unit);
            auto tracks = parse<DataFormat::ZPP, vec<symusic::Track<T>>>(data);
            auto result = std::make_shared<TrackSequence<T>>();
            result->reserve(tracks.size());
            for (auto& track : tracks) {
                result->push_back(
                    std::make_shared<Track<T>>(
                        detail::adopt_storage<SharedStorage>(std::move(track))
                    )
                );
            }
            return result;
        }
    );
}

}   // namespace symusic::py
