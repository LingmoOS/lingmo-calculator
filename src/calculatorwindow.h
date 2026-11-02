// SPDX-FileCopyrightText: 2026 LingmoOS Contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "ui/window.h"

#include <functional>
#include <string>
#include <vector>

namespace Lingmo::ui {
class Label;
class View;
}

namespace calculator {

class CalculatorRoot;

// The calculator window and the state the tests can drive.
class CalculatorWindow
{
public:
    CalculatorWindow();
    ~CalculatorWindow();

    CalculatorWindow(const CalculatorWindow &) = delete;
    CalculatorWindow &operator=(const CalculatorWindow &) = delete;

    Lingmo::ui::Window &window() { return m_window; }

    void append(const std::string &text);
    void backspace();
    void clear();
    void evaluate();

    const std::string &expression() const;
    std::string result() const;
    const std::vector<std::string> &history() const;

    void focus();

private:
    Lingmo::ui::Window m_window;
    CalculatorRoot *m_root = nullptr;
};

} // namespace calculator
