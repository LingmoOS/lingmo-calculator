// SPDX-FileCopyrightText: 2026 LingmoOS Contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <string>

namespace calculator {

struct CalendarDate
{
    int year = 1970;
    int month = 1; // 1-12
    int day = 1;   // 1-31

    bool valid() const;
    bool operator==(const CalendarDate&) const = default;
    bool operator<(const CalendarDate& o) const;
};

struct DateDiffResult
{
    bool ok = false;
    std::string error;
    int totalDays = 0;  // signed: negative when from > to
    int years = 0;
    int months = 0;
    int days = 0;       // remainder after years and months
    int weekday = 0;    // 0=Sunday of `to`
};

struct DateAddResult
{
    bool ok = false;
    std::string error;
    CalendarDate result;
    std::string formatted; // "YYYY-MM-DD (Weekday)"
};

class DateCalculator
{
public:
    // Days between two calendar dates; signed (negative when from > to).
    DateDiffResult difference(const CalendarDate& from, const CalendarDate& to) const;

    // Add/subtract a number of days to a date.
    DateAddResult addDays(const CalendarDate& date, int days) const;

    // Parse "YYYY-MM-DD" or "YYYY/MM/DD".
    static bool parseDate(const std::string& text, CalendarDate& out);
    // Format as "YYYY-MM-DD".
    static std::string formatDate(const CalendarDate& date);
    // The weekday name (Sunday .. Saturday).
    static std::string weekdayName(int weekday);
    // Today's date.
    static CalendarDate today();
};

} // namespace calculator
