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
#include "ui/icon.h"
#include "ui/keys.h"
#include "ui/label.h"
#include "ui/painter.h"
#include "ui/theme.h"
#include "ui/view.h"
#include "ui/window.h"
#include "ui/window_header.h"

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

using namespace Lingmo::ui;
using Lingmo::i18n::tr;

namespace calculator {

namespace {

constexpr double kHeaderHeight = 44;
constexpr double kSidebarWidth = 200;
constexpr double kNavItemHeight = 48;

} // namespace

// Width/height for each mode (window interior, without chrome). Sidebar width added.
static constexpr double kModeWidths[]  = { 600, 800, 720, 660, 880 };
static constexpr double kModeHeights[] = { 664, 664, 624, 464, 584 };

// A single navigation item in the sidebar.
class NavigationItem : public View
{
public:
    NavigationItem(const std::string& iconName, const std::string& text)
        : m_icon(addChild<ThemeIcon>(iconName, 20))
        , m_text(addChild<Label>(text))
    {
        setName("navigationItem");
        m_text->setFontSize(13);
        m_text->setWeight(500);
    }

    void setSelected(bool selected)
    {
        if (m_selected == selected) return;
        m_selected = selected;
        requestPaint();
    }

    bool selected() const { return m_selected; }

    std::function<void()> onClicked;

protected:
    void paint(Painter& painter) override
    {
        const Theme& theme = Theme::current();
        const RectF rect = localRect();

        if (m_selected) {
            painter.fillRect(rect, theme.alternateBackground);
            painter.fillRect({0, 0, 3, rect.height}, theme.highlight);
        } else if (m_hovered) {
            painter.fillRect(rect, theme.alternateBackground.withAlpha(0.5));
        }
    }

    void layout() override
    {
        const RectF rect = localRect();
        const double iconSize = 20;
        const double leftPadding = 16;
        const double iconTextGap = 12;

        m_icon->setGeometry({leftPadding, (rect.height - iconSize) / 2, iconSize, iconSize});

        const double textX = leftPadding + iconSize + iconTextGap;
        const SizeF textSize = m_text->sizeHint();
        m_text->setGeometry({textX, (rect.height - textSize.height) / 2,
                             std::max(0.0, rect.width - textX - 16), textSize.height});
    }

    void pointerPress(const PointerEvent& event) override
    {
        if (event.button == MouseButton::Left) {
            m_pressed = true;
        }
    }

    void pointerRelease(const PointerEvent& event) override
    {
        if (m_pressed && event.button == MouseButton::Left) {
            m_pressed = false;
            if (localRect().contains(event.position) && onClicked) {
                onClicked();
            }
        }
    }

    void pointerEnter() override
    {
        m_hovered = true;
        requestPaint();
        setCursorShape(CursorShape::Pointer);
    }

    void pointerLeave() override
    {
        m_hovered = false;
        m_pressed = false;
        requestPaint();
        setCursorShape(CursorShape::Default);
    }

private:
    ThemeIcon* m_icon = nullptr;
    Label* m_text = nullptr;
    bool m_selected = false;
    bool m_hovered = false;
    bool m_pressed = false;
};

// The sidebar containing all navigation items.
class Sidebar : public View
{
public:
    Sidebar()
    {
        setName("sidebar");
    }

    NavigationItem* addItem(const std::string& iconName, const std::string& text)
    {
        auto item = addChild<NavigationItem>(iconName, text);
        m_items.push_back(item);
        return item;
    }

    void setCurrentIndex(int index)
    {
        if (index < 0 || index >= int(m_items.size())) return;
        if (m_currentIndex == index) return;

        if (m_currentIndex >= 0 && m_currentIndex < int(m_items.size())) {
            m_items[m_currentIndex]->setSelected(false);
        }

        m_currentIndex = index;
        m_items[index]->setSelected(true);
    }

    int currentIndex() const { return m_currentIndex; }

    std::function<void(int)> onItemSelected;

protected:
    void paint(Painter& painter) override
    {
        const Theme& theme = Theme::current();
        const RectF rect = localRect();
        painter.fillRect(rect, theme.secondBackground);
        painter.drawVerticalLine(rect.width - 1, 0, rect.height,
                                 theme.text.withAlpha(0.06), painter.borderWidth(1));
    }

