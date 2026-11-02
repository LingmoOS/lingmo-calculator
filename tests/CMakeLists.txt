# SPDX-FileCopyrightText: 2026 LingmoOS Contributors
# SPDX-License-Identifier: GPL-3.0-or-later

find_package(GTest REQUIRED)
include(GoogleTest)

add_executable(calculator-core-tests
    core_test.cpp
)
target_link_libraries(calculator-core-tests PRIVATE calculator-core GTest::gtest_main)
gtest_discover_tests(calculator-core-tests)

add_executable(calculator-ui-tests
    ui_test.cpp
)
target_link_libraries(calculator-ui-tests PRIVATE calculator-ui GTest::gtest)
gtest_discover_tests(calculator-ui-tests DISCOVERY_TIMEOUT 30 PROPERTIES TIMEOUT 120)

add_executable(calculator-capture capture.cpp)
target_link_libraries(calculator-capture PRIVATE calculator-ui)
