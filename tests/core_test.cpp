// SPDX-FileCopyrightText: 2026 LingmoOS Contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The expression reader, against HMath's high precision numbers.

#include "core/calculator.h"

#include <gtest/gtest.h>

#include <string>

namespace {

std::string evaluated(const std::string& expression)
{
    const calculator::Calculator calculator;
    const calculator::Result result = calculator.evaluate(expression);
    EXPECT_TRUE(result.ok) << expression << " failed";
    return result.text;
}

calculator::Error errorOf(const std::string& expression)
{
    const calculator::Calculator calculator;
    const calculator::Result result = calculator.evaluate(expression);
    EXPECT_FALSE(result.ok) << expression << " unexpectedly evaluated to " << result.text;
    return result.error;
}

} // namespace

TEST(Calculator, AddsAndSubtracts)
{
    EXPECT_EQ(evaluated("1 + 2"), "3");
    EXPECT_EQ(evaluated("7-10"), "-3");
    EXPECT_EQ(evaluated("12 + 8 - 20"), "0");
}

TEST(Calculator, MultipliesAndDivides)
{
    EXPECT_EQ(evaluated("12*8"), "96");
    EXPECT_EQ(evaluated("144/12"), "12");
    EXPECT_EQ(evaluated("10/4"), "2.5");
    EXPECT_EQ(evaluated("7/2"), "3.5");
}

TEST(Calculator, FollowsOperatorPrecedence)
{
    EXPECT_EQ(evaluated("1+2*3"), "7");
    EXPECT_EQ(evaluated("(1+2)*3"), "9");
    EXPECT_EQ(evaluated("2-3*4+5"), "-5");
    EXPECT_EQ(evaluated("100/5/2"), "10");
    EXPECT_EQ(evaluated("100/(5/2)"), "40");
}

TEST(Calculator, PowersAreRightAssociativeAndBindBeforeSigns)
{
    EXPECT_EQ(evaluated("2^10"), "1024");
    EXPECT_EQ(evaluated("2^3^2"), "512");
    EXPECT_EQ(evaluated("-2^2"), "-4");
    EXPECT_EQ(evaluated("2^-3"), "0.125");
    EXPECT_EQ(evaluated("(1+1)^(3+2)"), "32");
}

TEST(Calculator, PercentMeansAHundredth)
{
    EXPECT_EQ(evaluated("50%"), "0.5");
    EXPECT_EQ(evaluated("200+10%"), "200.1");
    EXPECT_EQ(evaluated("25%*4"), "1");
    EXPECT_EQ(evaluated("-5%"), "-0.05");
}

TEST(Calculator, KeepsExactDecimals)
{
    EXPECT_EQ(evaluated("0.1+0.2"), "0.3");
    EXPECT_EQ(evaluated("1.1*3"), "3.3");
    EXPECT_EQ(evaluated("0.3-0.1"), "0.2");
    EXPECT_EQ(evaluated("1/3"), "0.3333333333");
    EXPECT_EQ(evaluated("2/3"), "0.6666666667");
}

TEST(Calculator, HandlesLargeAndSmallMagnitudes)
{
    EXPECT_EQ(evaluated("2^100"), "1.2676506002e30");
    EXPECT_EQ(evaluated("1/1000000000000"), "1e-12");
    EXPECT_EQ(evaluated("999999999999999999*999999999999999999"), "1e36");
}

TEST(Calculator, AcceptsPrettyOperatorsFromTheKeypad)
{
    EXPECT_EQ(evaluated("6×7"), "42");
    EXPECT_EQ(evaluated("8÷2"), "4");
    EXPECT_EQ(evaluated("5−3"), "2");
    EXPECT_EQ(evaluated("6 × 7"), "42");
}

TEST(Calculator, ClosesOpenParentheses)
{
    EXPECT_EQ(evaluated("2*(3+4"), "14");
    EXPECT_EQ(evaluated("(1+2"), "3");
    EXPECT_EQ(evaluated("-(2+3"), "-5");
}

TEST(Calculator, TrimsAndIgnoresATrailingEquals)
{
    EXPECT_EQ(evaluated("  4*5  "), "20");
    EXPECT_EQ(evaluated("4*5="), "20");
}

TEST(Calculator, RejectsDivisionByZero)
{
    EXPECT_EQ(errorOf("1/0"), calculator::Error::DivisionByZero);
    EXPECT_EQ(errorOf("2/(3-3)"), calculator::Error::DivisionByZero);
}

TEST(Calculator, RejectsEmptyAndMalformedInput)
{
    EXPECT_EQ(errorOf(""), calculator::Error::Empty);
    EXPECT_EQ(errorOf("   "), calculator::Error::Empty);
    EXPECT_EQ(errorOf("+"), calculator::Error::Syntax);
    EXPECT_EQ(errorOf("2++"), calculator::Error::Syntax);
    EXPECT_EQ(errorOf("2+*3"), calculator::Error::Syntax);
    EXPECT_EQ(errorOf("abc"), calculator::Error::Syntax);
    EXPECT_EQ(errorOf("."), calculator::Error::Syntax);
    EXPECT_EQ(errorOf("2 3"), calculator::Error::Syntax);
}

TEST(Calculator, RejectsUnmatchedClosings)
{
    EXPECT_EQ(errorOf(")"), calculator::Error::UnmatchedParenthesis);
    EXPECT_EQ(errorOf("2+3)"), calculator::Error::UnmatchedParenthesis);
}

TEST(Calculator, RejectsHugeExponents)
{
    EXPECT_EQ(errorOf("2^10000"), calculator::Error::ExponentTooLarge);
    EXPECT_EQ(errorOf("2^-10000"), calculator::Error::ExponentTooLarge);
}

TEST(Calculator, RejectsOverlongExpressions)
{
    std::string expression(1000, '1');
    EXPECT_EQ(errorOf(expression), calculator::Error::TooLong);
}

TEST(Calculator, FormatsWholeNumbersWithoutTrailingPoint)
{
    EXPECT_EQ(calculator::Calculator::format(HNumber(3)), "3");
    EXPECT_EQ(calculator::Calculator::format(HNumber("2.500")), "2.5");
    const HNumber negative("0");
    EXPECT_EQ(calculator::Calculator::format(HMath::negate(negative)), "0");
}

TEST(Calculator, NormalizesOperators)
{
    EXPECT_EQ(calculator::Calculator::normalize("6×7"), "6*7");
    EXPECT_EQ(calculator::Calculator::normalize("8÷2"), "8/2");
    EXPECT_EQ(calculator::Calculator::normalize("5−3"), "5-3");
    EXPECT_EQ(calculator::Calculator::normalize(" 1 + 1 "), "1 + 1");
}

TEST(Calculator, AutoFixIsIdempotentOnBalancedExpressions)
{
    EXPECT_EQ(calculator::Calculator::autoFix("(1+2)*3"), "(1+2)*3");
    EXPECT_EQ(calculator::Calculator::autoFix("2*(3+4"), "2*(3+4)");
    EXPECT_EQ(calculator::Calculator::autoFix("4+"), "4+");
}
