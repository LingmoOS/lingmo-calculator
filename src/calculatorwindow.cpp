// SPDX-FileCopyrightText: 2026 LingmoOS Contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "calculatorwindow.h"

#include "core/calculator.h"

#include "base/i18n.h"
#include "base/timer.h"
#include "ui/application.h"
#include "ui/clipboard.h"
#include "ui/keys.h"
#include "ui/label.h"
#include "ui/painter.h"
#include "ui/theme.h"
#include "ui/view.h"
#include "ui/window.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

using namespace Lingmo::ui;
using Lingmo::i18n::tr;

namespace calculator {

namespace {

constexpr int kMaximumHistory = 40;
constexpr std::size_t kMaximumExpression = Calculator::kMaximumLength;

// One calculator key, drawn as a quiet plate with a centred glyph or digit.
class CalcButton : public View
{
public:
    CalcButton(std::string text, bool accent = false, double pointSize = 18)
        : m_text(std::move(text))
        , m_accent(accent)
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
        if (m_text == text)
            return;
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
        if (event.button != MouseButton::Left)
            return;
        m_pressed = true;
        requestPaint();
    }

    void pointerRelease(const PointerEvent& event) override
    {
        if (!m_pressed)
            return;
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

} // namespace

class CalculatorRoot : public View
{
public:
    CalculatorRoot()
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

        m_copiedTimer.setInterval(std::chrono::milliseconds(1400));
        m_copiedTimer.onTimeout = [this] {
            if (m_copyButton)
                m_copyButton->setText(tr("Copy"));
        };
        addKeys();
        addUtilityKeys();
        refresh();
    }

    void append(const std::string& text)
    {
        if (text.empty())
            return;
        if (m_justEvaluated) {
            if (std::isdigit(static_cast<unsigned char>(text.front())) || text == "." || text == "(")
                m_expression.clear();
            m_result.clear();
            m_lastExpression.clear();
            m_justEvaluated = false;
        }
        if (m_expression.size() + text.size() > kMaximumExpression)
            return;
        m_expression += text;
        refresh();
    }

    void backspace()
    {
        if (m_expression.empty())
            return;
        m_expression.pop_back();
        m_justEvaluated = false;
        refresh();
    }

    void clear()
    {
        if (!m_expression.empty() || !m_result.empty()) {
            m_expression.clear();
            m_result.clear();
        } else {
            m_history.clear();
        }
        m_lastExpression.clear();
        m_justEvaluated = false;
        refresh();
    }

    void clearHistory()
    {
        m_history.clear();
        refresh();
    }

    void copyResult()
    {
        const std::string value = m_result.empty() ? std::string("0") : m_result;
        Clipboard::setText(value);
        if (m_copyButton) {
            m_copyButton->setText(tr("Copied"));
            m_copiedTimer.start();
        }
    }

    void toggleSign()
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