    void layout() override
    {
        const RectF rect = localRect();
        double y = 12;

        for (auto item : m_items) {
            item->setGeometry({0, y, rect.width, kNavItemHeight});
            y += kNavItemHeight;
        }
    }

private:
    std::vector<NavigationItem*> m_items;
    int m_currentIndex = -1;
};

// The top-level view with sidebar and content area.
class SidebarRoot : public View
{
public:
    SidebarRoot()
        : m_sidebar(addChild<Sidebar>())
        , m_contentArea(addChild<View>())
        , m_header(m_contentArea->addChild<WindowHeader>(kHeaderHeight))
    {
        setName("calculatorRoot");
        m_sidebar->setName("sidebar");
        m_contentArea->setName("contentArea");
        m_header->setName("header");

        auto title = m_header->setContent<Label>(tr("Calculator"));
        title->setFontSize(14.5);
        title->setWeight(600);
        title->setColor(ColorRecipe(ColorId::Text));

        m_header->setButtonMargin(4);
        m_header->setButtonsCentered(true);
    }

    Sidebar* sidebar() const { return m_sidebar; }
    View* contentArea() const { return m_contentArea; }

protected:
    void paint(Painter& painter) override
    {
        const Theme& theme = Theme::current();
        painter.fillRect(localRect(), theme.background);
    }

    void layout() override
    {
        const RectF rect = localRect();

        m_sidebar->setGeometry({0, 0, kSidebarWidth, rect.height});

        const RectF contentRect{kSidebarWidth, 0, std::max(0.0, rect.width - kSidebarWidth), rect.height};
        m_contentArea->setGeometry(contentRect);

        m_header->setGeometry({0, 0, contentRect.width, kHeaderHeight});

        const RectF pageRect{0, kHeaderHeight, contentRect.width, std::max(0.0, contentRect.height - kHeaderHeight)};
        for (const auto& child : m_contentArea->children()) {
            if (child.get() == m_header) continue;
            child->setGeometry(pageRect);
        }
    }

private:
    Sidebar* m_sidebar = nullptr;
    View* m_contentArea = nullptr;
    WindowHeader* m_header = nullptr;
};

// ---- CalculatorWindow ----------------------------------------------------

CalculatorWindow::CalculatorWindow()
{
    m_window.setTitle(tr("Calculator"));
    m_window.setMinimumSize({540, 500});

    auto root = std::make_unique<SidebarRoot>();
    m_root = root.get();

    // Add navigation items with system icons.
    Sidebar* sidebar = m_root->sidebar();
    sidebar->addItem("accessories-calculator", tr("Standard"));
    sidebar->addItem("x-office-spreadsheet", tr("Scientific"));
    sidebar->addItem("application-x-executable", tr("Programmer"));
    sidebar->addItem("x-office-calendar", tr("Date Calculator"));
    sidebar->addItem("office-chart", tr("Graphing"));

    // Add pages to content area.
    View* contentArea = m_root->contentArea();
    m_standardPage   = contentArea->addChild<StandardPage>();
    m_scientificPage = contentArea->addChild<ScientificPage>();
    m_programmerPage = contentArea->addChild<ProgrammerPage>();
    m_datePage       = contentArea->addChild<DatePage>();
    m_graphPage      = contentArea->addChild<GraphPage>();

    m_window.setContentView(std::move(root));

    // Wire sidebar selection.
    sidebar->onItemSelected = [this](int index) {
        setMode(static_cast<Mode>(index));
    };

    // Wire navigation items click.
    for (int i = 0; i < 5; ++i) {
        auto* item = static_cast<NavigationItem*>(sidebar->children()[i].get());
        const int idx = i;
        item->onClicked = [this, idx]() {
            m_root->sidebar()->setCurrentIndex(idx);
            if (m_root->sidebar()->onItemSelected)
                m_root->sidebar()->onItemSelected(idx);
        };
    }

    // Restore saved mode.
    Lingmo::base::Settings settings(Lingmo::base::paths::lingmoConfig("calculator"));
    const int saved = int(settings.intValue("window/mode", 0));
    const Mode initial = (saved >= 0 && saved < int(Mode::Count))
                         ? static_cast<Mode>(saved) : Mode::Standard;
    applyMode(initial);
    m_root->sidebar()->setCurrentIndex(int(initial));
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
