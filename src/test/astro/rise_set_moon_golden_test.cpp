/*
 * CelestialCalendar:
 *   A C++23-style library that performs astronomical calculations and date conversions among various calendars,
 *   including Gregorian, Lunar, and Chinese Ganzhi calendars.
 *
 * Copyright (C) 2026 Ningqi Wang (0xf3cd)
 * Email: nq.maigre@gmail.com
 * Repo : https://github.com/0xf3cd/celestial-calendar
 *
 * This project is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This project is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this project. If not, see <https://www.gnu.org/licenses/>.
 */

#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "util.hpp"
#include "rise_set.hpp"
#include "rise_set_test_helper.hpp"

// End-to-end golden dataset for moonrise/moonset/lunar-transit (#62), collected 2026-08-14 by
// `statistics/moonrise_golden_crawler.py`:
// - Source: USNO rstt/oneday API, queried per site at tz=0, minute precision — each response
//   lists the events of one UT day, matching `moon::calculate`'s UT-day window 1:1.
// - Empty cell = the event does not occur on that UT date. For the Moon this is routine
//   calendar arithmetic (transits ~24.84 h apart → one skipped moonrise every ~29.5 days;
//   the August span catches skipped events at 3 of 7 sites — Singapore/Beijing rise, Quito
//   set), NOT a polar verdict; the Tromsø August rows also pin the true polar cases (2026 is
//   inside the major-standstill season, so the Moon is circumpolar / never-rising at 69.65°N
//   for several days each month — candidate dates located with the library's engine, then
//   pinned against USNO).
// - The 2026-05-14 Tromsø row is a DOUBLE-RISE day: USNO lists two rises in the same cell
//   (00:31 and 23:56); this library reports one event per cell — the later one (see
//   `moon::calculate`'s note) — and the crawler's last-wins dict keeps 23:56 accordingly.
// - Polar direction inference: a transit-only row means the Moon stayed above the horizon all
//   day (USNO lists the upper culmination) → DAY; an all-blank row means it never rose → NIGHT.
// Tolerance: ±2 min, the same contract as the solar golden set (#44) — USNO cells are
// minute-rounded (±0.5 min quantization).

