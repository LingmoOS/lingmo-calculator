// SPDX-FileCopyrightText: 2026 LingmoOS Contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/calculator.h"

#include "ui/view.h"

#include <string>

namespace Lingmo::ui {
class Button;
class Label;
class TextField;
}

namespace calculator {

// The graph page: plots y = f(x) with pan and zoom.
class GraphPage : public Lingmo::ui::View
{
public:
    GraphPage();

    Lingmo::ui::SizeF sizeHint() const override { return {640, 540}; }

protected:
    void paint(Lingmo::ui::Painter& painter) override;
    void layout() override;
    bool keyPress(const Lingmo::ui::KeyEvent& event) override;

private:
    class GraphView;

    void replot();

    Lingmo::ui::TextField* m_input = nullptr;
    Lingmo::ui::Button* m_resetButton = nullptr;
    Lingmo::ui::Label* m_errorLabel = nullptr;
    GraphView* m_graph = nullptr;
};

} // namespace calculator
