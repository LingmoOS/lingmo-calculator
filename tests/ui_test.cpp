// SPDX-FileCopyrightText: 2026 LingmoOS Contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "calculatorwindow.h"

#include "ui/application.h"
#include "ui/clipboard.h"

#include <gtest/gtest.h>

#include <string>

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    Lingmo::ui::Application app(argc, argv, Lingmo::ui::Application::Platform::Headless);
    return RUN_ALL_TESTS();
}

TEST(CalculatorWindow, EvaluatesAndKeepsHistory)
{
    calculator::CalculatorWindow window;
    window.append("1+2*3");
    window.evaluate();
    EXPECT_EQ(window.result(), "7");
    ASSERT_EQ(window.history().size(), 1u);
    EXPECT_EQ(window.history().front(), "1+2*3 = 7");

    window.append("+1");
    window.evaluate();
    EXPECT_EQ(window.result(), "8");
}

TEST(CalculatorWindow, ClearsAndBackspaces)
{
    calculator::CalculatorWindow window;
    window.append("123");
    window.backspace();
    EXPECT_EQ(window.expression(), "12");
    window.clear();
    EXPECT_TRUE(window.expression().empty());
    EXPECT_EQ(window.result(), "0");
}

TEST(CalculatorWindow, HandlesParenthesesAndPowers)
{
    calculator::CalculatorWindow window;
    window.append("(2+3)^2");
    window.evaluate();
    EXPECT_EQ(window.result(), "25");
}

TEST(CalculatorWindow, CopiesTheResultAndClearsHistory)
{
    calculator::CalculatorWindow window;
    window.append("6*7");
    window.evaluate();
    window.copyResult();
    std::string copied;
    Lingmo::ui::Clipboard::requestText([&copied](std::string text) { copied = std::move(text); });
    EXPECT_EQ(copied, "42");
    ASSERT_EQ(window.history().size(), 1u);
    window.clearHistory();
    EXPECT_TRUE(window.history().empty());
    EXPECT_EQ(window.result(), "42");
}
