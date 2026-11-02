// SPDX-FileCopyrightText: 2026 LingmoOS Contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "modes/programmer_page.h"

#include "base/i18n.h"
#include "ui/color_id.h"
#include "ui/combo_box.h"
#include "ui/keys.h"
#include "ui/label.h"
#include "ui/painter.h"
#include "ui/segmented_control.h"
#include "ui/theme.h"
#include "ui/view.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

using namespace Lingmo::ui;
using Lingmo::i18n::tr;

namespace calculator {

// CalcButton moved out of anonymous namespace so addChild<CalcButton> can instantiate it
class ProgrammerPage::CalcButton : public View
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

    void setEnabled(bool enabled)
    {
        if (m_enabled == enabled) return;
        m_enabled = enabled;
        requestPaint();
    }

    bool isEnabled() const { return m_enabled; }
    bool acceptsPointer() const override { return m_enabled; }

protected:
    void layout() override
    {
        const Theme& theme = Theme::current();
        m_label->setGeometry(localRect());
        Color c = m_accent ? theme.highlightedText : theme.text;
        if (!m_enabled) c = theme.disabledText;
        m_label->setColor(c);
    }

    void paint(Painter& painter) override
    {
        const Theme& theme = Theme::current();
        if (!m_enabled) {
            painter.fillRoundedRect(localRect(), theme.mediumRadius,
                                    theme.disabledText.withAlpha(0.04));
            return;
        }
        Color fill = m_accent ? theme.highlight : theme.secondBackground;
        if (m_pressed)
            fill = m_accent ? theme.highlight.darker(1.12) : theme.disabledText.withAlpha(0.16);
        else if (isHovered())
            fill = m_accent ? theme.highlight.lighter(1.08) : theme.disabledText.withAlpha(0.09);
        painter.fillRoundedRect(localRect(), theme.mediumRadius, fill);
    }

    void pointerPress(const PointerEvent& event) override
    {
        if (event.button != MouseButton::Left || !m_enabled) return;
        m_pressed = true;
        requestPaint();
    }

    void pointerRelease(const PointerEvent& event) override
    {
        if (!m_pressed) return;
        m_pressed = false;
        requestPaint();
        if (m_enabled && localRect().contains(event.position) && onClicked)
            onClicked();
    }

    void hoverChanged() override { requestPaint(); }

private:
    std::string m_text;
    bool m_accent = false;
    bool m_pressed = false;
    bool m_enabled = true;
    Label* m_label = nullptr;
};

ProgrammerPage::ProgrammerPage()
{
    m_baseControl = addChild<SegmentedControl>(
        std::vector<std::string>{"HEX", "DEC", "OCT", "BIN"},
        SegmentedControl::Style::Segmented);
    m_baseControl->setCurrentIndex(1); // DEC default
    m_baseControl->setContentHeight(30);
    m_baseControl->onCurrentChanged = [this](int index) {
        const Base bases[] = {Base::Hex, Base::Dec, Base::Oct, Base::Bin};
        setBase(bases[index]);
    };

    m_widthCombo = addChild<ComboBox>(
        std::vector<std::string>{"64-bit", "32-bit", "16-bit", "8-bit"});
    m_widthCombo->onCurrentChanged = [this](int index) {
        const BitWidth widths[] = {BitWidth::Bits64, BitWidth::Bits32, BitWidth::Bits16, BitWidth::Bits8};
        m_calc.setBitWidth(widths[index]);
        refresh();
    };

    // Base-label captions (right-aligned tags).
    auto makeCaption = [this](const char* text) {
        Label* l = addChild<Label>(text);
        l->setFontSize(11);
        l->setWeight(600);
        l->setColor(ColorRecipe(ColorId::DisabledText));
        return l;
    };
    m_hexCaption = makeCaption("HEX");
    m_decCaption = makeCaption("DEC");
    m_octCaption = makeCaption("OCT");
    m_binCaption = makeCaption("BIN");

    auto makeValueLabel = [this]() {
        Label* l = addChild<Label>();
        l->setFontSize(14);
        l->setTextAlignment(Label::TextAlignment::Right);
        l->setElide(Label::Elide::Middle);
        return l;
    };
    m_hexLabel = makeValueLabel();
    m_decLabel = makeValueLabel();
    m_octLabel = makeValueLabel();
    m_binLabel = makeValueLabel();

    m_expressionLabel = addChild<Label>();
    m_expressionLabel->setTextAlignment(Label::TextAlignment::Right);
    m_expressionLabel->setFontSize(13);
    m_expressionLabel->setColor(ColorRecipe(ColorId::DisabledText));
    m_expressionLabel->setElide(Label::Elide::Middle);

    m_resultLabel = addChild<Label>("0");
    m_resultLabel->setTextAlignment(Label::TextAlignment::Right);
    m_resultLabel->setFontSize(32);
    m_resultLabel->setWeight(600);
    m_resultLabel->setElide(Label::Elide::Middle);

    addDigitKeys();
    addOperatorKeys();
    addBitOpKeys();
    refresh();
}

