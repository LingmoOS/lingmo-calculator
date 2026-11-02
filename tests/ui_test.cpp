// SPDX-FileCopyrightText: 2026 LingmoOS Contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "calculatorwindow.h"

#include "ui/application.h"

#include <gtest/gtest.h>

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
