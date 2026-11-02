// SPDX-FileCopyrightText: 2026 LingmoOS Contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "modes/date_page.h"

#include "base/i18n.h"
#include "ui/button.h"
#include "ui/color_id.h"
#include "ui/label.h"
#include "ui/painter.h"
#include "ui/segmented_control.h"
#include "ui/text_field.h"
#include "ui/theme.h"
#include "ui/view.h"

#include <cstdio>
#include <string>

using namespace Lingmo::ui;
using Lingmo::i18n::tr;

namespace calculator {

DatePage::DatePage()
{
    m_modeSwitch = addChild<SegmentedControl>(
        std::vector<std::string>{tr("Difference"), tr("Add Days")},
        SegmentedControl::Style::Segmented);
    m_modeSwitch->setContentHeight(30);
    m_modeSwitch->onCurrentChanged = [this](int index) { switchSubMode(index); };

    m_fromLabel = addChild<Label>(tr("From"));
    m_fromLabel->setFontSize(13);
    m_fromLabel->setColor(ColorRecipe(ColorId::DisabledText));

    m_fromDate = addChild<TextField>();
    m_fromDate->setPlaceholder("YYYY-MM-DD");

    m_toLabel = addChild<Label>(tr("To"));
    m_toLabel->setFontSize(13);
    m_toLabel->setColor(ColorRecipe(ColorId::DisabledText));

    m_toDate = addChild<TextField>();
    m_toDate->setPlaceholder("YYYY-MM-DD");

    m_daysLabel = addChild<Label>(tr("Days"));
    m_daysLabel->setFontSize(13);
    m_daysLabel->setColor(ColorRecipe(ColorId::DisabledText));

    m_daysInput = addChild<TextField>();
    m_daysInput->setPlaceholder(tr("+/- days"));

    m_calcButton = addChild<Button>(tr("Calculate"));
    m_calcButton->onClicked = [this] {
        if (m_subMode == 0) calculateDifference();
        else calculateAddDays();
    };

    m_resultLabel = addChild<Label>();
    m_resultLabel->setFontSize(16);
    m_resultLabel->setWeight(500);
    m_resultLabel->setTextAlignment(Label::TextAlignment::Center);
    m_resultLabel->setVerticallyCentered(true);

    // Pre-fill with today.
    const std::string today = DateCalculator::formatDate(DateCalculator::today());
    m_fromDate->setText(today);
    m_toDate->setText(today);

    switchSubMode(0);
}

void DatePage::switchSubMode(int index)
{
    m_subMode = index;
    m_modeSwitch->setCurrentIndex(index);

    const bool diff = (index == 0);
    m_toLabel->setVisible(diff);
    m_toDate->setVisible(diff);
    m_daysLabel->setVisible(!diff);
    m_daysInput->setVisible(!diff);

    m_resultLabel->setText({});
    requestLayout();
}

void DatePage::calculateDifference()
{
    CalendarDate from, to;
    if (!DateCalculator::parseDate(m_fromDate->text(), from)) {
        showResult(tr("Invalid start date"));
        return;
    }
    if (!DateCalculator::parseDate(m_toDate->text(), to)) {
        showResult(tr("Invalid end date"));
        return;
    }
    const DateCalculator calc;
    const DateDiffResult r = calc.difference(from, to);
    if (!r.ok) { showResult(r.error); return; }

    char buf[128];
    std::snprintf(buf, sizeof(buf), "%d %s  (%d %s %d %s %d %s)",
        r.totalDays, r.totalDays == 1 ? "day" : "days",
        r.years,  r.years  == 1 ? "year"  : "years",
        r.months, r.months == 1 ? "month" : "months",
        r.days,   r.days   == 1 ? "day"   : "days");
    showResult(buf);
}

void DatePage::calculateAddDays()
{
    CalendarDate date;
    if (!DateCalculator::parseDate(m_fromDate->text(), date)) {
        showResult(tr("Invalid date"));
        return;
    }
    const std::string daysText = m_daysInput->text();
    if (daysText.empty()) { showResult(tr("Enter a number of days")); return; }

    char* end = nullptr;
    const long days = std::strtol(daysText.c_str(), &end, 10);
    if (!end || *end != '\0') { showResult(tr("Invalid number of days")); return; }

    const DateCalculator calc;
    const DateAddResult r = calc.addDays(date, int(days));
    if (!r.ok) { showResult(r.error); return; }
    showResult(r.formatted);
}

void DatePage::showResult(const std::string& text)
{
    m_resultLabel->setText(text);
    requestLayout();
    requestPaint();
}

void DatePage::paint(Painter& painter)
{
    painter.fillRect(localRect(), Theme::current().background);
}

void DatePage::layout()
{
    const RectF rect = localRect();
    const double margin = 24;
    const double w = rect.width - 2 * margin;
    const double fieldH = 36;
    const double labelH = 18;
    const double gap = 8;

    m_modeSwitch->setGeometry({margin, margin, w, 30});

    double y = margin + 30 + 16;

    m_fromLabel->setGeometry({margin, y, w, labelH});
    y += labelH + 4;
    m_fromDate->setGeometry({margin, y, w, fieldH});
    y += fieldH + gap;

    if (m_subMode == 0) {
        m_toLabel->setGeometry({margin, y, w, labelH});
        y += labelH + 4;
        m_toDate->setGeometry({margin, y, w, fieldH});
        y += fieldH + gap;
    } else {
        m_daysLabel->setGeometry({margin, y, w, labelH});
        y += labelH + 4;
        m_daysInput->setGeometry({margin, y, w, fieldH});
        y += fieldH + gap;
    }

    m_calcButton->setGeometry({margin, y, w, 38});
    y += 38 + 16;
    m_resultLabel->setGeometry({margin, y, w, 40});
}

} // namespace calculator
