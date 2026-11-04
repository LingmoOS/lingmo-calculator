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
#include "ui/header_button.h"
#include "ui/label.h"
#include "ui/layout.h"
#include "ui/menu.h"
#include "ui/painter.h"
#include "ui/theme.h"
#include "ui/view.h"
#include "ui/window.h"
#include "ui/window_header.h"

#include <string>
#include <vector>

using namespace Lingmo::ui;
using Lingmo::i18n::tr;

namespace calculator {

namespace {

constexpr double kHeaderHeight = 44;

// Mode widths without sidebar (compact).
static constexpr double kModeWidths[]  = { 340, 520, 480, 420, 600 };
static constexpr double kModeHeights[] = { 664, 664, 624, 464, 584 };

// Simple root: WindowHeader at top, pages fill the rest.
class CalcRoot : public View
{
public:
    CalcRoot()
        : m_header(addChild<WindowHeader>(kHeaderHeight))
        , m_content(addChild<View>())
    {
        setName("calculatorRoot");
        m_header->setName("header");
        m_header->setButtonsCentered(true);
        m_header->setButtonMargin(4);
        m_content->setName("content");
    }

    WindowHeader* header() const { return m_header; }
    View* content() const { return m_content; }

protected:
    void paint(Painter& painter) override
    {
        painter.fillRect(localRect(), Theme::current().background);
    }

    void layout() override
    {
        const RectF rect = localRect();
        m_header->setGeometry({0, 0, rect.width, kHeaderHeight});
        m_content->setGeometry({0, kHeaderHeight, rect.width,
                                std::max(0.0, rect.height - kHeaderHeight)});
        // Pages inside the content area fill it completely.
        const RectF page = {0, 0, m_content->geometry().width, m_content->geometry().height};
        for (const auto& child : m_content->children())
            child->setGeometry(page);
    }

private:
    WindowHeader* m_header;
    View* m_content;
};

} // namespace

// ---- CalculatorWindow -------------------------------------------------------

const char* CalculatorWindow::modeName(Mode mode)
{
    switch (mode) {
    case Mode::Standard:   return "Standard";
    case Mode::Scientific: return "Scientific";
    case Mode::Programmer: return "Programmer";
    case Mode::Date:       return "Date Calculator";
    case Mode::Graph:      return "Graphing";
    default:               return "Standard";
    }
}

CalculatorWindow::CalculatorWindow()
{
    m_window.setTitle(tr("Calculator"));
    m_window.setMinimumSize({300, 400});

    auto root = std::make_unique<CalcRoot>();
    m_root = root.get();

    // Build header content: mode label on left, menu button on right.
    auto* row = m_root->header()->setContent<RowLayout>();
    row->setSpacing(4);
    row->setLayoutHints({.fillWidth = true});

    m_modeLabel = row->addChild<Label>(tr(modeName(Mode::Standard)));
    m_modeLabel->setFontSize(14.5);
    m_modeLabel->setWeight(600);
    m_modeLabel->setColor(ColorRecipe(ColorId::Text));
    m_modeLabel->setLayoutHints({.fillWidth = true});

    m_optionsButton = row->addChild<HeaderButton>("menu", 32);
    m_optionsButton->onClicked = [this] { openModeMenu(); };

    // Add pages.
    View* content = m_root->content();
    m_standardPage   = content->addChild<StandardPage>();
    m_scientificPage = content->addChild<ScientificPage>();
    m_programmerPage = content->addChild<ProgrammerPage>();
    m_datePage       = content->addChild<DatePage>();
    m_graphPage      = content->addChild<GraphPage>();

    m_window.setContentView(std::move(root));

    // Restore saved mode.
    Lingmo::base::Settings settings(Lingmo::base::paths::lingmoConfig("calculator"));
    const int saved = int(settings.intValue("window/mode", 0));
    const Mode initial = (saved >= 0 && saved < int(Mode::Count))
                         ? static_cast<Mode>(saved) : Mode::Standard;
    applyMode(initial);
}

CalculatorWindow::~CalculatorWindow()
{
    Lingmo::base::Settings settings(Lingmo::base::paths::lingmoConfig("calculator"));
    settings.setValue("window/mode", int64_t(m_mode));
    settings.sync();
}

void CalculatorWindow::openModeMenu()
{
    m_optionsButton->setChecked(true);
    m_menu = std::make_unique<Menu>();

    struct ModeEntry { Mode mode; const char* label; };
    const ModeEntry entries[] = {
        { Mode::Standard,   "Standard"       },
        { Mode::Scientific, "Scientific"     },
        { Mode::Programmer, "Programmer"     },
        { Mode::Date,       "Date Calculator"},
        { Mode::Graph,      "Graphing"       },
    };

    for (const auto& e : entries) {
        const size_t idx = m_menu->addItem(tr(e.label), [this, mode = e.mode] {
            setMode(mode);
        });
        m_menu->setItemChecked(idx, m_mode == e.mode);
    }

    m_menu->onClosed = [this] { m_optionsButton->setChecked(false); };
    m_menu->popupFor(m_optionsButton, 4);
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

    if (m_modeLabel)
        m_modeLabel->setText(tr(modeName(mode)));

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
