# SPDX-FileCopyrightText: 2026 LingmoOS Contributors
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.16)

project(lingmo-calculator VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)
include(CTest)

find_package(LingmoSDK REQUIRED)

# HMath and its arbitrary precision number library are the old SpeedCrunch
# engine, kept under their GPL-2.0-or-later notices; the expression reader
# around them is new.
add_library(calculator-core STATIC
    src/core/calculator.cpp
    src/core/programmer_calc.cpp
    src/core/date_calc.cpp
    engine/hmath.cpp
    engine/number.c
)
target_include_directories(calculator-core PUBLIC src engine)

add_library(calculator-ui STATIC
    src/calculatorwindow.cpp
    src/modes/standard_page.cpp
    src/modes/scientific_page.cpp
    src/modes/programmer_page.cpp
    src/modes/date_page.cpp
    src/modes/graph_page.cpp
)
target_include_directories(calculator-ui PUBLIC src)
target_link_libraries(calculator-ui PUBLIC calculator-core Lingmo::SDK)

add_executable(lingmo-calculator src/main.cpp)
target_compile_definitions(lingmo-calculator PRIVATE CALCULATOR_VERSION="${PROJECT_VERSION}")
target_link_libraries(lingmo-calculator PRIVATE calculator-ui)

install(TARGETS lingmo-calculator RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

lingmo_add_translations(lingmo-calculator SOURCES src DESKTOP_FILES lingmo-calculator.desktop)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/lingmo-calculator.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
