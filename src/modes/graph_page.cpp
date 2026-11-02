// SPDX-FileCopyrightText: 2026 LingmoOS Contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "modes/graph_page.h"

#include "core/calculator.h"

#include "base/i18n.h"
#include "ui/button.h"
#include "ui/canvas_2d.h"
#include "ui/color_id.h"
#include "ui/keys.h"
#include "ui/label.h"
#include "ui/painter.h"
#include "ui/path.h"
#include "ui/text_field.h"
#include "ui/theme.h"
#include "ui/view.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

using namespace Lingmo::ui;
using Lingmo::i18n::tr;

namespace calculator {

// ---- GraphView -----------------------------------------------------------

class GraphPage::GraphView : public View
{
public:
    GraphView()
    {
        setCursorShape(CursorShape::Move);
        setTracksHover(true);
    }

    void setExpression(const std::string& expr)
    {
        m_expression = expr;
        sample();
        requestPaint();
    }

    void resetView()
    {
        m_xMin = -10.0;
        m_xMax =  10.0;
        m_yMin = std::numeric_limits<double>::quiet_NaN();
        m_yMax = std::numeric_limits<double>::quiet_NaN();
        sample();
        requestPaint();
    }

    bool hasError() const { return m_allNaN; }
    bool isEmpty()  const { return m_expression.empty(); }

    bool acceptsPointer() const override { return true; }

protected:
    void paint(Painter& painter) override
    {
        const Theme& theme = Theme::current();
        const RectF rect = localRect();

        painter.fillRect(rect, theme.secondBackground);

        Canvas2D canvas(painter);
        canvas.save();

        const double w = rect.width;
        const double h = rect.height;

        const double yMin = std::isnan(m_yMin) ? -10.0 : m_yMin;
        const double yMax = std::isnan(m_yMax) ?  10.0 : m_yMax;
        const double xRange = m_xMax - m_xMin;
        const double yRange = yMax - yMin;
        if (xRange <= 0 || yRange <= 0) {
            canvas.restore();
            return;
        }

        // Map world → canvas.
        auto wx = [&](double x) { return (x - m_xMin) / xRange * w; };
        auto wy = [&](double y) { return h - (y - yMin) / yRange * h; };

        // Grid lines.
        canvas.setStrokeColor({
            theme.disabledText.r / 255.0,
            theme.disabledText.g / 255.0,
            theme.disabledText.b / 255.0,
            0.07});
        canvas.setLineWidth(0.5);

        const double xStep = niceStep(xRange, 8);
        const double xStart = std::ceil(m_xMin / xStep) * xStep;
        for (double x = xStart; x <= m_xMax + 1e-9; x += xStep) {
            const double px = wx(x);
            Path line;
            line.moveTo({px, 0});
            line.lineTo({px, h});
            canvas.strokePath(line);
        }

        const double yStep = niceStep(yRange, 6);
        const double yStart = std::ceil(yMin / yStep) * yStep;
        for (double y = yStart; y <= yMax + 1e-9; y += yStep) {
            const double py = wy(y);
            Path line;
            line.moveTo({0, py});
            line.lineTo({w, py});
            canvas.strokePath(line);
        }

        // Axes.
        canvas.setStrokeColor({
            theme.disabledText.r / 255.0,
            theme.disabledText.g / 255.0,
            theme.disabledText.b / 255.0,
            0.28});
        canvas.setLineWidth(1.0);

        if (m_xMin <= 0 && 0 <= m_xMax) {
            const double px = wx(0);
            Path line;
            line.moveTo({px, 0});
            line.lineTo({px, h});
            canvas.strokePath(line);
        }
        if (yMin <= 0 && 0 <= yMax) {
            const double py = wy(0);
            Path line;
            line.moveTo({0, py});
            line.lineTo({w, py});
            canvas.strokePath(line);
        }

        // Curve.
        if (!m_expression.empty() && !m_samples.empty()) {
            canvas.setStrokeColor({
                theme.highlight.r / 255.0,
                theme.highlight.g / 255.0,
                theme.highlight.b / 255.0,
                1.0});
            canvas.setLineWidth(2.0);

            Path curve;
            bool penDown = false;
            for (const auto& pt : m_samples) {
                if (std::isnan(pt.second)) {
                    if (penDown) {
                        canvas.strokePath(curve);
                        curve = Path();
                        penDown = false;
                    }
                    continue;
                }
                const double px = wx(pt.first);
                const double py = wy(pt.second);
                if (!penDown) {
                    curve.moveTo({px, py});
                    penDown = true;
                } else {
                    curve.lineTo({px, py});
                }
            }
            if (penDown)
                canvas.strokePath(curve);
        }

        canvas.restore();
    }

    void pointerPress(const PointerEvent& event) override
    {
        if (event.button == MouseButton::Left) {
            m_dragging = true;
            m_dragStart = event.position;
            m_dragXMin  = m_xMin;
            m_dragXMax  = m_xMax;
        }
    }

