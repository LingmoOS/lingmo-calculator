// SPDX-FileCopyrightText: 2026 LingmoOS Contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <string>

namespace calculator {

enum class Base { Hex = 16, Dec = 10, Oct = 8, Bin = 2 };
enum class BitWidth { Bits64 = 64, Bits32 = 32, Bits16 = 16, Bits8 = 8 };

struct ProgrammerResult
{
    bool ok = false;
    std::string error;
    int64_t value = 0;

    std::string hex() const;
    std::string dec() const;
    std::string oct() const;
    std::string bin() const;
    // Formatted in a given base.
    std::string inBase(Base base) const;
};

// Integer-only arithmetic with bitwise operations. Inputs are strings in
// the current base; the value is stored as a signed 64-bit integer and
// masked to the chosen width before output.
class ProgrammerCalculator
{
public:
    ProgrammerCalculator() = default;

    void setBase(Base base) { m_base = base; }
    Base base() const { return m_base; }
    void setBitWidth(BitWidth width) { m_width = width; }
    BitWidth bitWidth() const { return m_width; }

    // Evaluates expression. Supports + - * / % & | ^ ~ << >> and parentheses.
    // Identifiers A-F are hex digits when base is Hex.
    ProgrammerResult evaluate(const std::string& expression) const;

    // Parse a single integer token in the current base.
    bool parse(const std::string& text, int64_t& out) const;

    // Mask the value to the current bit width, treating it as signed.
    int64_t mask(int64_t value) const;

    // Format helpers, base-prefixed (0x, 0o, 0b) except decimal.
    static std::string formatHex(int64_t value);
    static std::string formatDec(int64_t value);
    static std::string formatOct(int64_t value);
    static std::string formatBin(int64_t value, int groupSize = 4);

private:
    Base m_base = Base::Dec;
    BitWidth m_width = BitWidth::Bits64;
};

} // namespace calculator
