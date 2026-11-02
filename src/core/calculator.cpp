// SPDX-FileCopyrightText: 2026 LingmoOS Contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/calculator.h"

#include <cctype>
#include <cstdlib>
#include <string_view>

namespace calculator {

namespace {

// Thrown by the parser, caught by evaluate().
struct ParseError
{
    Error error;
};

// Takes the C string HMath hands out, which the caller frees.
std::string take(char* text)
{
    std::string value = text ? text : "";
    std::free(text);
    return value;
}

// HMath's scientific format rounds its mantissa, which can turn 9.99… into
// "10"; the exponent then belongs one higher.
std::string tidyScientific(const std::string& text)
{
    const size_t e = text.find('e');
    if (e == std::string::npos)
        return text;
    const std::string mantissa = text.substr(0, e);
    const std::string exponent = text.substr(e + 1);
    const bool negative = !mantissa.empty() && mantissa.front() == '-';
    const std::string digits = negative ? mantissa.substr(1) : mantissa;
    if (digits != "10")
        return text;
    const long value = std::strtol(exponent.c_str(), nullptr, 10) + 1;
    return (negative ? "-" : "") + std::string("1e") + std::to_string(value);
}

// A recursive descent parser over an expression already normalized to ASCII:
//
//   expression := term (('+' | '-') term)*
//   term       := unary (('*' | '/') unary)*
//   unary      := ('+' | '-') unary | power
//   power      := postfix ('^' unary)?
//   postfix    := primary '%'*
//   primary    := number | '(' expression ')'
//
// The signs bind less tightly than ^, so -2^2 is -4, and ^ is right
// associative, so 2^3^2 is 512.
class Parser
{
public:
    explicit Parser(std::string_view text)
        : m_text(text)
    {
    }

    HNumber parse()
    {
        skipSpaces();
        if (m_position >= m_text.size())
            throw ParseError{Error::Empty};
        const HNumber value = parseExpression();
        skipSpaces();
        if (m_position < m_text.size()) {
            if (m_text[m_position] == ')')
                throw ParseError{Error::UnmatchedParenthesis};
            throw ParseError{Error::Syntax};
        }
        return value;
    }

private:
    void skipSpaces()
    {
        while (m_position < m_text.size() && m_text[m_position] == ' ')
            ++m_position;
    }

    bool consume(char character)
    {
        skipSpaces();
        if (m_position < m_text.size() && m_text[m_position] == character) {
            ++m_position;
            return true;
        }
        return false;
    }

    HNumber parseExpression()
    {
        HNumber value = parseTerm();
        for (;;) {
            if (consume('+'))
                value = HMath::add(value, parseTerm());
            else if (consume('-'))
                value = HMath::sub(value, parseTerm());
            else
                return value;
        }
    }

    HNumber parseTerm()
    {
        HNumber value = parseUnary();
        for (;;) {
            if (consume('*')) {
                value = HMath::mul(value, parseUnary());
            } else if (consume('/')) {
                const HNumber divisor = parseUnary();
                if (divisor.isZero())
                    throw ParseError{Error::DivisionByZero};
                value = HMath::div(value, divisor);
            } else {
                return value;
            }
        }
    }

    HNumber parseUnary()
    {
        if (consume('+'))
            return parseUnary();
        if (consume('-'))
            return HMath::negate(parseUnary());
        return parsePower();
    }

    HNumber parsePower()
    {
        const HNumber base = parsePostfix();
        if (!consume('^'))
            return base;
        const HNumber exponent = parseUnary();
        if (exponent.isNan())
            throw ParseError{Error::Undefined};
        if (exponent > HNumber(Calculator::kMaximumExponent)
                || exponent < HNumber(-Calculator::kMaximumExponent))
            throw ParseError{Error::ExponentTooLarge};
        return HMath::raise(base, exponent);
    }

    HNumber parsePostfix()
    {
        HNumber value = parsePrimary();
        while (consume('%'))
            value = HMath::div(value, HNumber(100));
        return value;
    }