    void pointerMove(const PointerEvent& event) override
    {
        if (!m_dragging) return;
        const double dx = event.position.x - m_dragStart.x;
        const double w  = std::max(1.0, geometry().width);
        const double shift = -dx / w * (m_dragXMax - m_dragXMin);
        m_xMin = m_dragXMin + shift;
        m_xMax = m_dragXMax + shift;
        sample();
        requestPaint();
    }

    void pointerRelease(const PointerEvent&) override
    {
        m_dragging = false;
    }

    bool scroll(const ScrollEvent& event) override
    {
        const double factor = (event.deltaY > 0) ? 0.85 : 1.0 / 0.85;
        const double centre = (m_xMin + m_xMax) / 2.0;
        m_xMin = centre + (m_xMin - centre) * factor;
        m_xMax = centre + (m_xMax - centre) * factor;
        sample();
        requestPaint();
        return true;
    }

private:
    static double niceStep(double range, int targetLines)
    {
        const double rough = range / targetLines;
        const double magnitude = std::pow(10.0, std::floor(std::log10(rough)));
        const double normalized = rough / magnitude;
        double nice = (normalized < 1.5) ? 1 : (normalized < 3.5) ? 2 : (normalized < 7.5) ? 5 : 10;
        return nice * magnitude;
    }

    void sample()
    {
        m_samples.clear();
        m_allNaN = false;
        if (m_expression.empty()) return;

        const ScientificCalculator engine(AngleUnit::Radians);
        const int N = 600;
        const double step = (m_xMax - m_xMin) / N;

        double yMin = std::numeric_limits<double>::max();
        double yMax = std::numeric_limits<double>::lowest();
        bool anyValid = false;

        m_samples.reserve(N + 1);
        for (int i = 0; i <= N; ++i) {
            const double x = m_xMin + i * step;
            const double y = engine.evaluateDouble(m_expression, "x", x);
            m_samples.push_back({x, y});
            if (!std::isnan(y) && !std::isinf(y)) {
                yMin = std::min(yMin, y);
                yMax = std::max(yMax, y);
                anyValid = true;
            }
        }

        m_allNaN = !anyValid;
        if (anyValid && std::isnan(m_yMin)) {
            const double pad = (yMax - yMin) * 0.1;
            m_yMin = yMin - pad - 0.1;
            m_yMax = yMax + pad + 0.1;
        }
    }

    std::string m_expression;
    std::vector<std::pair<double, double>> m_samples;

    double m_xMin = -10.0;
    double m_xMax =  10.0;
    double m_yMin = std::numeric_limits<double>::quiet_NaN();
    double m_yMax = std::numeric_limits<double>::quiet_NaN();

    PointF m_dragStart{};
    double m_dragXMin = -10.0;
    double m_dragXMax =  10.0;
    bool m_dragging = false;
    bool m_allNaN = false;
};

// ---- GraphPage -----------------------------------------------------------

GraphPage::GraphPage()
{
    m_input = addChild<TextField>();
    m_input->setPlaceholder(tr("Enter a function of x, e.g. sin(x) or x^2-1"));
    m_input->onTextChanged = [this] { replot(); };

    m_resetButton = addChild<Button>(tr("Reset View"));
    m_resetButton->onClicked = [this] {
        m_graph->resetView();
        m_errorLabel->setText({});
    };

    m_errorLabel = addChild<Label>();
    m_errorLabel->setFontSize(12);
    m_errorLabel->setColor(ColorRecipe(ColorId::DisabledText));
    m_errorLabel->setTextAlignment(Label::TextAlignment::Center);

    m_graph = addChild<GraphView>();
}

void GraphPage::replot()
{
    const std::string expr = m_input->text();
    m_graph->setExpression(expr);
    if (!expr.empty() && m_graph->hasError())
        m_errorLabel->setText(tr("Could not evaluate this expression"));
    else
        m_errorLabel->setText({});
    requestLayout();
}

void GraphPage::paint(Painter& painter)
{
    painter.fillRect(localRect(), Theme::current().background);
}

void GraphPage::layout()
{
    const RectF rect = localRect();
    const double margin = 16;
    const double w = rect.width - 2 * margin;

    const double inputH = 36;
    m_input->setGeometry({margin, margin, std::max(0.0, w - 120), inputH});
    m_resetButton->setGeometry({margin + w - 112, margin, 112, inputH});

    const double errorH = 18;
    m_errorLabel->setGeometry({margin, margin + inputH + 4, w, errorH});

    const double graphTop = margin + inputH + 4 + errorH + 6;
    m_graph->setGeometry({margin, graphTop,
                          std::max(0.0, rect.width - 2 * margin),
                          std::max(0.0, rect.height - graphTop - margin)});
}

bool GraphPage::keyPress(const KeyEvent& event)
{
    if (event.key == key::Return) { replot(); return true; }
    return false;
}

} // namespace calculator
