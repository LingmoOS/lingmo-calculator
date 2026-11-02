// SPDX-FileCopyrightText: 2026 LingmoOS Contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "calculatorwindow.h"

#include "base/i18n.h"
#include "ui/application.h"

#include <cstdio>
#include <string>

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "-h" || argument == "--help") {
            std::printf("Usage: lingmo-calculator\nLingmo Calculator\n");
            return 0;
        }
        if (argument == "-v" || argument == "--version") {
            std::printf("lingmo-calculator %s\n", CALCULATOR_VERSION);
            return 0;
        }
    }

    Lingmo::ui::Application app(argc, argv);
    app.setApplicationId("lingmo-calculator");
    Lingmo::i18n::setDomain("lingmo-calculator");

    calculator::CalculatorWindow window;
    window.window().show();
    window.focus();
    return app.run();
}
