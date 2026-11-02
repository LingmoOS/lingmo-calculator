// SPDX-FileCopyrightText: 2026 LingmoOS Contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/date_calc.h"

#include "ui/view.h"

#include <string>

namespace Lingmo::ui {
class Button;
class Label;
class SegmentedControl;
class TextField;
}

namespace calculator {

// The date page: two sub-modes — date difference and date arithmetic.
class DatePage : public Lingmo::ui::View
{
public:
    DatePage();

    Lingmo::ui::SizeF sizeHint() const override { return {440, 380}; }

protected:
    void paint(Lingmo::ui::Painter& painter) override;
    void layout() override;

private:
    void calculateDifference();
    void calculateAddDays();
    void showResult(const std::string& text);
    void switchSubMode(int index);

    Lingmo::ui::SegmentedControl* m_modeSwitch = nullptr;
    Lingmo::ui::Label* m_fromLabel = nullptr;
    Lingmo::ui::TextField* m_fromDate = nullptr;
    Lingmo::ui::Label* m_toLabel = nullptr;
    Lingmo::ui::TextField* m_toDate = nullptr;
    Lingmo::ui::Label* m_daysLabel = nullptr;
    Lingmo::ui::TextField* m_daysInput = nullptr;
    Lingmo::ui::Button* m_calcButton = nullptr;
    Lingmo::ui::Label* m_resultLabel = nullptr;

    int m_subMode = 0; // 0 = difference, 1 = add days
};

} // namespace calculator