namespace astro::rise_set::test {

using namespace astro::rise_set;
using astro::toolbox::AngleDeg;

namespace {

constexpr double TOL_MIN = 2.0;

/** @brief Our JDE(TT) result as minutes-of-day on the UT clock. */
auto jde_to_ut_minutes(const double jde) -> double {
  const calendar::Datetime ut1 = astro::julian_day::jde_to_ut1(jde);
  return ut1.fraction() * 1440.0;
}

// `cell` and `what` are two adjacent string_views by design (golden cell first, tag last).
// NOLINTBEGIN(bugprone-easily-swappable-parameters)
void expect_event(
  const std::optional<double>& event_jde, const std::string_view cell,
  const std::string_view what
) {
  const auto golden = cell_minutes(cell);
  ASSERT_EQ(event_jde.has_value(), golden.has_value()) << what;
  // Explicit guard (not just the ASSERT above): clang-tidy's unchecked-optional-access
  // cannot see through gtest macros (#110's lesson).
  if (not event_jde.has_value() or not golden.has_value()) {
    return;
  }
  ASSERT_LE(clock_diff(jde_to_ut_minutes(*event_jde), *golden), TOL_MIN) << what;
}
// NOLINTEND(bugprone-easily-swappable-parameters)

struct MoonRow {
  int month;
  int day;
  double lat;
  double lon;
  std::string_view rise;
  std::string_view transit;
  std::string_view set;
};

// USNO rows, UT clock (tz=0). See the file header for provenance.
// NOLINTBEGIN(modernize-use-designated-initializers)
const std::vector<MoonRow> USNO_ROWS {
  {  8, 13,   -0.22,   -78.51, "11:59", "18:11", "     " },  // Quito
  {  8, 14,   -0.22,   -78.51, "12:47", "18:58", "00:22" },  // Quito
  {  8, 15,   -0.22,   -78.51, "13:33", "19:44", "01:09" },  // Quito
  {  8, 16,   -0.22,   -78.51, "14:17", "20:28", "01:54" },  // Quito
  {  8, 17,   -0.22,   -78.51, "15:02", "21:13", "02:39" },  // Quito
  {  8, 13,    1.35,   103.82, "     ", "05:36", "11:49" },  // Singapore
  {  8, 14,    1.35,   103.82, "00:13", "06:25", "12:37" },  // Singapore
  {  8, 15,    1.35,   103.82, "01:01", "07:11", "13:22" },  // Singapore
  {  8, 16,    1.35,   103.82, "01:46", "07:56", "14:06" },  // Singapore
  {  8, 17,    1.35,   103.82, "02:31", "08:41", "14:50" },  // Singapore
  {  8, 13,   51.50,   -0.13, "05:27", "12:46", "19:45" },  // London
  {  8, 14,   51.50,   -0.13, "06:52", "13:35", "19:58" },  // London
  {  8, 15,   51.50,   -0.13, "08:14", "14:20", "20:10" },  // London
  {  8, 16,   51.50,   -0.13, "09:33", "15:05", "20:22" },  // London
  {  8, 17,   51.50,   -0.13, "10:51", "15:50", "20:36" },  // London
  {  8, 13,   39.90,   116.41, "22:54", "04:44", "11:35" },  // Beijing
  {  8, 14,   39.90,   116.41, "     ", "05:33", "12:00" },  // Beijing
  {  8, 15,   39.90,   116.41, "00:04", "06:19", "12:23" },  // Beijing
  {  8, 16,   39.90,   116.41, "01:12", "07:04", "12:46" },  // Beijing
  {  8, 17,   39.90,   116.41, "02:18", "07:49", "13:10" },  // Beijing
  {  8, 13,   40.71,   -74.01, "11:00", "17:52", "00:05" },  // NewYork
  {  8, 14,   40.71,   -74.01, "12:12", "18:40", "00:31" },  // NewYork
  {  8, 15,   40.71,   -74.01, "13:22", "19:25", "00:54" },  // NewYork
  {  8, 16,   40.71,   -74.01, "14:30", "20:10", "01:17" },  // NewYork
  {  8, 17,   40.71,   -74.01, "15:36", "20:54", "01:39" },  // NewYork
  {  8, 13,  -33.87,   151.21, "21:22", "02:20", "07:58" },  // Sydney
  {  8, 14,  -33.87,   151.21, "21:51", "03:09", "09:05" },  // Sydney
  {  8, 15,  -33.87,   151.21, "22:18", "03:56", "10:10" },  // Sydney
  {  8, 16,  -33.87,   151.21, "22:45", "04:41", "11:13" },  // Sydney
  {  8, 17,  -33.87,   151.21, "23:14", "05:25", "12:15" },  // Sydney
  {  8, 13,   69.65,    18.96, "02:31", "11:27", "19:23" },  // Tromso
  {  8, 14,   69.65,    18.96, "04:51", "12:16", "18:56" },  // Tromso
  {  8, 15,   69.65,    18.96, "06:57", "13:02", "18:32" },  // Tromso
  {  8, 16,   69.65,    18.96, "08:59", "13:46", "18:05" },  // Tromso
  {  8, 17,   69.65,    18.96, "11:07", "14:31", "17:32" },  // Tromso
  {  8,  8,   69.65,    18.96, "     ", "06:28", "     " },  // Tromso polar day
  {  8,  9,   69.65,    18.96, "     ", "07:32", "     " },  // Tromso polar day
  {  8, 20,   69.65,    18.96, "     ", "     ", "     " },  // Tromso polar night
  {  8, 21,   69.65,    18.96, "     ", "     ", "     " },  // Tromso polar night
  {  5, 14,   69.65,    18.96, "23:56", "08:20", "17:09" },  // Tromso double-rise day: USNO
  // lists rises 00:31 AND 23:56 in this cell; this library reports the later one (see header).
  {  6, 18,   69.65,    18.96, "02:42", "14:11", "23:38" },  // Tromso double-SET day: USNO
  // lists sets 01:07 AND 23:38; the window end dips below the interior minimum — the shape
  // that once hid this day's only rise inside a non-monotone segment.
};
// NOLINTEND(modernize-use-designated-initializers)

}  // namespace


TEST(RiseSetMoonGolden, UsnoRiseTransitSet) {
  for (const auto& row : USNO_ROWS) {
    const auto ymd = util::to_ymd(2026, row.month, row.day);
    const Result result = moon::calculate(ymd, loc(row.lat, row.lon));
    const auto tag = std::to_string(row.month) + "-" + std::to_string(row.day)
                   + " @ " + std::to_string(row.lat);

    expect_event(result.rise_jde, row.rise, tag + " rise");
    expect_event(result.set_jde, row.set, tag + " set");

    const bool has_rise = cell_minutes(row.rise).has_value();
    const bool has_transit = cell_minutes(row.transit).has_value();
    const bool has_set = cell_minutes(row.set).has_value();
    const bool no_events = not has_rise and not has_transit and not has_set;

    // USNO omits the transit on never-rising days; ours always reports the (real, sub-horizon)
    // meridian crossing, like the solar API does. On those rows the transit cell has no golden
    // value and is skipped — same precedent as the solar polar-night row (#44).
    if (not no_events) {
      expect_event(result.transit_jde, row.transit, tag + " transit");
    }

    // Topology follows from the cells (see the file header for the direction inference):
    // all-blank ⇒ NIGHT; transit-only ⇒ DAY; anything else ⇒ NONE (a lone missing rise/set
    // is the Moon's skipped-day calendar arithmetic, not a polar verdict).
    Polar expected = Polar::NONE;
    if (no_events) {
      expected = Polar::NIGHT;
    } else if (not has_rise and has_transit and not has_set) {
      expected = Polar::DAY;
    }
    ASSERT_EQ(result.polar, expected) << tag;

    // Pin the day axis: every event the engine emits must land inside the queried UT date —
    // `clock_diff` above is day-blind, and the whole point of the UT-day window is that the
    // cell attribution matches the almanac's.
    for (const auto& event : { result.rise_jde, result.transit_jde, result.set_jde }) {
      if (event.has_value()) {
        ASSERT_EQ(astro::julian_day::jde_to_ut1(*event).ymd, ymd) << tag << " event date";
      }
    }
  }
}

}  // namespace astro::rise_set::test
