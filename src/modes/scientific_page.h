// SPDX-FileCopyrightText: 2026 LingmoOS Contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "base/timer.h"
#include "core/calculator.h"

#include "ui/view.h"

#include <string>
#include <vector>

namespace Lingmo::ui {
class Label;
class SegmentedControl;
}

namespace calculator {

// The scientific page: the standard keys plus a function grid (sin, cos, log,
// sqrt …) and an angle-unit toggle. Uses ScientificCalculator.
class ScientificPage : public Lingmo::ui::View
{
public:
    ScientificPage();

    void append(const std::string& text);
    void backspace();
    void clear();
    void evaluate();
    void copyResult();

    const std::string& expression() const { return m_expression; }
    const std::string& result() const { return m_result; }
    const std::vector<std::string>& history() const { return m_history; }

    void focus();

    Lingmo::ui::SizeF sizeHint() const override { return {560, 560}; }
    bool acceptsFocus() const override { return true; }

protected:
    void paint(Lingmo::ui::Painter& painter) override;
    void layout() override;
    bool keyPress(const Lingmo::ui::KeyEvent& event) override;
    void pointerPress(const Lingmo::ui::PointerEvent& event) override;

private:
    class CalcButton;

    void addFunctionKeys();
    void addStandardKeys();
    void addUtilityKeys();
    void toggleSign();
    void refresh();
    Lingmo::ui::RectF displayRect() const;

    Lingmo::ui::Label* m_historyLines[3] = {};
    Lingmo::ui::Label* m_expressionLabel = nullptr;
    Lingmo::ui::Label* m_resultLabel = nullptr;
    Lingmo::ui::SegmentedControl* m_angleToggle = nullptr;
    CalcButton* m_copyButton = nullptr;
    std::vector<CalcButton*> m_funcButtons;
    std::vector<CalcButton*> m_buttons;
    std::vector<CalcButton*> m_utilityButtons;
    std::string m_expression;
    std::string m_result;
    std::string m_lastExpression;
    std::vector<std::string> m_history;
    Lingmo::base::Timer m_copiedTimer;
    AngleUnit m_angleUnit = AngleUnit::Radians;
    bool m_justEvaluated = false;
};

} // namespace calculator