    HNumber parsePrimary()
    {
        skipSpaces();
        if (m_position >= m_text.size())
            throw ParseError{Error::Syntax};
        if (m_text[m_position] == ')')
            throw ParseError{Error::UnmatchedParenthesis};
        if (consume('(')) {
            if (++m_depth > Calculator::kMaximumDepth)
                throw ParseError{Error::Syntax};
            const HNumber value = parseExpression();
            if (!consume(')'))
                throw ParseError{Error::UnmatchedParenthesis};
            --m_depth;
            return value;
        }
        const char character = m_text[m_position];
        if (!std::isdigit(static_cast<unsigned char>(character)) && character != '.')
            throw ParseError{Error::Syntax};
        return parseNumber();
    }

    HNumber parseNumber()
    {
        const size_t start = m_position;
        bool digit = false;
        bool point = false;
        while (m_position < m_text.size()) {
            const char character = m_text[m_position];
            if (std::isdigit(static_cast<unsigned char>(character))) {
                digit = true;
                ++m_position;
            } else if (character == '.' && !point) {
                point = true;
                ++m_position;
            } else {
                break;
            }
        }
        if (!digit)
            throw ParseError{Error::Syntax};
        const std::string literal(m_text.substr(start, m_position - start));
        const HNumber number(literal.c_str());
        if (number.isNan())
            throw ParseError{Error::Syntax};
        return number;
    }

