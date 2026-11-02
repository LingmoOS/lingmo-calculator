// SPDX-FileCopyrightText: 2026 LingmoOS Contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "modes/scientific_page.h"

#include "core/calculator.h"

#include "base/i18n.h"
#include "base/timer.h"
#include "ui/clipboard.h"
#include "ui/color_id.h"
#include "ui/keys.h"
#include "ui/label.h"
#include "ui/painter.h"
#include "ui/segmented_control.h"
#include "ui/theme.h"
#include "ui/view.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <string>
#include <vector>

using namespace Lingmo::ui;
using Lingmo::i18n::tr;

namespace calculator {

namespace {
constexpr int kMaximumHistory = 40;
}

// CalcButton moved out of anonymous namespace so addChild<CalcButton> can instantiate it
class ScientificPage::CalcButton : public View
{
public:
    CalcButton(std::string text, bool accent = false, double pointSize = 16)
        : m_text(std::move(text)), m_accent(accent)
    {
        m_label = addChild<Label>(m_text);
        m_label->setTextAlignment(Label::TextAlignment::Center);
        m_label->setVerticallyCentered(true);
        m_label->setFontSize(pointSize);
        m_label->setWeight(500);
        setTracksHover(true);
        setCursorShape(CursorShape::Pointer);
    }

    std::function<void()> onClicked;

    void setText(std::string text)
    {
        if (m_text == text) return;
        m_text = std::move(text);
        m_label->setText(m_text);
        requestLayout();
        requestPaint();
    }

    bool acceptsPointer() const override { return true; }

protected:
    void layout() override
    {
        const Theme& theme = Theme::current();
        m_label->setGeometry(localRect());
        m_label->setColor(m_accent ? theme.highlightedText : theme.text);
    }

    void paint(Painter& painter) override
    {
        const Theme& theme = Theme::current();
        Color fill = m_accent ? theme.highlight : theme.secondBackground;
        if (m_pressed)
            fill = m_accent ? theme.highlight.darker(1.12) : theme.disabledText.withAlpha(0.16);
        else if (isHovered())
            fill = m_accent ? theme.highlight.lighter(1.08) : theme.disabledText.withAlpha(0.09);
        painter.fillRoundedRect(localRect(), theme.mediumRadius, fill);
    }

    void pointerPress(const PointerEvent& event) override
    {
        if (event.button != MouseButton::Left) return;
        m_pressed = true;
        requestPaint();
    }

    void pointerRelease(const PointerEvent& event) override
    {
        if (!m_pressed) return;
        m_pressed = false;
        requestPaint();
        if (localRect().contains(event.position) && onClicked)
            onClicked();
    }

