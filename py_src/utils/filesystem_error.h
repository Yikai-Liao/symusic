#pragma once

#include <cerrno>
#include <exception>
#include <filesystem>

#include <nanobind/nanobind.h>

namespace symusic::py {

inline void register_filesystem_error_translator() {
    nanobind::register_exception_translator([](const std::exception_ptr& exception, void*) {
        try {
            std::rethrow_exception(exception);
        } catch (const std::filesystem::filesystem_error& error) {
            const auto&      native_path = error.path1().native();
            nanobind::object filename;
            if (!native_path.empty()) {
                const auto length = static_cast<Py_ssize_t>(native_path.size());
#ifdef _WIN32
                filename = nanobind::steal(PyUnicode_FromWideChar(native_path.data(), length));
#else
                filename
                    = nanobind::steal(PyUnicode_DecodeFSDefaultAndSize(native_path.data(), length));
#endif
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