void ProgrammerPage::setBase(Base base)
{
    m_calc.setBase(base);
    const int indices[] = {0, 1, 2, 3};
    const Base bases[] = {Base::Hex, Base::Dec, Base::Oct, Base::Bin};
    for (int i = 0; i < 4; ++i) {
        if (bases[i] == base) {
            m_baseControl->setCurrentIndex(i);
            break;
        }
    }
    m_expression.clear();
    m_result.clear();
    m_currentValue = 0;
    updateKeyAvailability();
    refresh();
}

void ProgrammerPage::append(const std::string& text)
{
    if (m_expression.size() + text.size() > 64) return;
    m_expression += text;
    refresh();
}

void ProgrammerPage::backspace()
{
    if (m_expression.empty()) return;
    m_expression.pop_back();
    refresh();
}

void ProgrammerPage::clear()
{
    m_expression.clear();
    m_result.clear();
    m_currentValue = 0;
    refresh();
}

void ProgrammerPage::evaluate()
{
    if (m_expression.empty()) return;
    const ProgrammerResult result = m_calc.evaluate(m_expression);
    if (!result.ok) {
        m_result = tr("Error");
        requestPaint();
        return;
    }
    m_currentValue = result.value;
    m_result = result.inBase(m_calc.base());
    m_expression = m_result;
    refresh();
}

void ProgrammerPage::refresh()
{
    // Try a live evaluation of what is typed so far.
    if (!m_expression.empty()) {
        const ProgrammerResult r = m_calc.evaluate(m_expression);
        if (r.ok) {
            m_currentValue = r.value;
            m_result = r.inBase(m_calc.base());
        }
    }

    m_expressionLabel->setText(m_expression);
    m_resultLabel->setText(m_result.empty() ? "0" : m_result);

    updateBaseLabels();
    requestLayout();
    requestPaint();
}

void ProgrammerPage::updateBaseLabels()
{
    m_hexLabel->setText(ProgrammerCalculator::formatHex(m_currentValue));
    m_decLabel->setText(ProgrammerCalculator::formatDec(m_currentValue));
    m_octLabel->setText(ProgrammerCalculator::formatOct(m_currentValue));
    m_binLabel->setText(ProgrammerCalculator::formatBin(m_currentValue));

    // Highlight the active base.
    const Theme& theme = Theme::current();
    m_hexLabel->setColor(m_calc.base() == Base::Hex ? theme.text : theme.disabledText);
    m_decLabel->setColor(m_calc.base() == Base::Dec ? theme.text : theme.disabledText);
    m_octLabel->setColor(m_calc.base() == Base::Oct ? theme.text : theme.disabledText);
    m_binLabel->setColor(m_calc.base() == Base::Bin ? theme.text : theme.disabledText);
}

void ProgrammerPage::updateKeyAvailability()
{
    // Digit buttons: A-F (indices 10-15) only in hex; 8-9 only in dec/hex; 2-7 only in oct+.
    for (std::size_t i = 0; i < m_digitButtons.size(); ++i) {
        int digit = int(i);
        bool enabled = true;
        switch (m_calc.base()) {
        case Base::Bin: enabled = (digit <= 1);  break;
        case Base::Oct: enabled = (digit <= 7);  break;
        case Base::Dec: enabled = (digit <= 9);  break;
        case Base::Hex: enabled = true;           break;
        }
        m_digitButtons[i]->setEnabled(enabled);
    }
}

void ProgrammerPage::paint(Painter& painter)
{
    const Theme& theme = Theme::current();
    painter.fillRect(localRect(), theme.background);

    // Base-representations panel.
    const RectF panel{16, 52, std::max(0.0, geometry().width - 32), 92};
    painter.fillRoundedRect(panel, theme.bigRadius, theme.secondBackground);
}

