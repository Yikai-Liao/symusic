#include "common.h"
#include <charconv>
#include <algorithm>
#include <stdexcept>
#include <cctype>
#include <cmath>

namespace miniabc {

constexpr bool is_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v';
}

std::string_view trim(std::string_view view) {
    while (!view.empty() && is_space(view.front())) { view.remove_prefix(1); }
    while (!view.empty() && is_space(view.back())) { view.remove_suffix(1); }
    return view;
}

std::optional<int> parse_integer(std::string_view text) {
    if (text.empty()) { return std::nullopt; }
    int  value = 0;
    auto res   = std::from_chars(text.data(), text.data() + text.size(), value);
    if (res.ec == std::errc{}) { return value; }
    return std::nullopt;
}

Fraction parse_fraction(std::string_view text) {
    auto slash = text.find('/');
    if (slash == std::string::npos) {
        if (auto number = parse_integer(trim(text))) { return make_fraction(*number, 1); }
        throw std::runtime_error("miniabc: invalid fraction '" + std::string(text) + "'");
    }
    auto lhs = trim(text.substr(0, slash));
    auto rhs = trim(text.substr(slash + 1));
    auto num = parse_integer(lhs);
    auto den = parse_integer(rhs);
    if (!num || !den || *den == 0) {
        throw std::runtime_error("miniabc: invalid fraction '" + std::string(text) + "'");
    }
    return make_fraction(*num, *den);
}

int letter_index(const char letter) {
    const char upper = static_cast<char>(std::toupper(static_cast<unsigned char>(letter)));
    switch (upper) {
    case 'C': return 0;
    case 'D': return 1;
    case 'E': return 2;
    case 'F': return 3;
    case 'G': return 4;
    case 'A': return 5;
    case 'B': return 6;
    default: throw std::runtime_error("miniabc: unsupported pitch letter");
    }
}

const std::array<char, 7> SHARP_ORDER = {'F', 'C', 'G', 'D', 'A', 'E', 'B'};
const std::array<char, 7> FLAT_ORDER  = {'B', 'E', 'A', 'D', 'G', 'C', 'F'};

std::array<int, 7> build_key_signature(const KeySignature& ksig) {
    std::array<int, 7> table{};
    table.fill(0);
    if (ksig.accidentals == 0) { return table; }
    const bool sharp = ksig.accidentals > 0;
    const auto count = std::min<int>(7, std::abs(ksig.accidentals));
    for (auto idx = 0; idx < count; ++idx) {
        const char letter = sharp ? SHARP_ORDER[idx] : FLAT_ORDER[idx];
        const auto pos    = letter_index(letter);
        table[pos]        = sharp ? 1 : -1;
    }
    return table;
}

}   // namespace miniabc
