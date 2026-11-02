// SPDX-FileCopyrightText: 2026 LingmoOS Contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/programmer_calc.h"

#include "ui/view.h"

#include <string>
#include <vector>

namespace Lingmo::ui {
class ComboBox;
class Label;
class SegmentedControl;
}

namespace calculator {

// The programmer page: integer arithmetic in HEX/DEC/OCT/BIN with bitwise ops.
class ProgrammerPage : public Lingmo::ui::View
{
public:
    ProgrammerPage();

    Base base() const { return m_calc.base(); }
    void setBase(Base base);

    Lingmo::ui::SizeF sizeHint() const override { return {500, 520}; }

protected:
    void paint(Lingmo::ui::Painter& painter) override;
    void layout() override;
    bool keyPress(const Lingmo::ui::KeyEvent& event) override;
    void pointerPress(const Lingmo::ui::PointerEvent& event) override;

private:
    class CalcButton;

    void addDigitKeys();
    void addOperatorKeys();
    void addBitOpKeys();
    void append(const std::string& text);
    void backspace();
    void clear();
    void evaluate();
    void refresh();
    void updateBaseLabels();
    void updateKeyAvailability();

    Lingmo::ui::SegmentedControl* m_baseControl = nullptr;
    Lingmo::ui::ComboBox* m_widthCombo = nullptr;
    Lingmo::ui::Label* m_hexLabel = nullptr;
    Lingmo::ui::Label* m_decLabel = nullptr;
    Lingmo::ui::Label* m_octLabel = nullptr;
    Lingmo::ui::Label* m_binLabel = nullptr;
    Lingmo::ui::Label* m_hexCaption = nullptr;
    Lingmo::ui::Label* m_decCaption = nullptr;
    Lingmo::ui::Label* m_octCaption = nullptr;
    Lingmo::ui::Label* m_binCaption = nullptr;
    Lingmo::ui::Label* m_expressionLabel = nullptr;
    Lingmo::ui::Label* m_resultLabel = nullptr;

    std::vector<CalcButton*> m_digitButtons;   // 0-9, A-F
    std::vector<CalcButton*> m_operatorButtons;
    std::vector<CalcButton*> m_bitOpButtons;

    std::string m_expression;
    std::string m_result;
    int64_t m_currentValue = 0;

    ProgrammerCalculator m_calc;
};

} // namespace calculator
