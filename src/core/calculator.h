// SPDX-FileCopyrightText: 2026 LingmoOS Contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <string>

#include "hmath.h"

namespace calculator {

// What can go wrong while an expression is read. The message for each is the
// application's to translate.
enum class Error {
    None = 0,
    Empty,
    TooLong,
    Syntax,
    UnmatchedParenthesis,
    DivisionByZero,
    ExponentTooLarge,
    Undefined,
};

struct Result
{
    bool ok = false;
    // Meaningful when ok.
    HNumber value;
    // The number as the display shows it, empty unless ok.
    std::string text;
    // None when ok.
    Error error = Error::None;
};

// Whether trig functions receive/return degrees or radians.
enum class AngleUnit { Radians, Degrees };

// A small exact expression evaluator: numbers, + - * / ^, a postfix % (a
// hundredth) and unary signs, with parentheses. It works on HMath's
// arbitrary-precision decimals, so 0.1 + 0.2 is exactly 0.3 and 1 / 3 keeps
// ten decimals. No variables, no functions, no floating point.
class Calculator
{
public:
    // The longest expression taken, after normalization.
    static constexpr size_t kMaximumLength = 256;
    // How deeply parentheses may nest.
    static constexpr int kMaximumDepth = 128;
    // The largest exponent ^ takes, either way; bigger ones would print a
    // number without end.
    static constexpr int kMaximumExponent = 1000;

    Result evaluate(const std::string& expression) const;

    // The number as the calculator shows it: up to ten decimals, in
    // scientific notation past seventeen whole digits or below the smallest
    // shown fraction. Never "-0".
    static std::string format(const HNumber& number);

    // ×, ÷ and the Unicode minus signs become the ASCII operators; other
    // control characters go; the ends are trimmed.
    static std::string normalize(const std::string& expression);

    // Drops a trailing '=' and closes the parentheses left open, so
    // "2*(3+4" evaluates as 14.
    static std::string autoFix(const std::string& expression);
};

// Extends Calculator with named functions (sin, cos, log, sqrt, …),
// constants (pi, e) and an angle unit switch for trig functions.
// The standard four-function grammar is a strict subset.
class ScientificCalculator
{
public:
    explicit ScientificCalculator(AngleUnit unit = AngleUnit::Radians);

    void setAngleUnit(AngleUnit unit) { m_unit = unit; }
    AngleUnit angleUnit() const { return m_unit; }

    // Same semantics as Calculator::evaluate plus functions and constants.
    Result evaluate(const std::string& expression) const;

    // Evaluate a double for graph sampling; quiet NaN on error.
    double evaluateDouble(const std::string& expression, const std::string& variable,
                          double value) const;

private:
    AngleUnit m_unit;
};

} // namespace calculator