void ProgrammerPage::layout()
{
    const RectF rect = localRect();
    const double margin = 16;
    const double w = rect.width - 2 * margin;

    m_baseControl->setGeometry({margin, margin, w - 130, 30});
    m_widthCombo->setGeometry({margin + w - 120, margin, 120, 30});

    // 4 base-label rows inside the panel.
    const double panelTop = 52;
    const double capW = 40;
    const double rowH = 22;
    const double gap = 1;
    for (int i = 0; i < 4; ++i) {
        Label* cap = (i == 0 ? m_hexCaption : i == 1 ? m_decCaption : i == 2 ? m_octCaption : m_binCaption);
        Label* val = (i == 0 ? m_hexLabel   : i == 1 ? m_decLabel   : i == 2 ? m_octLabel   : m_binLabel);
        const double y = panelTop + 4 + i * (rowH + gap);
        cap->setGeometry({margin + 8, y, capW, rowH});
        val->setGeometry({margin + capW + 12, y, std::max(0.0, w - capW - 20), rowH});
    }

    // Expression + result below the panel.
    const double exprTop = panelTop + 96;
    m_expressionLabel->setGeometry({margin, exprTop, w, 18});
    m_resultLabel->setGeometry({margin, exprTop + 20, w, 40});

    // Keypad: 6 rows × 5 cols for operator/digit; 2 rows × 6 cols for bit ops.
    const double keyTop = exprTop + 66;
    const double keyH = rect.height - keyTop - margin;
    const double gap2 = 7;

    // Bit-op row at bottom: 6 buttons.
    const double bitOpH = 32;
    const double bitOpW = (w - gap2 * 5) / 6;
    const double bitOpY = rect.height - margin - bitOpH;
    for (std::size_t i = 0; i < m_bitOpButtons.size(); ++i)
        m_bitOpButtons[i]->setGeometry({margin + i * (bitOpW + gap2), bitOpY, bitOpW, bitOpH});

    // Digit/operator grid: 5 cols, 4 rows.
    const double gridH = bitOpY - keyTop - gap2;
    const double cellW = (w - gap2 * 4) / 5;
    const double cellH = (gridH - gap2 * 3) / 4;

    // 16 digit buttons in 4×4 then 4 operator buttons as the 5th column.
    for (std::size_t i = 0; i < m_digitButtons.size() && i < 16; ++i) {
        const int row = int(i / 4), col = int(i % 4);
        m_digitButtons[i]->setGeometry({margin + col * (cellW + gap2),
                                         keyTop + row * (cellH + gap2), cellW, cellH});
    }
    for (std::size_t i = 0; i < m_operatorButtons.size() && i < 4; ++i) {
        m_operatorButtons[i]->setGeometry({margin + 4 * (cellW + gap2),
                                            keyTop + i * (cellH + gap2), cellW, cellH});
    }
}

bool ProgrammerPage::keyPress(const KeyEvent& event)
{
    if (!event.text.empty()) {
        const char c = event.text.front();
        if (c == '=') { evaluate(); return true; }
        const std::string allowed = "0123456789abcdefABCDEF+-*/()&|^~<>";
        if (allowed.find(c) != std::string::npos) { append(event.text); return true; }
    }
    switch (event.key) {
    case key::Return:   evaluate();  return true;
    case key::BackSpace:
    case key::Delete:   backspace(); return true;
    case key::Escape:   clear();     return true;
    default:            return false;
    }
}

void ProgrammerPage::pointerPress(const PointerEvent&)
{
    setFocus(FocusReason::Mouse);
}

void ProgrammerPage::addDigitKeys()
{
    // 0-9 then A-F.
    const char* labels[] = {"0","1","2","3","4","5","6","7","8","9","A","B","C","D","E","F"};
    // Layout: 4×4 grid of digits, row-major order:
    // Row 0: D E F C(clear)  → but we want 0-9 first. Let's lay them out cleanly:
    // We'll put them in order 7 8 9 / 4 5 6 / 1 2 3 / 0 . ← =  so the grid flows naturally.
    // Actually just add all 16 in index order; layout() maps them row by row.
    for (int i = 0; i < 16; ++i) {
        CalcButton* btn = addChild<CalcButton>(labels[i], false, 16);
        const std::string val = labels[i];
        btn->onClicked = [this, val] { append(val); };
        m_digitButtons.push_back(btn);
    }
}

void ProgrammerPage::addOperatorKeys()
{
    struct Op { const char* label; const char* insert; bool accent; };
    const Op ops[] = {
        {"÷", "/",  true},
        {"×", "*",  true},
        {"−", "-",  true},
        {"+", "+",  true},
    };
    for (const Op& op : ops) {
        CalcButton* btn = addChild<CalcButton>(op.label, op.accent, 20);
        const std::string insert = op.insert;
        btn->onClicked = [this, insert] { append(insert); };
        m_operatorButtons.push_back(btn);
    }
    // Also add = ← C as part of digit grid extras (we add them to digitButtons[16..18]).
    struct Extra { const char* label; bool accent; };
    const Extra extras[] = {{"=", true}, {"←", false}, {"C", false}};
    for (const Extra& e : extras) {
        CalcButton* btn = addChild<CalcButton>(e.label, e.accent, 18);
        const std::string lbl = e.label;
        btn->onClicked = [this, lbl] {
            if (lbl == "=")       evaluate();
            else if (lbl == "←")  backspace();
            else                  clear();
        };
        m_digitButtons.push_back(btn);
    }
}

void ProgrammerPage::addBitOpKeys()
{
    struct BitOp { const char* label; const char* insert; };
    const BitOp ops[] = {
        {"AND", "&"},
        {"OR",  "|"},
        {"XOR", "xor"},
        {"NOT", "~"},
        {"LSH", "<<"},
        {"RSH", ">>"},
    };
    for (const BitOp& op : ops) {
        CalcButton* btn = addChild<CalcButton>(op.label, false, 12);
        const std::string insert = op.insert;
        btn->onClicked = [this, insert] { append(insert); };
        m_bitOpButtons.push_back(btn);
    }
}

} // namespace calculator