    void hoverChanged() override { requestPaint(); }

private:
    std::string m_text;
    bool m_accent;
    bool m_pressed = false;
    Label* m_label = nullptr;
};

ScientificPage::ScientificPage()
{
    for (Label*& line : m_historyLines) {
        line = addChild<Label>();
        line->setTextAlignment(Label::TextAlignment::Right);
        line->setFontSize(11);
        line->setColor(ColorRecipe(ColorId::DisabledText));
        line->setElide(Label::Elide::Middle);
    }
    m_expressionLabel = addChild<Label>();
    m_expressionLabel->setTextAlignment(Label::TextAlignment::Right);
    m_expressionLabel->setFontSize(19);
    m_expressionLabel->setWeight(500);
    m_expressionLabel->setElide(Label::Elide::Middle);

    m_resultLabel = addChild<Label>("0");
    m_resultLabel->setTextAlignment(Label::TextAlignment::Right);
    m_resultLabel->setFontSize(42);
    m_resultLabel->setWeight(600);
    m_resultLabel->setElide(Label::Elide::Middle);

    m_angleToggle = addChild<SegmentedControl>(
        std::vector<std::string>{tr("RAD"), tr("DEG")}, SegmentedControl::Style::Segmented);
    m_angleToggle->setName("angleToggle");
    m_angleToggle->setContentHeight(26);
    m_angleToggle->setFontSize(12);
    m_angleToggle->onCurrentChanged = [this](int index) {
        m_angleUnit = (index == 1) ? AngleUnit::Degrees : AngleUnit::Radians;
    };

    m_copiedTimer.setInterval(std::chrono::milliseconds(1400));
    m_copiedTimer.onTimeout = [this] {
        if (m_copyButton) m_copyButton->setText(tr("Copy"));
    };

    addFunctionKeys();
    addStandardKeys();
    addUtilityKeys();
    refresh();
}

void ScientificPage::append(const std::string& text)
{
    if (text.empty()) return;
    if (m_justEvaluated) {
        if (std::isdigit(static_cast<unsigned char>(text.front())) || text == "." || text == "(")
            m_expression.clear();
        m_result.clear();
        m_lastExpression.clear();
        m_justEvaluated = false;
    }
    if (m_expression.size() + text.size() > Calculator::kMaximumLength) return;
    m_expression += text;
    refresh();
}

void ScientificPage::backspace()
{
    if (m_expression.empty()) return;
    m_expression.pop_back();
    m_justEvaluated = false;
    refresh();
}

void ScientificPage::clear()
{
    m_expression.clear();
    m_result.clear();
    m_lastExpression.clear();
    m_justEvaluated = false;
    refresh();
}

void ScientificPage::evaluate()
{
    if (m_expression.empty()) return;
    const ScientificCalculator engine(m_angleUnit);
    const Result value = engine.evaluate(Calculator::autoFix(m_expression));
    if (!value.ok) { m_result = tr("Error"); refresh(); return; }
    const std::string expression = Calculator::normalize(m_expression);
    m_history.push_back(expression + " = " + value.text);
    while (m_history.size() > size_t(kMaximumHistory))
        m_history.erase(m_history.begin());
    m_lastExpression = m_expression;
    m_result = value.text;
    m_expression = value.text;
    m_justEvaluated = true;
    refresh();
}

void ScientificPage::copyResult()
{
    const std::string value = m_result.empty() ? std::string("0") : m_result;
    Clipboard::setText(value);
    if (m_copyButton) {
        m_copyButton->setText(tr("Copied"));
        m_copiedTimer.start();
    }
}

void ScientificPage::focus()
{
    setFocus(FocusReason::Keyboard);
}

void ScientificPage::toggleSign()
{
    if (m_expression.empty() || m_expression.back() == '(')
        append("-");
    else if (m_expression.front() == '-')
        m_expression.erase(m_expression.begin());
    else
        m_expression.insert(m_expression.begin(), '-');
    m_justEvaluated = false;
    refresh();
}

void ScientificPage::paint(Painter& painter)
{
    const Theme& theme = Theme::current();
    painter.fillRect(localRect(), theme.background);
    const RectF display = displayRect();
    painter.fillRoundedRect(display, theme.bigRadius, theme.secondBackground);
    painter.drawSmoothRectangle(display, theme.bigRadius, Color{0, 0, 0, 0}, 1,
        theme.disabledText.withAlpha(static_cast<uint8_t>(theme.darkMode ? 18 : 12)));
}

void ScientificPage::layout()
{
    const RectF rect = localRect();
    const double margin = 16;
    const RectF display = displayRect();
    const double inner = 16;

    // Angle toggle in the top-right corner, above the display.
    const SizeF angleHint = m_angleToggle->sizeHint();
    m_angleToggle->setGeometry({rect.width - margin - 120, margin, 120, 26});

    // Display starts below the toggle.
    const double displayTop = 16;
    const double displayRight = rect.width - margin;
    const double displayLeft = margin;
    for (int i = 0; i < 3; ++i) {
        m_historyLines[i]->setGeometry({display.x + inner, display.y + 12 + i * 17,
                                        display.width - 2 * inner, 15});
    }
    m_expressionLabel->setGeometry({display.x + inner, display.bottom() - 78,
                                    display.width - 2 * inner, 24});
    m_resultLabel->setGeometry({display.x + inner, display.bottom() - 54,
                                display.width - 2 * inner, 46});

    // Left function grid: 5 rows × 4 cols, width 190.
    const double funcLeft = margin;
    const double funcTop = display.bottom() + 12;
    const double funcWidth = 190;
    const double funcGap = 6;
    const double funcCellW = (funcWidth - funcGap * 3) / 4;
    const double funcCellH = 32;
    for (std::size_t i = 0; i < m_funcButtons.size(); ++i) {
        const int row = int(i / 4), col = int(i % 4);
        m_funcButtons[i]->setGeometry({funcLeft + col * (funcCellW + funcGap),
                                        funcTop + row * (funcCellH + funcGap),
                                        funcCellW, funcCellH});
    }

    // Right side: utility row + 4×5 digit grid, same as standard.
    const double rightLeft = margin + funcWidth + 12;
    const double rightWidth = rect.width - rightLeft - margin;

    const double utilityTop = display.bottom() + 12;
    const double utilityHeight = 28;
    const double utilityGap = 8;
    const double utilityWidth = (rightWidth - utilityGap * 4) / 5;
    for (std::size_t i = 0; i < m_utilityButtons.size(); ++i)
        m_utilityButtons[i]->setGeometry({rightLeft + i * (utilityWidth + utilityGap),
                                           utilityTop, utilityWidth, utilityHeight});

    const double top = utilityTop + utilityHeight + 8;
    const double gridHeight = rect.height - top - margin;
    const double gap = 8;
    const double cellWidth = (rightWidth - gap * 3) / 4;
    const double cellHeight = (gridHeight - gap * 4) / 5;
    for (std::size_t i = 0; i < m_buttons.size(); ++i) {
        const int row = int(i / 4), column = int(i % 4);
        m_buttons[i]->setGeometry({rightLeft + column * (cellWidth + gap),
                                   top + row * (cellHeight + gap), cellWidth, cellHeight});
    }
}

bool ScientificPage::keyPress(const KeyEvent& event)
{
    if (event.modifiers & ControlModifier) {
        if (event.key == key::letter('c')) { copyResult(); return true; }
    }
    if (!event.text.empty()) {
        const char c = event.text.front();
        if (c == '=') { evaluate(); return true; }
        if (std::isdigit(static_cast<unsigned char>(c)) || c == '.' || c == '+' || c == '-'
            || c == '*' || c == '/' || c == '%' || c == '(' || c == ')' || c == '^') {
            append(event.text);
            return true;
        }
    }
    switch (event.key) {
    case key::Return:   evaluate();  return true;
    case key::BackSpace:
    case key::Delete:   backspace(); return true;
    case key::Escape:   clear();     return true;
    default:            return false;
    }
}

void ScientificPage::pointerPress(const PointerEvent&)
{
    setFocus(FocusReason::Mouse);
}

RectF ScientificPage::displayRect() const
{
    return {16, 16, std::max(0.0, geometry().width - 32), 138};
}

void ScientificPage::addFunctionKeys()
{
    struct Fn { const char* label; const char* insert; };
    // 5 rows × 4 cols = 20 buttons.
    const Fn fns[] = {
        {"sin", "sin("},  {"cos", "cos("},  {"tan", "tan("},  {"asin", "asin("},
        {"acos", "acos("},{"atan", "atan("},{"sinh", "sinh("},{"cosh", "cosh("},
        {"√", "sqrt("},   {"∛", "cbrt("},   {"ln", "ln("},    {"log", "log("},
        {"exp", "exp("},  {"abs", "abs("},  {"⌊⌋", "floor("}, {"⌈⌉", "ceil("},
        {"π", "3.1415926536"}, {"e", "2.718281828"}, {"n!", "fact("}, {"Copy", "COPY"},
    };
    for (const Fn& f : fns) {
        CalcButton* btn = addChild<CalcButton>(f.label, false, 12);
        const std::string insert = f.insert;
        btn->onClicked = [this, insert] {
            if (insert == "COPY") copyResult();
            else append(insert);
        };
        if (insert == "COPY") m_copyButton = btn;
        m_funcButtons.push_back(btn);
    }
}

void ScientificPage::addStandardKeys()
{
    struct Key { const char* text; const char* insert; bool accent; };
    const Key keys[] = {
        {"C",  "C",    false}, {"←", "BACKSPACE", false}, {"%",  "%",  false}, {"÷", "/", true},
        {"7",  "7",    false}, {"8", "8",          false}, {"9",  "9",  false}, {"×", "*", true},
        {"4",  "4",    false}, {"5", "5",          false}, {"6",  "6",  false}, {"−", "-", true},
        {"1",  "1",    false}, {"2", "2",          false}, {"3",  "3",  false}, {"+", "+", true},
        {"±",  "SIGN", false}, {"0", "0",          false}, {".",  ".",  false}, {"=", "=", true},
    };
    for (const Key& k : keys) {
        CalcButton* btn = addChild<CalcButton>(k.text, k.accent, k.accent ? 22 : 18);
        const std::string insert = k.insert;
        btn->onClicked = [this, insert] {
            if (insert == "C")         clear();
            else if (insert == "BACKSPACE") backspace();
            else if (insert == "SIGN")  toggleSign();
            else if (insert == "=")     evaluate();
            else                        append(insert);
        };
        m_buttons.push_back(btn);
    }
}

void ScientificPage::addUtilityKeys()
{
    struct U { const char* text; const char* action; };
    const U utilities[] = {{"(", "("}, {")", ")"}, {"^", "^"}, {"x²", "^2"}, {"Copy", "COPY"}};
    for (const U& u : utilities) {
        CalcButton* btn = addChild<CalcButton>(u.text, false, 12.5);
        const std::string action = u.action;
        btn->onClicked = [this, action] {
            if (action == "COPY") copyResult();
            else append(action);
        };
        m_utilityButtons.push_back(btn);
    }
}

void ScientificPage::refresh()
{
    const std::size_t count = m_history.size();
    for (int i = 0; i < 3; ++i) {
        const std::size_t offset = count >= std::size_t(3 - i) ? count - std::size_t(3 - i) : count;
        m_historyLines[i]->setText(offset < count ? m_history[offset] : std::string());
    }
    m_expressionLabel->setText(m_justEvaluated ? m_lastExpression : m_expression);
    m_resultLabel->setText(m_result.empty() ? std::string("0") : m_result);
    requestLayout();
    requestPaint();
}

} // namespace calculator