    std::string_view m_text;
    size_t m_position = 0;
    int m_depth = 0;
};

bool isOperator(char character)
{
    return character == '+' || character == '-' || character == '*' || character == '/'
            || character == '^' || character == '(' || character == ')';
}

} // namespace

Result Calculator::evaluate(const std::string& expression) const
{
    Result result;
    const std::string normalized = normalize(expression);
    if (normalized.size() > kMaximumLength) {
        result.error = Error::TooLong;
        return result;
    }
    if (normalized.empty()) {
        result.error = Error::Empty;
        return result;
    }
    const std::string fixed = autoFix(normalized);
    try {
        Parser parser(fixed);
        HNumber value = parser.parse();
        if (value.isNan()) {
            result.error = Error::Undefined;
            return result;
        }
        result.ok = true;
        result.value = value;
        result.text = format(value);
    } catch (const ParseError& error) {
        result.error = error.error;
    }
    return result;
}

std::string Calculator::format(const HNumber& number)
{
    if (number.isNan())
        return "Error";

    const std::string fixed = take(HMath::formatFixed(number, -1));
    const size_t start = !fixed.empty() && fixed[0] == '-' ? 1 : 0;
    const size_t point = fixed.find('.');
    const size_t whole = (point == std::string::npos ? fixed.size() : point) - start;
    // A nonzero number rounded away at ten decimals: show it in scientific
    // notation rather than as 0.
    const bool roundedAway = fixed == "0" && !number.isZero();
    if (whole > 17 || roundedAway)
        return tidyScientific(take(HMath::formatScientific(number, -1)));
    return fixed;
}

std::string Calculator::normalize(const std::string& expression)
{
    struct Replacement
    {
        std::string_view from;
        char to;
    };
    // UTF-8 for ×, ÷, −, – and —.
    static constexpr Replacement replacements[] = {
        {"\xC3\x97", '*'}, {"\xC3\xB7", '/'}, {"\xE2\x88\x92", '-'},
        {"\xE2\x80\x93", '-'}, {"\xE2\x80\x94", '-'},
    };

    std::string result;
    result.reserve(expression.size());
    size_t position = 0;
    while (position < expression.size()) {
        bool replaced = false;
        const std::string_view rest(expression.data() + position, expression.size() - position);
        for (const Replacement& replacement : replacements) {
            if (rest.starts_with(replacement.from)) {
                result.push_back(replacement.to);
                position += replacement.from.size();
                replaced = true;
                break;
            }
        }
        if (replaced)
            continue;
        const unsigned char character = static_cast<unsigned char>(expression[position]);
        if (character >= 0x20 && character != 0x7F)
            result.push_back(static_cast<char>(character));
        ++position;
    }

    // Trim ASCII spaces from both ends.
    const size_t begin = result.find_first_not_of(' ');
    if (begin == std::string::npos)
        return {};
    const size_t end = result.find_last_not_of(' ');
    return result.substr(begin, end - begin + 1);
}

std::string Calculator::autoFix(const std::string& expression)
{
    std::string fixed = normalize(expression);
    while (!fixed.empty() && fixed.back() == '=')
        fixed.pop_back();
    while (!fixed.empty() && fixed.back() == ' ')
        fixed.pop_back();
    if (fixed.empty())
        return {};

    int balance = 0;
    for (char character : fixed) {
        if (character == '(')
            ++balance;
        else if (character == ')')
            --balance;
    }
    // A close of ours would only hide a typo; opens at the end are a habit.
    if (balance < 0)
        return fixed;

    // The expression must end with something a ')' can follow.
    if (isOperator(fixed.back()))
        return fixed;
    fixed.append(static_cast<size_t>(balance), ')');
    return fixed;
}

// --- ScientificCalculator -------------------------------------------------

namespace {

// A function table entry: a name and a thunk that applies it to its arg.
struct NamedFunction
{
    const char* name;
    HNumber (*apply)(const HNumber&);
    // For trig: the angle conversion to apply before the math call.
    bool trig = false;
};

HNumber toRadians(const HNumber& degrees) { return HMath::div(degrees, HNumber(180)).mul(HMath::pi()); }
HNumber fromRadians(const HNumber& radians)
{
    return HMath::div(radians, HMath::pi()).mul(HNumber(180));
}

// Wraps a 1-arg HMath method into the function-pointer shape the table uses.
HNumber wrapSin(const HNumber& x) { return HMath::sin(x); }
HNumber wrapCos(const HNumber& x) { return HMath::cos(x); }
HNumber wrapTan(const HNumber& x) { return HMath::tan(x); }
HNumber wrapAsin(const HNumber& x) { return HMath::asin(x); }
HNumber wrapAcos(const HNumber& x) { return HMath::acos(x); }
HNumber wrapAtan(const HNumber& x) { return HMath::atan(x); }
HNumber wrapSinh(const HNumber& x) { return HMath::sinh(x); }
HNumber wrapCosh(const HNumber& x) { return HMath::cosh(x); }
HNumber wrapTanh(const HNumber& x) { return HMath::tanh(x); }
HNumber wrapSqrt(const HNumber& x) { return HMath::sqrt(x); }
HNumber wrapCbrt(const HNumber& x) { return HMath::cbrt(x); }
HNumber wrapExp(const HNumber& x) { return HMath::exp(x); }
HNumber wrapLn(const HNumber& x) { return HMath::ln(x); }
HNumber wrapLog(const HNumber& x) { return HMath::log(x); }
HNumber wrapLg(const HNumber& x) { return HMath::lg(x); }
HNumber wrapAbs(const HNumber& x) { return HMath::abs(x); }
HNumber wrapFloor(const HNumber& x) { return HMath::floor(x); }
HNumber wrapCeil(const HNumber& x) { return HMath::ceil(x); }
HNumber wrapInt(const HNumber& x) { return HMath::integer(x); }
HNumber wrapSign(const HNumber& x) { return HMath::sign(x); }
HNumber wrapFactorial(const HNumber& x) { return HMath::factorial(x); }

const NamedFunction kFunctions[] = {
    {"sin",  wrapSin,  true},
    {"cos",  wrapCos,  true},
    {"tan",  wrapTan,  true},
    {"asin", wrapAsin, true},
    {"acos", wrapAcos, true},
    {"atan", wrapAtan, true},
    {"sinh", wrapSinh, false},
    {"cosh", wrapCosh, false},
    {"tanh", wrapTanh, false},
    {"sqrt", wrapSqrt, false},
    {"cbrt", wrapCbrt, false},
    {"exp",  wrapExp,  false},
    {"ln",   wrapLn,   false},
    {"log",  wrapLog,  false},
    {"lg",   wrapLg,   false},
    {"abs",  wrapAbs,  false},
    {"floor",wrapFloor,false},
    {"ceil", wrapCeil, false},
    {"int",  wrapInt,  false},
    {"sign", wrapSign, false},
    {"fact", wrapFactorial, false},
};

const NamedFunction* findFunction(std::string_view name)
{
    for (const NamedFunction& fn : kFunctions) {
        if (name == fn.name)
            return &fn;
    }
    return nullptr;
}

// True when a leading run of letters names a function or a constant; fills
// `out` with the lowercased name and advances `position`.
bool readIdentifier(std::string_view text, size_t& position, std::string& out)
{
    const size_t start = position;
    while (position < text.size() && std::isalpha(static_cast<unsigned char>(text[position])))
        ++position;
    out = std::string(text.substr(start, position - start));
    return !out.empty();
}

// A recursive descent parser that adds function calls and constants to the
// standard grammar:
//   primary := number | '(' expression ')' | identifier '(' expression ')'
//            | identifier (constant)
class ScientificParser
{
public:
    ScientificParser(std::string_view text, AngleUnit unit, std::string variable = {},
                     double varValue = 0.0)
        : m_text(text), m_unit(unit), m_variable(std::move(variable)), m_varValue(varValue) {}

