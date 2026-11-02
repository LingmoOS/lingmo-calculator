// SPDX-FileCopyrightText: 2026 LingmoOS Contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "calculatorwindow.h"

#include "base/event_loop.h"
#include "ui/application.h"
#include "ui/theme.h"
#include "ui/window.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <poll.h>
#include <string>

using namespace Lingmo;
using namespace Lingmo::ui;

namespace {
void spin(int milliseconds)
{
    base::EventLoop &loop = Application::instance()->loop();
    const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
    while (std::chrono::steady_clock::now() < end) {
        pollfd descriptor{loop.fd(), POLLIN, 0};
        ::poll(&descriptor, 1, 10);
        loop.dispatch();
    }
}
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s OUTDIR\n", argv[0]);
        return 1;
    }
    const std::string directory = argv[1];
    std::filesystem::create_directories(directory);

    Application app(argc, argv, Application::Platform::Headless);
    app.setApplicationId("lingmo-calculator");

    for (const bool dark : {false, true}) {
        Theme::setCurrent(dark ? Theme::dark() : Theme::light());
        calculator::CalculatorWindow window;
        window.window().setScale(1.5);
        window.window().resize({400, 620});
        window.append(dark ? "12*8" : "12+30*2");
        window.evaluate();
        window.window().show();
        window.focus();
        spin(150);
        window.window().grab().savePng(directory + "/calculator-" + (dark ? "dark" : "light") + ".png");
    }
    return 0;
}
