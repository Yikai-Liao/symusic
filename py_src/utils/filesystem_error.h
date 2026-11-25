#pragma once

#include <cerrno>
#include <exception>
#include <filesystem>

#include <nanobind/nanobind.h>

namespace symusic::py {

inline nanobind::str decode_path(const std::filesystem::path& path) {
    const auto& native = path.native();
    const auto  length = static_cast<Py_ssize_t>(native.size());
#ifdef _WIN32
    return nanobind::steal<nanobind::str>(PyUnicode_FromWideChar(native.data(), length));
#else
    return nanobind::steal<nanobind::str>(PyUnicode_DecodeFSDefaultAndSize(native.data(), length));
#endif
}

inline void register_filesystem_error_translator() {
    nanobind::register_exception_translator([](const std::exception_ptr& exception, void*) {
        try {
            std::rethrow_exception(exception);
        } catch (const std::filesystem::filesystem_error& error) {
            nanobind::object filename;
            if (!error.path1().empty()) {
                filename = decode_path(error.path1());
                if (!filename.is_valid()) { return; }
            }
            const int  saved_errno = errno;
            const auto condition   = error.code().default_error_condition();
            errno = condition.category() == std::generic_category() ? condition.value() : EIO;
            PyErr_SetFromErrnoWithFilenameObject(PyExc_OSError, filename.ptr());
            errno = saved_errno;
        }
    });
}

}   // namespace symusic::py
