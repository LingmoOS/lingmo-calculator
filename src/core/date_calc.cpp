// SPDX-FileCopyrightText: 2026 LingmoOS Contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/date_calc.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <ctime>

namespace calculator {

namespace {

bool isLeap(int year)
{
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

int daysInMonth(int year, int month)
{
    static const int kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12)
        return 0;
    if (month == 2 && isLeap(year))
        return 29;
    return kDays[month - 1];
}

// Days from epoch (1970-01-01) for a Gregorian date; negative before it.
int64_t toEpochDays(const CalendarDate& d)
{
    // Algorithm from "Calendrical Calculations"; works for any valid date.
    int y = d.year - (d.month <= 2);
    const int era = (y >= 0 ? y : y - 399) / 400;
    const uint64_t yoe = uint64_t(y - era * 400);               // [0, 399]
    const uint64_t doy = uint64_t((153 * (d.month + (d.month > 2 ? -3 : 9)) + 2) / 5 + d.day - 1);
    const uint64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;  // [0, 146096]
    return int64_t(era) * 146097 + int64_t(doe) - 719468;
}

CalendarDate fromEpochDays(int64_t epoch)
{
    const int64_t z = epoch + 719468;
    const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const uint64_t doe = uint64_t(z - era * 146097);             // [0, 146096]
    const uint64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365; // [0, 399]
    const int64_t y = int64_t(yoe) + era * 400;
    const uint64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100); // [0, 365]
    const uint64_t mp = (5 * doy + 2) / 153;                     // [0, 11]
    const int day = int(doy - (153 * mp + 2) / 5 + 1);
    const int month = int(mp < 10 ? mp + 3 : mp - 9);
    const int year = int(y + (month <= 2));
    return {year, month, day};
}

int weekdayOf(const CalendarDate& d)
{
    // 1970-01-01 was a Thursday (4).
    const int64_t days = toEpochDays(d);
    const int wd = int(((days % 7) + 7 + 4) % 7);
    return wd; // 0=Sunday
}

} // namespace

bool CalendarDate::valid() const
{
    if (year < 1 || year > 9999)
        return false;
    if (month < 1 || month > 12)
        return false;
    if (day < 1 || day > daysInMonth(year, month))
        return false;
    return true;
}

bool CalendarDate::operator<(const CalendarDate& o) const
{
    if (year != o.year) return year < o.year;
    if (month != o.month) return month < o.month;
    return day < o.day;
}

DateDiffResult DateCalculator::difference(const CalendarDate& from, const CalendarDate& to) const
{
    DateDiffResult r;
    if (!from.valid() || !to.valid()) {
        r.error = "Invalid date";
        return r;
    }
    r.ok = true;
    r.totalDays = int(toEpochDays(to) - toEpochDays(from));

    // Year/month/day breakdown, counting from the earlier date.
    CalendarDate start = from;
    CalendarDate end = to;
    bool negative = false;
    if (end < start) {
        std::swap(start, end);
        negative = true;
    }

    int years = end.year - start.year;
    int months = end.month - start.month;
    int days = end.day - start.day;
    if (days < 0) {
        --months;
        const CalendarDate prev{end.year, end.month, 1};
        days += daysInMonth(prev.year, prev.month == 1 ? 12 : prev.month - 1);
    }
    if (months < 0) {
        --years;
        months += 12;
    }
    r.years = years;
    r.months = months;
    r.days = days;
    if (negative)
        r.totalDays = -r.totalDays;
    r.weekday = weekdayOf(to);
    return r;
}

DateAddResult DateCalculator::addDays(const CalendarDate& date, int days) const
{
    DateAddResult r;
    if (!date.valid()) {
        r.error = "Invalid date";
        return r;
    }
    r.ok = true;
    r.result = fromEpochDays(toEpochDays(date) + days);
    r.formatted = formatDate(r.result) + " (" + weekdayName(weekdayOf(r.result)) + ")";
    return r;
}

bool DateCalculator::parseDate(const std::string& text, CalendarDate& out)
{
    int y = 0, m = 0, d = 0;
    if (std::sscanf(text.c_str(), "%d-%d-%d", &y, &m, &d) == 3
        || std::sscanf(text.c_str(), "%d/%d/%d", &y, &m, &d) == 3) {
        out = {y, m, d};
        return out.valid();
    }
    return false;
}

std::string DateCalculator::formatDate(const CalendarDate& date)
{
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", date.year, date.month, date.day);
    return buf;
}

std::string DateCalculator::weekdayName(int weekday)
{
    static const char* kNames[] = {
        "Sunday", "Monday", "Tuesday", "Wednesday",
        "Thursday", "Friday", "Saturday"
    };
    if (weekday < 0 || weekday > 6)
        return {};
    return kNames[weekday];
}

CalendarDate DateCalculator::today()
{
    std::time_t now = std::time(nullptr);
    std::tm local{};
    localtime_r(&now, &local);
    return {local.tm_year + 1900, local.tm_mon + 1, local.tm_mday};
}

} // namespace calculator
