//
// Created by lyk on 23-9-20.
//
#include <string>
#include <pybind11/pybind11.h>
#include <pybind11/stl_bind.h>
#include <pybind11/functional.h>
#include <pybind11/eigen.h>
#include "Score.hpp"

namespace py = pybind11;
using namespace score;

PYBIND11_MAKE_OPAQUE(std::vector<Track>)
PYBIND11_MAKE_OPAQUE(std::vector<Note>)
PYBIND11_MAKE_OPAQUE(std::vector<ControlChange>)
PYBIND11_MAKE_OPAQUE(std::vector<TimeSignature>)
PYBIND11_MAKE_OPAQUE(std::vector<KeySignature>)
PYBIND11_MAKE_OPAQUE(std::vector<Tempo>)
PYBIND11_MAKE_OPAQUE(std::vector<PitchBend>)
PYBIND11_MAKE_OPAQUE(std::vector<TextMeta>)
PYBIND11_MAKE_OPAQUE(std::unordered_map<uint8_t, std::vector<ControlChange>>)

PYBIND11_MODULE(symusic, m) {
    py::class_<Note>(m, "Note")
        .def(py::init<float, float, int8_t, int8_t>())
        .def(py::init<const Note &>(), "Copy constructor")
        .def_readwrite("start", &Note::start)
        .def_readwrite("duration", &Note::duration)
        .def_readwrite("pitch", &Note::pitch)
        .def_readwrite("velocity", &Note::velocity)
        .def_property_readonly("end", &Note::end_time)
        .def("empty", &Note::empty, "duration <= 0 or velocity <= 0")
        .def("copy", &Note::copy, "Deep copy the note", py::return_value_policy::move)
        .def("__copy__", &Note::copy, "Deep copy the note")
        .def("__deepcopy__", &Note::copy, "Deep copy the note")
        .def("end_time", &Note::end_time)
        .def("shift_pitch", &Note::shift_pitch)
        .def("shift_velocity", &Note::shift_velocity)
        .def("__repr__", [](const Note &self) {
            return "<Note start=" + std::to_string(self.start) +
                   ", dur=" + std::to_string(self.duration) +
                   ", pitch=" + std::to_string((int) self.pitch) +
                   ", vel=" + std::to_string((int) self.velocity) + ">";
        });

    py::class_<TimeSignature>(m, "TimeSignature")
        .def(py::init<float, uint8_t, uint8_t>())
        .def(py::init<const TimeSignature &>(), "Copy constructor")
        .def_readwrite("time", &score::TimeSignature::time)
        .def_readwrite("numerator", &score::TimeSignature::numerator)
        .def_readwrite("denominator", &score::TimeSignature::denominator)
        .def("copy", &score::TimeSignature::copy, "Deep copy", py::return_value_policy::move)
        .def("__copy__", &score::TimeSignature::copy, "Deep copy")
        .def("__deepcopy__", &score::TimeSignature::copy, "Deep copy")
        .def("__repr__", [](TimeSignature &self) {
            return "<TimeSignature time=" + std::to_string(self.time) +
                   ", numerator=" + std::to_string((int) self.numerator) +
                   ", denominator=" + std::to_string((int) self.denominator) + ">";
        });

    py::class_<KeySignature>(m, "KeySignature")
        .def(py::init<float, int8_t, uint8_t>())
        .def(py::init<const KeySignature &>(), "Copy constructor")
        .def_readwrite("time", &score::KeySignature::time)
        .def_readwrite("key", &score::KeySignature::key)
        .def_readwrite("tonality", &score::KeySignature::tonality)
        .def("copy", &score::KeySignature::copy, "Deep copy", py::return_value_policy::move)
        .def("__copy__", &score::KeySignature::copy, "Deep copy")
        .def("__deepcopy__", &score::KeySignature::copy, "Deep copy")
        .def("__repr__", &score::KeySignature::to_string);

    py::class_<score::ControlChange>(m, "ControlChange")
        .def(py::init<float, uint8_t, uint8_t>())
        .def(py::init<const score::ControlChange &>(), "Copy constructor")
        .def_readwrite("time", &score::ControlChange::time)
        .def_readwrite("value", &score::ControlChange::value)
        .def("copy", &score::ControlChange::copy, "Deep copy", py::return_value_policy::move)
        .def("__copy__", &score::ControlChange::copy, "Deep copy")
        .def("__deepcopy__", &score::ControlChange::copy, "Deep copy")
        .def("__repr__", [](score::ControlChange &self) {
            return "<ControlChange time=" + std::to_string(self.time) +
                   ", value=" + std::to_string((int) self.value) + ">";
        });

    py::class_<score::Tempo>(m, "Tempo")
        .def(py::init<float, float>())
        .def(py::init<const score::Tempo &>(), "Copy constructor")
        .def_readwrite("time", &score::Tempo::time)
        .def_readwrite("tempo", &score::Tempo::qpm)
        .def("copy", &score::Tempo::copy, "Deep copy", py::return_value_policy::move)
        .def("__copy__", &score::Tempo::copy, "Deep copy")
        .def("__deepcopy__", &score::Tempo::copy, "Deep copy")
        .def("__repr__", [](score::Tempo &self) {
            return "<Tempo time=" + std::to_string(self.time) +
                   ", qpm=" + std::to_string(self.qpm) + ">";
        });

    py::class_<score::PitchBend>(m, "PitchBend")
        .def(py::init<float, uint8_t>())
        .def(py::init<const score::PitchBend &>(), "Copy constructor")
        .def_readwrite("time", &score::PitchBend::time)
        .def_readwrite("value", &score::PitchBend::value)
        .def("copy", &score::PitchBend::copy, "Deep copy", py::return_value_policy::move)
        .def("__copy__", &score::PitchBend::copy, "Deep copy")
        .def("__deepcopy__", &score::PitchBend::copy, "Deep copy")
        .def("__repr__", [](score::PitchBend &self) {
            return "<PitchBend time=" + std::to_string(self.time) +
                   ", value=" + std::to_string(self.value) + ">";
        });

    py::class_<score::TextMeta>(m, "TextMeta")
        .def(py::init<float, std::string &>())
        .def(py::init<const score::TextMeta &>(), "Copy constructor")
        .def_readwrite("time", &score::TextMeta::time)
        .def_readwrite("text", &score::TextMeta::text)
        .def("copy", &score::TextMeta::copy, "Deep copy", py::return_value_policy::move)
        .def("__copy__", &score::TextMeta::copy, "Deep copy")
        .def("__deepcopy__", &score::TextMeta::copy, "Deep copy")
        .def("__repr__", [](score::TextMeta &self) {
            return "<TextMeta time=" + std::to_string(self.time) +
                   ", text=\"" + self.text + "\">";
        });

    py::bind_vector<std::vector<Note>>(m, "NoteList")
        .def("__repr__", [](const std::vector<Note> &self) {
            return "<NoteList length=" + std::to_string(self.size()) + ">";
        });
    py::bind_vector<std::vector<ControlChange>>(m, "ControlChangeList")
        .def("__repr__", [](const std::vector<ControlChange> &self) {
            return "<ControlChangeList length=" + std::to_string(self.size()) + ">";
        });
    py::bind_vector<std::vector<TimeSignature>>(m, "TimeSignatureList")
        .def("__repr__", [](const std::vector<TimeSignature> &self) {
            return "<TimeSignatureList length=" + std::to_string(self.size()) + ">";
        });
    py::bind_vector<std::vector<KeySignature>>(m, "KeySignatureList")
        .def("__repr__", [](const std::vector<KeySignature> &self) {
            return "<KeySignatureList length=" + std::to_string(self.size()) + ">";
        });
    py::bind_vector<std::vector<Tempo>>(m, "TempoList")
        .def("__repr__", [](const std::vector<Tempo> &self) {
            return "<TempoList length=" + std::to_string(self.size()) + ">";
        });

    py::bind_vector<PitchBends>(m, "PitchBendList")
        .def("__repr__", [](const PitchBends &self) {
            return "<PitchBendList length=" + std::to_string(self.size()) + ">";
        });

    py::bind_vector<std::vector<TextMeta>>(m, "TextMetaList")
        .def("__repr__", [](const std::vector<TextMeta> &self) {
            return "<TextMetaList length=" + std::to_string(self.size()) + ">";
        });
    /*
    py::bind_map<std::unordered_map<uint8_t, std::vector<ControlChange>>>(m, "ControlMap")
        .def("__repr__", [](const std::unordered_map<uint8_t, std::vector<ControlChange>> &self) {
            size_t control_num = 0;
            for (const auto &vec: self) control_num += vec.second.size();
            return "<ControlMap control_types=" + std::to_string(self.size())
                   + ", event_num=" + std::to_string(self.size()) + ">";
        });
    */

    auto py_note_arr = py::class_<NoteArray>(m, "NoteArray");

    py::class_<Track>(m, "Track")
        .def(py::init<>())
        .def(py::init<const Track &>(), "Copy constructor")
        .def("copy", &Track::copy, "Deep copy")
        .def("__copy__", &score::Track::copy, "Deep copy")
        .def("__deepcopy__", &score::Track::copy, "Deep copy")
        .def("sort", &Track::sort)
        .def("shift_pitch", &Track::shift_pitch)
        .def("shift_time", &Track::shift_time)
        .def("shift_velocity", &Track::shift_velocity)
        .def("clip", &Track::clip, "Clip notes and controls to a given time range")
        .def("filter_notes", &Track::filter_notes, "Filter notes by a given function")
        .def("note_num", &Track::note_num)
        .def("start_time", &Track::start_time)
        .def("end_time", &Track::end_time)
        .def("get_control_changes", &Track::get_control_changes)
        .def("frame_pianoroll",
             &Track::frame_pianoroll,
             py::arg("quantization") = 16,
             py::return_value_policy::move)
        .def("onset_pianoroll",
             &Track::onset_pianoroll,
             py::arg("quantization") = 16,
             py::return_value_policy::move)
        .def("note_array",
             &Track::note_array,
             py::return_value_policy::move)
        .def_readwrite("name", &Track::name)
        .def_readwrite("program", &Track::program)
        .def_readwrite("is_drum", &Track::is_drum)
        .def_readwrite("notes", &Track::notes)
        .def_readwrite("controls", &Track::controls)
        .def_readwrite("pitch_bends", &Track::pitch_bends)
        .def("__repr__", [](const Track &self) {
            return "<Track name=\"" + self.name
                   + "\", program=" + std::to_string((int) self.program)
                   + ", is_drum=" + (self.is_drum ? "True" : "False")
                   + ", note_num=" + std::to_string(self.note_num())
                   + ", end_time=" + std::to_string(self.end_time())
                   + ">";
        });

    py_note_arr
        .def(py::init<const Track &>())
        .def(py::init<const NoteArray &>())
        .def_readwrite("name", &NoteArray::name)
        .def_readwrite("program", &NoteArray::program)
        .def_readwrite("start", &NoteArray::start)
        .def_readwrite("duration", &NoteArray::duration)
        .def_readwrite("pitch", &NoteArray::pitch)
        .def_readwrite("velocity", &NoteArray::velocity)
        .def("copy", &NoteArray::copy, "Deep copy")
        .def("__copy__", &NoteArray::copy, "Deep copy")
        .def("__deepcopy__", &NoteArray::copy, "Deep copy")
        .def("__repr__", [](const NoteArray &self) {
            return "<NoteArray name=\"" + self.name
                   + "\", program=" + std::to_string((int) self.program)
                   + ", note_num=" + std::to_string(self.start.size())
                   + ">";
        });

    py::bind_vector<std::vector<Track>>(m, "TrackList")
        .def("__repr__", [](const std::vector<Track> &self) {
            return "<TrackList length=" + std::to_string(self.size()) + ">";
        });

    py::class_<Score>(m, "Score")
        .def(py::init<>())
        .def(py::init(&Score::from_file))
        .def(py::init<const Score &>(), "Copy constructor")
        .def("copy", &Score::copy)
        .def("__copy__", &score::Score::copy, "Deep copy")
        .def("__deepcopy__", &score::Score::copy, "Deep copy")
        .def_property_readonly_static("from_file", [](const py::object &) {
            return py::cpp_function([](std::string &x) { return Score::from_file(x); });
        })  // binding class method in an erratic way: https://github.com/pybind/pybind11/issues/1693
        .def("sort", &Score::sort)
        .def("shift_pitch", &Score::shift_pitch)
        .def("shift_time", &Score::shift_time)
        .def("shift_velocity", &Score::shift_velocity)
        .def("clip", &Score::clip, "Clip events in the score to a given time range")
        .def("filter_notes", &Score::filter_notes, "Filter notes by a given function")
        .def("note_num", &Score::note_num)
        .def("end_time", &Score::end_time)
        .def("start_time", &Score::start_time)
        .def_readwrite("tracks", &Score::tracks)
        .def_readwrite("time_signatures", &Score::time_signatures)
        .def_readwrite("key_signatures", &Score::key_signatures)
        .def_readwrite("tempos", &Score::tempos)
        .def_readwrite("lyrics", &Score::lyrics)
        .def_readwrite("markers", &Score::markers)
        .def("__repr__", [](const Score &self) {
            return "<Score track_num="
                   + std::to_string(self.tracks.size())
                   + ", note_num=" + std::to_string(self.note_num())
                   + ", end_time=" + std::to_string(self.end_time())
                   + ">";
        });


#ifdef VERSION_INFO
    m.attr("__version__") = MACRO_STRINGIFY(VERSION_INFO);
#else
    m.attr("__version__") = "0.0.6";
#endif
}