    HNumber parse()
    {
        skipSpaces();
        if (m_position >= m_text.size())
            throw ParseError{Error::Empty};
        const HNumber value = parseExpression();
        skipSpaces();
        if (m_position < m_text.size()) {
            if (m_text[m_position] == ')')
                throw ParseError{Error::UnmatchedParenthesis};
            throw ParseError{Error::Syntax};
        }
        return value;
    }

private:
    void skipSpaces()
    {
        while (m_position < m_text.size() && m_text[m_position] == ' ')
            ++m_position;
    }

    bool consume(char character)
    {
        skipSpaces();
        if (m_position < m_text.size() && m_text[m_position] == character) {
            ++m_position;
            return true;
        }
        return false;
    }

    HNumber parseExpression()
    {
        HNumber value = parseTerm();
        for (;;) {
            if (consume('+'))
                value = HMath::add(value, parseTerm());
            else if (consume('-'))
                value = HMath::sub(value, parseTerm());
            else
                return value;
        }
    }

    HNumber parseTerm()
    {
        HNumber value = parseUnary();
        for (;;) {
            if (consume('*')) {
                value = HMath::mul(value, parseUnary());
            } else if (consume('/')) {
                const HNumber divisor = parseUnary();
                if (divisor.isZero())
                    throw ParseError{Error::DivisionByZero};
                value = HMath::div(value, divisor);
            } else {
                return value;
            }
        }
    }

    HNumber parseUnary()
    {
        if (consume('+'))
            return parseUnary();
        if (consume('-'))
            return HMath::negate(parseUnary());
        return parsePower();
    }

    HNumber parsePower()
    {
        const HNumber base = parsePostfix();
        if (!consume('^'))
            return base;
        const HNumber exponent = parseUnary();
        if (exponent.isNan())
            throw ParseError{Error::Undefined};
        if (exponent > HNumber(Calculator::kMaximumExponent)
                || exponent < HNumber(-Calculator::kMaximumExponent))
            throw ParseError{Error::ExponentTooLarge};
        return HMath::raise(base, exponent);
    }

    HNumber parsePostfix()
    {
        HNumber value = parsePrimary();
        while (consume('%'))
            value = HMath::div(value, HNumber(100));
        return value;
    }

