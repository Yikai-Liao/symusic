#pragma once

#include <cstdint>
#include <numeric>
#include <string>
#include <vector>
#include <cmath>
#include <stdexcept>

namespace miniabc {

struct SourceLocation {
    std::size_t line   = 0;
    std::size_t column = 0;
};

struct Fraction {
    std::int64_t num = 0;
    std::int64_t den = 1;

    bool operator==(const Fraction& other) const = default;
};

constexpr Fraction normalize(const Fraction frac) {
    if (frac.den == 0) { throw std::runtime_error("miniabc: zero denominator"); }
    auto num = frac.num;
    auto den = frac.den;
    if (den < 0) {
        num = -num;
        den = -den;
    }
    auto abs_num = num < 0 ? -num : num;
    auto abs_den = den < 0 ? -den : den;
    auto gcd     = std::gcd(abs_num, abs_den);
    return Fraction{num / gcd, den / gcd};
}

constexpr Fraction make_fraction(const std::int64_t num, const std::int64_t den = 1) {
    return normalize(Fraction{num, den});
}

constexpr Fraction add(const Fraction a, const Fraction b) {
    return normalize(Fraction{a.num * b.den + b.num * a.den, a.den * b.den});
}

constexpr Fraction sub(const Fraction a, const Fraction b) {
    return normalize(Fraction{a.num * b.den - b.num * a.den, a.den * b.den});
}

constexpr Fraction mul(const Fraction a, const Fraction b) {
    return normalize(Fraction{a.num * b.num, a.den * b.den});
}

constexpr Fraction div(const Fraction a, const Fraction b) {
    return normalize(Fraction{a.num * b.den, a.den * b.num});
}

constexpr std::int64_t mul_div(const Fraction frac, const std::int64_t scale) {
    return (frac.num * scale) / frac.den;
}

struct Diagnostic {
    std::size_t line   = 0;
    std::size_t column = 0;
    std::string message;
};

}   // namespace miniabc
