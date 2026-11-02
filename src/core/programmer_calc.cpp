// SPDX-FileCopyrightText: 2026 LingmoOS Contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/programmer_calc.h"

#include <algorithm>
#include <bitset>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace calculator {

namespace {

struct IntParseError {};

int digitValue(char c, int base)
{
    int v = -1;
    if (c >= '0' && c <= '9')
        v = c - '0';
    else if (c >= 'a' && c <= 'f')
        v = c - 'a' + 10;
    else if (c >= 'A' && c <= 'F')
        v = c - 'A' + 10;
    if (v < 0 || v >= base)
        return -1;
    return v;
}

// Minimal recursive descent for integer expressions with bit-ops.
// Grammar:
//   expr    := bitwise (('+' | '-') bitwise)*
//   bitwise := shift (('&' | '|' | 'xor') shift)*
//   shift   := term (('<<' | '>>') term)*
//   term    := unary (('*' | '/' | '%') unary)*
//   unary   := ('~' | '-' | '+') unary | primary
//   primary := number | '(' expr ')'
class IntParser
{
public:
    IntParser(std::string_view text, int base) : m_text(text), m_base(base) {}

    int64_t parse()
    {
        skip();
        if (m_pos >= m_text.size())
            throw IntParseError{};
        const int64_t v = parseExpr();
        skip();
        if (m_pos < m_text.size())
            throw IntParseError{};
        return v;
    }

private:
    void skip()
    {
        while (m_pos < m_text.size() && m_text[m_pos] == ' ')
            ++m_pos;
    }

    bool consume(char c)
    {
        skip();
        if (m_pos < m_text.size() && m_text[m_pos] == c) {
            ++m_pos;
            return true;
        }
        return false;
    }

    bool peek(char c)
    {
        skip();
        return m_pos < m_text.size() && m_text[m_pos] == c;
    }

    int64_t parseExpr()
    {
        int64_t v = parseBitwise();
        for (;;) {
            if (consume('+'))
                v += parseBitwise();
            else if (consume('-'))
                v -= parseBitwise();
            else
                return v;
        }
    }

    int64_t parseBitwise()
    {
        int64_t v = parseShift();
        for (;;) {
            skip();
            // Check for 'xor' keyword before '|' to avoid a partial match.
            if (m_pos + 2 < m_text.size()
                && std::tolower(m_text[m_pos]) == 'x'
                && std::tolower(m_text[m_pos + 1]) == 'o'
                && std::tolower(m_text[m_pos + 2]) == 'r') {
                m_pos += 3;
                v ^= parseShift();
            } else if (consume('&')) {
                v &= parseShift();
            } else if (peek('|') && (m_pos + 1 >= m_text.size() || m_text[m_pos + 1] != '|')) {
                ++m_pos;
                v |= parseShift();
            } else {
                return v;
            }
        }
    }

    int64_t parseShift()
    {
        int64_t v = parseTerm();
        for (;;) {
            skip();
            if (m_pos + 1 < m_text.size() && m_text[m_pos] == '<' && m_text[m_pos + 1] == '<') {
                m_pos += 2;
                const int64_t n = parseTerm();
                v = (n >= 0 && n < 64) ? (v << n) : 0;
            } else if (m_pos + 1 < m_text.size() && m_text[m_pos] == '>' && m_text[m_pos + 1] == '>') {
                m_pos += 2;
                const int64_t n = parseTerm();
                v = (n >= 0 && n < 64) ? (v >> n) : 0;
            } else {
                return v;
            }
        }
    }

    int64_t parseTerm()
    {
        int64_t v = parseUnary();
        for (;;) {
            if (consume('*')) {
                v *= parseUnary();
            } else if (consume('/')) {
                const int64_t d = parseUnary();
                if (d == 0)
                    throw IntParseError{};
                v /= d;
            } else if (consume('%')) {
                const int64_t d = parseUnary();
                if (d == 0)
                    throw IntParseError{};
                v %= d;
            } else {
                return v;
            }
        }
    }

    int64_t parseUnary()
    {
        if (consume('~'))
            return ~parseUnary();
        if (consume('-'))
            return -parseUnary();
        if (consume('+'))
            return parseUnary();
        return parsePrimary();
    }

