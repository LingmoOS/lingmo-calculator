// SPDX-FileCopyrightText: 2026 LingmoOS Contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "ui/view.h"
#include "ui/label.h"
#include "base/timer.h"

#include <functional>
#include <string>
#include <vector>

namespace calculator {

// The standard four-function keypad page. Owns its own expression state.
class StandardPage : public Lingmo::ui::View
{
public:
    StandardPage();

    void append(const std::string& text);
    void backspace();
    void clear();
    void clearHistory();
    void evaluate();
    void copyResult();

    const std::string& expression() const { return m_expression; }
    const std::string& result() const { return m_result; }
    const std::vector<std::string>& history() const { return m_history; }

    void focus();

    Lingmo::ui::SizeF sizeHint() const override { return {380, 560}; }
    bool acceptsFocus() const override { return true; }

protected:
    void paint(Lingmo::ui::Painter& painter) override;
    void layout() override;
    bool keyPress(const Lingmo::ui::KeyEvent& event) override;
    void pointerPress(const Lingmo::ui::PointerEvent& event) override;

private:
    class CalcButton;
    void addKeys();
    void addUtilityKeys();
    void toggleSign();
    void refresh();
    Lingmo::ui::RectF displayRect() const;

    Lingmo::ui::Label* m_historyLines[3] = {};
    Lingmo::ui::Label* m_expressionLabel = nullptr;
    Lingmo::ui::Label* m_resultLabel = nullptr;
    CalcButton* m_copyButton = nullptr;
    std::vector<CalcButton*> m_buttons;
    std::vector<CalcButton*> m_utilityButtons;
    std::string m_expression;
    std::string m_result;
    std::string m_lastExpression;
    std::vector<std::string> m_history;
    Lingmo::base::Timer m_copiedTimer;
    bool m_justEvaluated = false;
};

} // namespace calculator
