// SPDX-FileCopyrightText: 2026 LingmoOS Contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "calculatorwindow.h"

#include "modes/date_page.h"
#include "modes/graph_page.h"
#include "modes/programmer_page.h"
#include "modes/scientific_page.h"
#include "modes/standard_page.h"

#include "base/i18n.h"
#include "base/paths.h"
#include "base/settings.h"
#include "ui/color_id.h"
#include "ui/keys.h"
#include "ui/label.h"
#include "ui/painter.h"
#include "ui/segmented_control.h"
#include "ui/theme.h"
#include "ui/view.h"
#include "ui/window.h"

#include <algorithm>
#include <string>
#include <vector>

using namespace Lingmo::ui;
using Lingmo::i18n::tr;

namespace calculator {

// Width/height for each mode (window interior, without chrome).
static constexpr double kModeWidths[]  = { 400, 600, 520, 460, 680 };
static constexpr double kModeHeights[] = { 664, 664, 624, 464, 584 };

// The top-level view that hosts the mode-selector and the pages.
class MultiModeRoot : public View
{
public:
    MultiModeRoot()
    {
        m_modeSwitch = addChild<SegmentedControl>(
            std::vector<std::string>{"Standard", "Scientific", "Programmer", "Date", "Graph"},
            SegmentedControl::Style::Segmented);
        m_modeSwitch->setName("modeSwitch");
        m_modeSwitch->setContentHeight(28);
        m_modeSwitch->setFontSize(12.5);
    }

    SegmentedControl* modeSwitch() const { return m_modeSwitch; }

protected:
    void paint(Painter& painter) override
    {
        const Theme& theme = Theme::current();
        painter.fillRect(localRect(), theme.background);
        painter.drawHorizontalLine(0, kHeaderHeight - 1, geometry().width,
                                   theme.text.withAlpha(0.06), painter.borderWidth(1));
    }

    void layout() override
    {
        const RectF rect = localRect();
        const double switchW = std::min(std::max(rect.width * 0.88, 300.0), rect.width - 24.0);
        m_modeSwitch->setGeometry({(rect.width - switchW) / 2.0, 8, switchW, 28});

        const RectF pageRect{0, kHeaderHeight, rect.width, std::max(0.0, rect.height - kHeaderHeight)};
        for (const auto& child : children()) {
            if (child.get() == m_modeSwitch) continue;
            child->setGeometry(pageRect);
        }
    }

private:
    static constexpr double kHeaderHeight = 44;
    SegmentedControl* m_modeSwitch = nullptr;
};

// ---- CalculatorWindow ----------------------------------------------------

CalculatorWindow::CalculatorWindow()
{
    m_window.setTitle(tr("Calculator"));
    m_window.setMinimumSize({340, 500});

    auto root = std::make_unique<MultiModeRoot>();
    m_root = root.get();

    m_standardPage   = m_root->addChild<StandardPage>();
    m_scientificPage = m_root->addChild<ScientificPage>();
    m_programmerPage = m_root->addChild<ProgrammerPage>();
    m_datePage       = m_root->addChild<DatePage>();
    m_graphPage      = m_root->addChild<GraphPage>();

    m_window.setContentView(std::move(root));

    // Wire mode selector.
    m_root->modeSwitch()->onCurrentChanged = [this](int index) {
        setMode(static_cast<Mode>(index));
    };

    // Restore saved mode.
    Lingmo::base::Settings settings(Lingmo::base::paths::lingmoConfig("calculator"));
    const int saved = int(settings.intValue("window/mode", 0));
    const Mode initial = (saved >= 0 && saved < int(Mode::Count))
                         ? static_cast<Mode>(saved) : Mode::Standard;
    applyMode(initial);
    m_root->modeSwitch()->setCurrentIndex(int(initial));
}

CalculatorWindow::~CalculatorWindow()
{
    // Persist the current mode.
    Lingmo::base::Settings settings(Lingmo::base::paths::lingmoConfig("calculator"));
    settings.setValue("window/mode", int64_t(m_mode));
    settings.sync();
}

void CalculatorWindow::applyMode(Mode mode)
{
    m_mode = mode;
    const int idx = int(mode);

    m_standardPage->setVisible(mode == Mode::Standard);
    m_scientificPage->setVisible(mode == Mode::Scientific);
    m_programmerPage->setVisible(mode == Mode::Programmer);
    m_datePage->setVisible(mode == Mode::Date);
    m_graphPage->setVisible(mode == Mode::Graph);

    m_window.resize({kModeWidths[idx], kModeHeights[idx]});
}

void CalculatorWindow::setMode(Mode mode)
{
    if (m_mode == mode) return;
    applyMode(mode);
}

// Standard-mode delegation (preserves test API).
void CalculatorWindow::append(const std::string& text)    { m_standardPage->append(text); }
void CalculatorWindow::backspace()                         { m_standardPage->backspace(); }
void CalculatorWindow::clear()                             { m_standardPage->clear(); }
void CalculatorWindow::clearHistory()                      { m_standardPage->clearHistory(); }
void CalculatorWindow::evaluate()                          { m_standardPage->evaluate(); }
void CalculatorWindow::copyResult()                        { m_standardPage->copyResult(); }
const std::string& CalculatorWindow::expression() const    { return m_standardPage->expression(); }
std::string CalculatorWindow::result() const
{
    const std::string& r = m_standardPage->result();
    return r.empty() ? std::string("0") : r;
}
const std::vector<std::string>& CalculatorWindow::history() const { return m_standardPage->history(); }
void CalculatorWindow::focus() { m_standardPage->focus(); }

} // namespace calculator
