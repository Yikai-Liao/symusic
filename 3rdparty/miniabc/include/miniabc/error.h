#pragma once

#include <cstddef>
#include <string>

namespace miniabc {

struct SourceLocation {
    std::size_t line   = 0;
    std::size_t column = 0;
};

struct Diagnostic {
    std::size_t line   = 0;
    std::size_t column = 0;
    std::string message;
};

}   // namespace miniabc
