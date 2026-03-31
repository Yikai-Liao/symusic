#pragma once

#include <filesystem>
#include <string_view>
#include <vector>

namespace symusic::process_runner {

class ScopedTempDir {
public:
    explicit ScopedTempDir(std::string_view prefix = "symusic-abc-");
    ScopedTempDir(const ScopedTempDir&)                = delete;
    ScopedTempDir& operator=(const ScopedTempDir&)     = delete;
    ScopedTempDir(ScopedTempDir&& other) noexcept;
    ScopedTempDir& operator=(ScopedTempDir&& other) noexcept;
    ~ScopedTempDir();

    [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

private:
    std::filesystem::path path_;
};

void run_process_checked(
    const std::filesystem::path&              executable,
    const std::vector<std::filesystem::path>& args,
    bool                                      suppress_stderr
);

}   // namespace symusic::process_runner
