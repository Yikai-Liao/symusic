#include "process_runner.h"

#include <chrono>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <fmt/format.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;
#endif

namespace symusic::process_runner {

namespace {

std::string path_to_utf8(const std::filesystem::path& path) {
#ifdef _WIN32
    const auto utf8 = path.u8string();
    return std::string(reinterpret_cast<const char*>(utf8.c_str()), utf8.size());
#else
    return path.string();
#endif
}

std::string render_command(
    const std::filesystem::path&              executable,
    const std::vector<std::filesystem::path>& args
) {
    std::string rendered = path_to_utf8(executable);
    for (const auto& arg : args) {
        rendered += " ";
        rendered += path_to_utf8(arg);
    }
    return rendered;
}

std::filesystem::path create_scoped_temp_dir(std::string_view prefix) {
    const auto temp_root = std::filesystem::temp_directory_path();

#ifdef _WIN32
    for (int attempt = 0; attempt < 128; ++attempt) {
        const auto now = static_cast<unsigned long long>(
            std::chrono::steady_clock::now().time_since_epoch().count()
        );
        const auto candidate
            = temp_root
              / fmt::format("{}{}-{}-{}", prefix, GetCurrentProcessId(), now, attempt);
        if (CreateDirectoryW(candidate.c_str(), nullptr) != 0) { return candidate; }

        const auto error = GetLastError();
        if (error == ERROR_ALREADY_EXISTS) { continue; }

        throw std::runtime_error(
            fmt::format(
                "CreateDirectoryW failed for {} (error {})",
                path_to_utf8(candidate),
                error
            )
        );
    }

    throw std::runtime_error(
        fmt::format(
            "Failed to create a unique temporary directory under {}",
            path_to_utf8(temp_root)
        )
    );
#else
    const auto pattern = temp_root / fmt::format("{}XXXXXX", prefix);
    auto       native  = pattern.string();
    std::vector<char> buffer(native.begin(), native.end());
    buffer.push_back('\0');

    if (::mkdtemp(buffer.data()) == nullptr) {
        throw std::runtime_error(
            fmt::format("mkdtemp failed for {}: {}", native, std::strerror(errno))
        );
    }

    return std::filesystem::path(buffer.data());
#endif
}

#ifdef _WIN32
std::wstring quote_windows_argument(const std::wstring& argument) {
    if (argument.empty()) { return L"\"\""; }

    if (argument.find_first_of(L" \t\n\v\"") == std::wstring::npos) { return argument; }

    std::wstring quoted;
    quoted.push_back(L'"');
    size_t backslashes = 0;

    for (const auto ch : argument) {
        if (ch == L'\\') {
            ++backslashes;
            continue;
        }

        if (ch == L'"') {
            quoted.append(backslashes * 2 + 1, L'\\');
            quoted.push_back(L'"');
            backslashes = 0;
            continue;
        }

        quoted.append(backslashes, L'\\');
        backslashes = 0;
        quoted.push_back(ch);
    }

    quoted.append(backslashes * 2, L'\\');
    quoted.push_back(L'"');
    return quoted;
}

std::wstring build_command_line(
    const std::filesystem::path&              executable,
    const std::vector<std::filesystem::path>& args
) {
    std::wstring command_line = quote_windows_argument(executable.native());
    for (const auto& arg : args) {
        command_line.push_back(L' ');
        command_line += quote_windows_argument(arg.native());
    }
    return command_line;
}

HANDLE duplicate_inheritable_handle(HANDLE handle, const char* label) {
    if (handle == nullptr || handle == INVALID_HANDLE_VALUE) {
        throw std::runtime_error(fmt::format("Failed to resolve {}", label));
    }

    HANDLE duplicated = nullptr;
    if (DuplicateHandle(
            GetCurrentProcess(),
            handle,
            GetCurrentProcess(),
            &duplicated,
            0,
            TRUE,
            DUPLICATE_SAME_ACCESS
        )
        == 0) {
        throw std::runtime_error(
            fmt::format("DuplicateHandle failed for {} (error {})", label, GetLastError())
        );
    }

    return duplicated;
}
#endif

}   // namespace

ScopedTempDir::ScopedTempDir(std::string_view prefix) : path_(create_scoped_temp_dir(prefix)) {}

ScopedTempDir::ScopedTempDir(ScopedTempDir&& other) noexcept : path_(std::move(other.path_)) {
    other.path_.clear();
}

ScopedTempDir& ScopedTempDir::operator=(ScopedTempDir&& other) noexcept {
    if (this == &other) { return *this; }

    std::error_code error;
    if (!path_.empty()) { std::filesystem::remove_all(path_, error); }

    path_ = std::move(other.path_);
    other.path_.clear();
    return *this;
}

ScopedTempDir::~ScopedTempDir() {
    std::error_code error;
    if (!path_.empty()) { std::filesystem::remove_all(path_, error); }
}

void run_process_checked(
    const std::filesystem::path&              executable,
    const std::vector<std::filesystem::path>& args,
    const bool                                suppress_stderr
) {
    const auto rendered_command = render_command(executable, args);

#ifdef _WIN32
    auto command_line = build_command_line(executable, args);
    std::vector<wchar_t> command_buffer(command_line.begin(), command_line.end());
    command_buffer.push_back(L'\0');

    STARTUPINFOW        startup_info{};
    PROCESS_INFORMATION process_info{};
    startup_info.cb = sizeof(startup_info);

    HANDLE stdin_handle  = nullptr;
    HANDLE stdout_handle = nullptr;
    HANDLE stderr_handle = nullptr;
    BOOL   inherit       = FALSE;

    if (suppress_stderr) {
        stdin_handle  = duplicate_inheritable_handle(GetStdHandle(STD_INPUT_HANDLE), "stdin");
        stdout_handle = duplicate_inheritable_handle(GetStdHandle(STD_OUTPUT_HANDLE), "stdout");
        stderr_handle = CreateFileW(
            L"NUL",
            GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        );
        if (stderr_handle == INVALID_HANDLE_VALUE) {
            if (stdin_handle != nullptr) { CloseHandle(stdin_handle); }
            if (stdout_handle != nullptr) { CloseHandle(stdout_handle); }
            throw std::runtime_error(
                fmt::format("CreateFileW failed for NUL (error {})", GetLastError())
            );
        }
        SetHandleInformation(stderr_handle, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);

        startup_info.dwFlags |= STARTF_USESTDHANDLES;
        startup_info.hStdInput  = stdin_handle;
        startup_info.hStdOutput = stdout_handle;
        startup_info.hStdError  = stderr_handle;
        inherit                 = TRUE;
    }

    const BOOL created = CreateProcessW(
        executable.c_str(),
        command_buffer.data(),
        nullptr,
        nullptr,
        inherit,
        0,
        nullptr,
        nullptr,
        &startup_info,
        &process_info
    );

    if (stdin_handle != nullptr) { CloseHandle(stdin_handle); }
    if (stdout_handle != nullptr) { CloseHandle(stdout_handle); }
    if (stderr_handle != nullptr && stderr_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(stderr_handle);
    }

    if (created == 0) {
        throw std::runtime_error(
            fmt::format(
                "Failed to start process '{}' (error {})",
                rendered_command,
                GetLastError()
            )
        );
    }

    WaitForSingleObject(process_info.hProcess, INFINITE);

    DWORD exit_code = 0;
    if (GetExitCodeProcess(process_info.hProcess, &exit_code) == 0) {
        const auto error = GetLastError();
        CloseHandle(process_info.hThread);
        CloseHandle(process_info.hProcess);
        throw std::runtime_error(
            fmt::format(
                "Failed to query process exit code for '{}' (error {})",
                rendered_command,
                error
            )
        );
    }

    CloseHandle(process_info.hThread);
    CloseHandle(process_info.hProcess);

    if (exit_code != 0) {
        throw std::runtime_error(
            fmt::format("Process exited with code {}: {}", exit_code, rendered_command)
        );
    }
#else
    std::vector<std::string> argv_storage;
    argv_storage.reserve(args.size() + 1);
    argv_storage.push_back(executable.string());
    for (const auto& arg : args) {
        argv_storage.push_back(arg.string());
    }

    std::vector<char*> argv;
    argv.reserve(argv_storage.size() + 1);
    for (auto& arg : argv_storage) {
        argv.push_back(arg.data());
    }
    argv.push_back(nullptr);

    posix_spawn_file_actions_t file_actions{};
    posix_spawn_file_actions_t* file_actions_ptr = nullptr;
    if (suppress_stderr) {
        int action_status = posix_spawn_file_actions_init(&file_actions);
        if (action_status != 0) {
            throw std::runtime_error(
                fmt::format(
                    "posix_spawn_file_actions_init failed for '{}': {}",
                    rendered_command,
                    std::strerror(action_status)
                )
            );
        }
        action_status = posix_spawn_file_actions_addopen(
            &file_actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0
        );
        if (action_status != 0) {
            posix_spawn_file_actions_destroy(&file_actions);
            throw std::runtime_error(
                fmt::format(
                    "posix_spawn_file_actions_addopen failed for '{}': {}",
                    rendered_command,
                    std::strerror(action_status)
                )
            );
        }
        file_actions_ptr = &file_actions;
    }

    pid_t process_id    = -1;
    const int spawn_err = posix_spawn(
        &process_id,
        executable.c_str(),
        file_actions_ptr,
        nullptr,
        argv.data(),
        environ
    );

    if (file_actions_ptr != nullptr) { posix_spawn_file_actions_destroy(file_actions_ptr); }

    if (spawn_err != 0) {
        throw std::runtime_error(
            fmt::format(
                "Failed to start process '{}': {}",
                rendered_command,
                std::strerror(spawn_err)
            )
        );
    }

    int wait_status = 0;
    while (waitpid(process_id, &wait_status, 0) == -1) {
        if (errno != EINTR) {
            throw std::runtime_error(
                fmt::format(
                    "waitpid failed for '{}': {}",
                    rendered_command,
                    std::strerror(errno)
                )
            );
        }
    }

    if (!WIFEXITED(wait_status) || WEXITSTATUS(wait_status) != 0) {
        const auto exit_code = WIFEXITED(wait_status) ? WEXITSTATUS(wait_status) : -1;
        throw std::runtime_error(
            fmt::format("Process exited with code {}: {}", exit_code, rendered_command)
        );
    }
#endif
}

}   // namespace symusic::process_runner
