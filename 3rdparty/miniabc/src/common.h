#ifndef MINIABC_COMMON_H
#define MINIABC_COMMON_H

#include "miniabc/miniabc.h"
#include <array>
#include <string>
#include <string_view>
#include <optional>

namespace miniabc {

// Utility Functions
std::string_view   trim(std::string_view s);
std::optional<int> parse_integer(std::string_view s);
Fraction           parse_fraction(std::string_view s);
int                letter_index(char letter);
std::array<int, 7> build_key_signature(const KeySignature& ksig);

// Constants
constexpr std::string_view   LETTERS             = "CDEFGAB";
constexpr std::array<int, 7> NATURAL_PITCH_CLASS = {0, 2, 4, 5, 7, 9, 11};

}   // namespace miniabc

#endif   // MINIABC_COMMON_H