    void evaluate()
    {
        if (m_expression.empty())
            return;
        const Calculator engine;
        const Result value = engine.evaluate(Calculator::autoFix(m_expression));
        if (!value.ok) {
            m_result = tr("Error");
            refresh();
            return;
        }
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

    const std::string& expression() const { return m_expression; }
    const std::string& result() const { return m_result; }
    const std::vector<std::string>& history() const { return m_history; }

    void focus() { setFocus(FocusReason::Keyboard); }

    bool acceptsFocus() const override { return true; }

    SizeF sizeHint() const override { return {400, 620}; }

protected:
    void paint(Painter& painter) override
    {
        const Theme& theme = Theme::current();
        painter.fillRect(localRect(), theme.background);
        const RectF display = displayRect();
        painter.fillRoundedRect(display, theme.bigRadius, theme.secondBackground);
        painter.drawSmoothRectangle(display, theme.bigRadius, Color {0, 0, 0, 0}, 1,
                                    theme.disabledText.withAlpha(
                                            static_cast<uint8_t>(theme.darkMode ? 18 : 12)));
    }

    void layout() override
    {
        const RectF rect = localRect();
        const double margin = 16;
        const RectF display = displayRect();
        const double inner = 16;
        for (int i = 0; i < 3; ++i) {
            const double y = display.y + 12 + i * 17;
            m_historyLines[i]->setGeometry({display.x + inner, y, display.width - 2 * inner, 15});
        }
        m_expressionLabel->setGeometry({display.x + inner, display.bottom() - 78,
                                   display.width - 2 * inner, 24});
        m_resultLabel->setGeometry({display.x + inner, display.bottom() - 54,
                               display.width - 2 * inner, 46});

        const double utilityTop = display.bottom() + 8;
        const double utilityHeight = 28;
        const double utilityGap = 8;
        const double utilityWidth = (rect.width - 2 * margin - utilityGap * 4) / 5;
        for (std::size_t i = 0; i < m_utilityButtons.size(); ++i) {
            m_utilityButtons[i]->setGeometry({margin + i * (utilityWidth + utilityGap), utilityTop,
                                              utilityWidth, utilityHeight});
        }

        const double top = utilityTop + utilityHeight + 8;
        const double width = rect.width - 2 * margin;
        const double height = rect.height - top - margin;
        const double gap = 8;
        const double cellWidth = (width - gap * 3) / 4;
        const double cellHeight = (height - gap * 4) / 5;
        for (std::size_t i = 0; i < m_buttons.size(); ++i) {
            const int row = int(i / 4);
            const int column = int(i % 4);
            m_buttons[i]->setGeometry({margin + column * (cellWidth + gap),
                                       top + row * (cellHeight + gap), cellWidth, cellHeight});
        }
    }

    bool keyPress(const KeyEvent& event) override
    {
        if (event.modifiers & ControlModifier) {
            if (event.key == key::letter('c')) {
                copyResult();
                return true;
            }
            if (event.key == key::letter('l')) {
                clearHistory();
                return true;
            }
        }
        if (!event.text.empty()) {
            const std::string& text = event.text;
            if (text.size() == 1) {
                const char c = text.front();
                if (std::isdigit(static_cast<unsigned char>(c)) || c == '.' || c == '+' || c == '-'
                    || c == '*' || c == '/' || c == '%' || c == '(' || c == ')' || c == '^') {
                    append(text);
                    return true;
                }
            }
        }
        switch (event.key) {
        case key::Return:
            evaluate();
            return true;
        case key::BackSpace:
        case key::Delete:
            backspace();
            return true;
        case key::Escape:
            clear();
            return true;
        default:
            return false;
        }
    }

    void pointerPress(const PointerEvent& event) override { setFocus(FocusReason::Mouse); }

private:
    RectF displayRect() const
    {
        return {16, 16, std::max(0.0, geometry().width - 32), 138};
    }

    void addKeys()
    {
        const struct Key
        {
            const char* text;
            const char* insert;
            bool accent;
        } keys[] = {
                {"C", "C", false},          {"←", "BACKSPACE", false},
                {"%", "%", false},          {"÷", "/", true},
                {"7", "7", false},          {"8", "8", false},
                {"9", "9", false},          {"×", "*", true},
                {"4", "4", false},          {"5", "5", false},
                {"6", "6", false},          {"−", "-", true},
                {"1", "1", false},          {"2", "2", false},
                {"3", "3", false},          {"+", "+", true},
                {"±", "SIGN", false},       {"0", "0", false},
                {".", ".", false},          {"=", "=", true},
        };
        for (const Key& key : keys) {
            CalcButton* button = addChild<CalcButton>(key.text, key.accent, key.accent ? 22 : 18);
            const std::string insert = key.insert;
            button->onClicked = [this, insert] {
                if (insert == "C")
                    clear();
                else if (insert == "BACKSPACE")
                    backspace();
                else if (insert == "SIGN")
                    toggleSign();
                else if (insert == "=")
                    evaluate();
                else
                    append(insert);
            };
            m_buttons.push_back(button);
        }
    }

    void addUtilityKeys()
    {
        struct Utility
        {
            const char* text;
            const char* action;
        };
        const Utility utilities[] = {
                {"(", "("}, {")", ")"}, {"^", "^"}, {"Copy", "COPY"}, {"Clear", "CLEAR"},
        };
        for (const Utility& utility : utilities) {
            CalcButton* button = addChild<CalcButton>(utility.text, false, 12.5);
            const std::string action = utility.action;
            button->onClicked = [this, action] {
                if (action == "COPY") {
                    copyResult();
                } else if (action == "CLEAR") {
                    clearHistory();
                } else {
                    append(action);
                }
            };
            if (action == "COPY")
                m_copyButton = button;
            m_utilityButtons.push_back(button);
        }
    }

    void refresh()
    {
        const std::size_t count = m_history.size();
        for (int i = 0; i < 3; ++i) {
            const std::size_t offset =
                    count >= std::size_t(3 - i) ? count - std::size_t(3 - i) : count;
            m_historyLines[i]->setText(offset < count ? m_history[offset] : std::string());
        }
        m_expressionLabel->setText(m_justEvaluated ? m_lastExpression : m_expression);
        m_resultLabel->setText(m_result.empty() ? std::string("0") : m_result);
        requestLayout();
        requestPaint();
    }

    Label* m_historyLines[3] = {};
    Label* m_expressionLabel = nullptr;
    Label* m_resultLabel = nullptr;
    CalcButton* m_copyButton = nullptr;
    std::vector<CalcButton*> m_buttons;
    std::vector<CalcButton*> m_utilityButtons;
    std::string m_expression;
    std::string m_result;
    std::string m_lastExpression;
    std::vector<std::string> m_history;
    base::Timer m_copiedTimer;
    bool m_justEvaluated = false;
};

CalculatorWindow::CalculatorWindow()
{
    m_window.setTitle(tr("Calculator"));
    m_window.resize({400, 620});
    m_window.setMinimumSize({340, 520});
    auto root = std::make_unique<CalculatorRoot>();
    m_root = root.get();
    m_window.setContentView(std::move(root));
}

CalculatorWindow::~CalculatorWindow() = default;

void CalculatorWindow::append(const std::string& text) { m_root->append(text); }
void CalculatorWindow::backspace() { m_root->backspace(); }
void CalculatorWindow::clear() { m_root->clear(); }
void CalculatorWindow::clearHistory() { m_root->clearHistory(); }
void CalculatorWindow::evaluate() { m_root->evaluate(); }
void CalculatorWindow::copyResult() { m_root->copyResult(); }
const std::string& CalculatorWindow::expression() const { return m_root->expression(); }
std::string CalculatorWindow::result() const
{
    return m_root->result().empty() ? std::string("0") : m_root->result();
}
const std::vector<std::string>& CalculatorWindow::history() const { return m_root->history(); }
void CalculatorWindow::focus() { m_root->focus(); }

} // namespace calculator
