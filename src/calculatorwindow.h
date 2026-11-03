// SPDX-FileCopyrightText: 2026 LingmoOS Contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "ui/window.h"

#include <string>
#include <vector>

namespace Lingmo::ui {
class View;
}

namespace calculator {

class DatePage;
class GraphPage;
class SidebarRoot;
class ProgrammerPage;
class ScientificPage;
class StandardPage;

// The calculator window. The standard page's API is preserved unchanged for
// tests; the window also supports switching between five modes.
class CalculatorWindow
{
public:
    enum class Mode { Standard = 0, Scientific, Programmer, Date, Graph, Count };

    CalculatorWindow();
    ~CalculatorWindow();

    CalculatorWindow(const CalculatorWindow&) = delete;
    CalculatorWindow& operator=(const CalculatorWindow&) = delete;

    Lingmo::ui::Window& window() { return m_window; }

    // Standard mode public API — used by tests.
    void append(const std::string& text);
    void backspace();
    void clear();
    void clearHistory();
    void evaluate();
    void copyResult();

    const std::string& expression() const;
    std::string result() const;
    const std::vector<std::string>& history() const;

    void focus();

    // Multi-mode API.
    void setMode(Mode mode);
    Mode currentMode() const { return m_mode; }

private:
    void applyMode(Mode mode);

    Lingmo::ui::Window m_window;
    SidebarRoot* m_root = nullptr;
    StandardPage* m_standardPage = nullptr;
    ScientificPage* m_scientificPage = nullptr;
    ProgrammerPage* m_programmerPage = nullptr;
    DatePage* m_datePage = nullptr;
    GraphPage* m_graphPage = nullptr;
    Mode m_mode = Mode::Standard;
};

} // namespace calculator