    HNumber parsePrimary()
    {
        skipSpaces();
        if (m_position >= m_text.size())
            throw ParseError{Error::Syntax};
        if (m_text[m_position] == ')')
            throw ParseError{Error::UnmatchedParenthesis};
        if (consume('(')) {
            if (++m_depth > Calculator::kMaximumDepth)
                throw ParseError{Error::Syntax};
            const HNumber value = parseExpression();
            if (!consume(')'))
                throw ParseError{Error::UnmatchedParenthesis};
            --m_depth;
            return value;
        }
        const char character = m_text[m_position];
        if (std::isalpha(static_cast<unsigned char>(character))) {
            std::string name;
            readIdentifier(m_text, m_position, name);
            // Lowercase the name for matching.
            for (char& c : name)
                c = char(std::tolower(static_cast<unsigned char>(c)));
            // Constant or the graph variable.
            if (name == m_variable)
                return HNumber(std::to_string(m_varValue).c_str());
            if (name == "pi")
                return HMath::pi();
            if (name == "e")
                return HMath::exp(HNumber(1));
            // Function call.
            skipSpaces();
            if (consume('(')) {
                const NamedFunction* fn = findFunction(name);
                if (!fn)
                    throw ParseError{Error::Syntax};
                if (++m_depth > Calculator::kMaximumDepth)
                    throw ParseError{Error::Syntax};
                HNumber arg = parseExpression();
                if (!consume(')'))
                    throw ParseError{Error::UnmatchedParenthesis};
                --m_depth;
                if (fn->trig && m_unit == AngleUnit::Degrees)
                    arg = toRadians(arg);
                HNumber result = fn->apply(arg);
                if (fn->trig && m_unit == AngleUnit::Degrees
                    && (name == "asin" || name == "acos" || name == "atan"))
                    result = fromRadians(result);
                return result;
            }
            throw ParseError{Error::Syntax};
        }
        if (!std::isdigit(static_cast<unsigned char>(character)) && character != '.')
            throw ParseError{Error::Syntax};
        return parseNumber();
    }

    HNumber parseNumber()
    {
        const size_t start = m_position;
        bool digit = false;
        bool point = false;
        while (m_position < m_text.size()) {
            const char character = m_text[m_position];
            if (std::isdigit(static_cast<unsigned char>(character))) {
                digit = true;
                ++m_position;
            } else if (character == '.' && !point) {
                point = true;
                ++m_position;
            } else {
                break;
            }
        }
        if (!digit)
            throw ParseError{Error::Syntax};
        const std::string literal(m_text.substr(start, m_position - start));
        const HNumber number(literal.c_str());
        if (number.isNan())
            throw ParseError{Error::Syntax};
        return number;
    }

    std::string_view m_text;
    size_t m_position = 0;
    int m_depth = 0;
    AngleUnit m_unit;
    std::string m_variable;
    double m_varValue;
};

// The constant e is the base of the natural logarithm; HMath exposes it
// through exp(1).
HNumber eulerE()
{
    return HMath::exp(HNumber(1));
}

} // namespace

ScientificCalculator::ScientificCalculator(AngleUnit unit) : m_unit(unit) {}

Result ScientificCalculator::evaluate(const std::string& expression) const
{
    Result result;
    const std::string normalized = Calculator::normalize(expression);
    if (normalized.size() > Calculator::kMaximumLength) {
        result.error = Error::TooLong;
        return result;
    }
    if (normalized.empty()) {
        result.error = Error::Empty;
        return result;
    }
    const std::string fixed = Calculator::autoFix(normalized);
    try {
        ScientificParser parser(fixed, m_unit);
        // Replace the placeholder for "e" the constant with the real value
        // by patching the parser's primary handling: handled in parsePrimary
        // via a special case below.
        HNumber value = parser.parse();
        if (value.isNan()) {
            result.error = Error::Undefined;
            return result;
        }
        result.ok = true;
        result.value = value;
        result.text = Calculator::format(value);
    } catch (const ParseError& error) {
        result.error = error.error;
    }
    return result;
}

double ScientificCalculator::evaluateDouble(const std::string& expression,
                                            const std::string& variable, double value) const
{
    try {
        ScientificParser parser(expression, m_unit, variable, value);
        const HNumber result = parser.parse();
        if (result.isNan())
            return std::numeric_limits<double>::quiet_NaN();
        // HMath's formatFixed gives a decimal string we can parse back to double.
        char* text = HMath::formatFixed(result, 15);
        if (!text)
            return std::numeric_limits<double>::quiet_NaN();
        const double d = std::strtod(text, nullptr);
        std::free(text);
        return d;
    } catch (...) {
        return std::numeric_limits<double>::quiet_NaN();
    }
}

} // namespace calculator