    int64_t parsePrimary()
    {
        skip();
        if (m_pos >= m_text.size())
            throw IntParseError{};
        if (consume('(')) {
            const int64_t v = parseExpr();
            if (!consume(')'))
                throw IntParseError{};
            return v;
        }
        // Skip a base prefix (0x, 0b, 0o).
        if (m_pos + 1 < m_text.size() && m_text[m_pos] == '0') {
            const char next = char(std::tolower(m_text[m_pos + 1]));
            if (next == 'x') { m_pos += 2; return readDigits(16); }
            if (next == 'b') { m_pos += 2; return readDigits(2); }
            if (next == 'o') { m_pos += 2; return readDigits(8); }
        }
        return readDigits(m_base);
    }

    int64_t readDigits(int base)
    {
        const size_t start = m_pos;
        int64_t v = 0;
        while (m_pos < m_text.size()) {
            const int d = digitValue(m_text[m_pos], base);
            if (d < 0)
                break;
            v = v * base + d;
            ++m_pos;
        }
        if (m_pos == start)
            throw IntParseError{};
        return v;
    }

    std::string_view m_text;
    size_t m_pos = 0;
    int m_base;
};

} // namespace

int64_t ProgrammerCalculator::mask(int64_t value) const
{
    const int bits = int(m_width);
    if (bits >= 64)
        return value;
    const uint64_t mask = (uint64_t(1) << bits) - 1;
    const uint64_t masked = uint64_t(value) & mask;
    // Sign-extend.
    const uint64_t sign = uint64_t(1) << (bits - 1);
    return int64_t((masked ^ sign) - sign);
}

ProgrammerResult ProgrammerCalculator::evaluate(const std::string& expression) const
{
    ProgrammerResult result;
    if (expression.empty()) {
        result.error = "Empty";
        return result;
    }
    try {
        IntParser parser(expression, int(m_base));
        result.value = mask(parser.parse());
        result.ok = true;
    } catch (const IntParseError&) {
        result.error = "Syntax error";
    }
    return result;
}

bool ProgrammerCalculator::parse(const std::string& text, int64_t& out) const
{
    try {
        IntParser parser(text, int(m_base));
        out = mask(parser.parse());
        return true;
    } catch (...) {
        return false;
    }
}

std::string ProgrammerResult::hex() const { return ProgrammerCalculator::formatHex(value); }
std::string ProgrammerResult::dec() const { return ProgrammerCalculator::formatDec(value); }
std::string ProgrammerResult::oct() const { return ProgrammerCalculator::formatOct(value); }
std::string ProgrammerResult::bin() const { return ProgrammerCalculator::formatBin(value); }

std::string ProgrammerResult::inBase(Base base) const
{
    switch (base) {
    case Base::Hex: return hex();
    case Base::Dec: return dec();
    case Base::Oct: return oct();
    case Base::Bin: return bin();
    }
    return dec();
}

std::string ProgrammerCalculator::formatHex(int64_t value)
{
    if (value == 0)
        return "0";
    const uint64_t u = uint64_t(value);
    char buf[20];
    std::snprintf(buf, sizeof(buf), "%llX", (unsigned long long)u);
    return buf;
}

std::string ProgrammerCalculator::formatDec(int64_t value)
{
    return std::to_string(value);
}

std::string ProgrammerCalculator::formatOct(int64_t value)
{
    if (value == 0)
        return "0";
    const uint64_t u = uint64_t(value);
    char buf[25];
    std::snprintf(buf, sizeof(buf), "%llo", (unsigned long long)u);
    return buf;
}

std::string ProgrammerCalculator::formatBin(int64_t value, int groupSize)
{
    if (value == 0)
        return "0";
    const uint64_t u = uint64_t(value);
    // Find the highest set bit.
    int highest = 0;
    for (int i = 63; i >= 0; --i) {
        if (u & (uint64_t(1) << i)) { highest = i; break; }
    }
    std::string result;
    result.reserve(highest + 1 + highest / groupSize);
    for (int i = highest; i >= 0; --i) {
        if (!result.empty() && (i + 1) % groupSize == 0)
            result += ' ';
        result += (u & (uint64_t(1) << i)) ? '1' : '0';
    }
    return result;
}

} // namespace calculator
