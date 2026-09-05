/*
 * CelestialCalendar:
 *   A C++23-style library that performs astronomical calculations and date conversions among various calendars,
 *   including Gregorian, Lunar, and Chinese Ganzhi calendars.
 *
 * Copyright (C) 2026 Ningqi Wang (0xf3cd)
 * Email: nq.maigre@gmail.com
 * Repo : https://github.com/0xf3cd/celestial-calendar
 *
 * SPDX-License-Identifier: MIT
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

// Retained material boundary (V12): the USNO API output rows below remain under their source terms
// and outside the project MIT grant.
// End-to-end golden dataset for moonrise/moonset/lunar-transit (#62), collected 2026-08-15 by
// `statistics/moonrise_golden_crawler.py`:
// - Source: USNO rstt/oneday API, queried per site at tz=0, minute precision — each response
//   lists the events of one UT day, matching `moon::calculate`'s UT-day window 1:1. Five
//   consecutive-day spans per site across decades (1999 / 2024 / 2026 / 2028 / 2049), plus
//   Tromsø polar and double-event rows (below).
// - Empty cell = the event does not occur on that UT date. For the Moon this is routine
//   calendar arithmetic (transits ~24.84 h apart → one skipped moonrise every ~29.5 days),
//   NOT a polar verdict; the Tromso August rows also pin the true polar cases (2026 is
//   inside the major-standstill season, so the Moon is circumpolar / never-rising at 69.65°N
//   for several days each month — candidate dates located with the library's engine, then
//   pinned against USNO).
// - The 2026-05-14 / 2026-06-18 Tromsø rows are DOUBLE-EVENT days: USNO lists two events of
//   one kind in the same cell; this library reports one event per cell — the later one (see
//   `moon::calculate`'s note) — and the crawler's last-wins dict keeps it accordingly.
// - The complete 181-request current-endpoint recapture and response hashes are pinned in
//   `src/test/provenance/usno/2026-08-26/v12-rstt-oneday.json`. Its API 4.0.1 field is
//   current evidence, not an inferred version for the historical 2026-08-15 collection.
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
  if (not event_jde.has_value() and not golden.has_value()) {
    return; // Both agree the event does not occur.
  }
  if (event_jde.has_value() and golden.has_value()) {
    ASSERT_LE(clock_diff(jde_to_ut_minutes(*event_jde), *golden), TOL_MIN) << what;
    return;
  }
  // Exactly one side reports the event. USNO rounds cells to the minute and can carry an
  // event across a UT midnight (23:59:5x → "00:00" on the next date) while this library's
  // window is exact — a presence mismatch is only legitimate in that boundary band.
  const double minutes = event_jde.has_value() ? jde_to_ut_minutes(*event_jde) : *golden;
  ASSERT_TRUE(minutes >= (1440.0 - TOL_MIN) or minutes <= TOL_MIN)
    << what << " presence mismatch outside the midnight-rounding band";
}
// NOLINTEND(bugprone-easily-swappable-parameters)

struct MoonRow {
  int year;
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
  {  1999,  6, 14,   -0.22,   -78.51, "11:58", "18:13", "     " },  // Quito
  {  1999,  6, 15,   -0.22,   -78.51, "13:00", "19:14", "00:27" },  // Quito
  {  1999,  6, 16,   -0.22,   -78.51, "14:00", "20:13", "01:28" },  // Quito
  {  1999,  6, 17,   -0.22,   -78.51, "14:55", "21:08", "02:26" },  // Quito
  {  1999,  6, 18,   -0.22,   -78.51, "15:47", "21:59", "03:20" },  // Quito
  {  2024,  3, 19,   -0.22,   -78.51, "19:37", "00:58", "07:10" },  // Quito
  {  2024,  3, 20,   -0.22,   -78.51, "20:25", "01:49", "08:00" },  // Quito
  {  2024,  3, 21,   -0.22,   -78.51, "21:09", "02:36", "08:46" },  // Quito
  {  2024,  3, 22,   -0.22,   -78.51, "21:51", "03:19", "09:29" },  // Quito
  {  2024,  3, 23,   -0.22,   -78.51, "22:31", "04:00", "10:10" },  // Quito
  {  2026,  8, 13,   -0.22,   -78.51, "11:59", "18:11", "     " },  // Quito
  {  2026,  8, 14,   -0.22,   -78.51, "12:47", "18:58", "00:22" },  // Quito
  {  2026,  8, 15,   -0.22,   -78.51, "13:33", "19:44", "01:09" },  // Quito
  {  2026,  8, 16,   -0.22,   -78.51, "14:17", "20:28", "01:54" },  // Quito
  {  2026,  8, 17,   -0.22,   -78.51, "15:02", "21:13", "02:39" },  // Quito
  {  2028, 12, 15,   -0.22,   -78.51, "10:33", "16:47", "23:02" },  // Quito
  {  2028, 12, 16,   -0.22,   -78.51, "11:32", "17:46", "     " },  // Quito
  {  2028, 12, 17,   -0.22,   -78.51, "12:28", "18:41", "00:00" },  // Quito
  {  2028, 12, 18,   -0.22,   -78.51, "13:20", "19:32", "00:54" },  // Quito
  {  2028, 12, 19,   -0.22,   -78.51, "14:08", "20:20", "01:44" },  // Quito
  {  2049,  6, 14,   -0.22,   -78.51, "22:22", "03:36", "09:51" },  // Quito
  {  2049,  6, 15,   -0.22,   -78.51, "23:25", "04:38", "10:53" },  // Quito
  {  2049,  6, 16,   -0.22,   -78.51, "     ", "05:41", "11:56" },  // Quito
  {  2049,  6, 17,   -0.22,   -78.51, "00:28", "06:43", "12:58" },  // Quito
  {  2049,  6, 18,   -0.22,   -78.51, "01:29", "07:43", "13:57" },  // Quito
  {  1999,  6, 14,    1.35,   103.82, "     ", "05:32", "11:49" },  // Singapore
  {  1999,  6, 15,    1.35,   103.82, "00:17", "06:34", "12:51" },  // Singapore
  {  1999,  6, 16,    1.35,   103.82, "01:18", "07:34", "13:50" },  // Singapore
  {  1999,  6, 17,    1.35,   103.82, "02:16", "08:31", "14:46" },  // Singapore
  {  1999,  6, 18,    1.35,   103.82, "03:11", "09:24", "15:38" },  // Singapore
  {  2024,  3, 19,    1.35,   103.82, "06:59", "13:14", "19:29" },  // Singapore
  {  2024,  3, 20,    1.35,   103.82, "07:49", "14:03", "20:16" },  // Singapore
  {  2024,  3, 21,    1.35,   103.82, "08:36", "14:48", "21:00" },  // Singapore
  {  2024,  3, 22,    1.35,   103.82, "09:19", "15:30", "21:42" },  // Singapore
  {  2024,  3, 23,    1.35,   103.82, "10:00", "16:11", "22:21" },  // Singapore
  {  2026,  8, 13,    1.35,   103.82, "     ", "05:36", "11:49" },  // Singapore
  {  2026,  8, 14,    1.35,   103.82, "00:13", "06:25", "12:37" },  // Singapore
  {  2026,  8, 15,    1.35,   103.82, "01:01", "07:11", "13:22" },  // Singapore
  {  2026,  8, 16,    1.35,   103.82, "01:46", "07:56", "14:06" },  // Singapore
  {  2026,  8, 17,    1.35,   103.82, "02:31", "08:41", "14:50" },  // Singapore
  {  2028, 12, 15,    1.35,   103.82, "22:56", "04:08", "10:20" },  // Singapore
  {  2028, 12, 16,    1.35,   103.82, "23:53", "05:07", "11:18" },  // Singapore
  {  2028, 12, 17,    1.35,   103.82, "     ", "06:04", "12:15" },  // Singapore
  {  2028, 12, 18,    1.35,   103.82, "00:47", "06:58", "13:08" },  // Singapore
  {  2028, 12, 19,    1.35,   103.82, "01:37", "07:47", "13:57" },  // Singapore
  {  2049,  6, 14,    1.35,   103.82, "09:44", "15:57", "22:09" },  // Singapore
  {  2049,  6, 15,    1.35,   103.82, "10:47", "16:59", "23:12" },  // Singapore
  {  2049,  6, 16,    1.35,   103.82, "11:50", "18:03", "     " },  // Singapore
  {  2049,  6, 17,    1.35,   103.82, "12:51", "19:04", "00:15" },  // Singapore
  {  2049,  6, 18,    1.35,   103.82, "13:50", "20:02", "01:16" },  // Singapore
  {  1999,  6, 14,   51.50,    -0.13, "04:39", "12:46", "20:55" },  // London
  {  1999,  6, 15,   51.50,    -0.13, "05:39", "13:48", "21:52" },  // London
  {  1999,  6, 16,   51.50,    -0.13, "06:47", "14:47", "22:38" },  // London
  {  1999,  6, 17,   51.50,    -0.13, "07:59", "15:43", "23:15" },  // London
  {  1999,  6, 18,   51.50,    -0.13, "09:12", "16:35", "23:45" },  // London
  {  2024,  3, 19,   51.50,    -0.13, "11:36", "20:24", "04:28" },  // London
  {  2024,  3, 20,   51.50,    -0.13, "12:50", "21:12", "04:56" },  // London
  {  2024,  3, 21,   51.50,    -0.13, "14:04", "21:56", "05:15" },  // London
  {  2024,  3, 22,   51.50,    -0.13, "15:17", "22:38", "05:30" },  // London
  {  2024,  3, 23,   51.50,    -0.13, "16:28", "23:18", "05:42" },  // London
  {  2026,  8, 13,   51.50,    -0.13, "05:27", "12:46", "19:45" },  // London
  {  2026,  8, 14,   51.50,    -0.13, "06:52", "13:35", "19:58" },  // London
  {  2026,  8, 15,   51.50,    -0.13, "08:14", "14:20", "20:10" },  // London
  {  2026,  8, 16,   51.50,    -0.13, "09:33", "15:05", "20:22" },  // London
  {  2026,  8, 17,   51.50,    -0.13, "10:51", "15:50", "20:36" },  // London
  {  2028, 12, 15,   51.50,    -0.13, "07:38", "11:21", "15:03" },  // London
  {  2028, 12, 16,   51.50,    -0.13, "08:35", "12:20", "16:08" },  // London
  {  2028, 12, 17,   51.50,    -0.13, "09:17", "13:16", "17:20" },  // London
  {  2028, 12, 18,   51.50,    -0.13, "09:49", "14:08", "18:35" },  // London
  {  2028, 12, 19,   51.50,    -0.13, "10:13", "14:56", "19:50" },  // London
  {  2049,  6, 14,   51.50,    -0.13, "19:03", "23:10", "02:23" },  // London
  {  2049,  6, 15,   51.50,    -0.13, "20:06", "     ", "03:17" },  // London
  {  2049,  6, 16,   51.50,    -0.13, "20:57", "00:14", "04:24" },  // London
  {  2049,  6, 17,   51.50,    -0.13, "21:37", "01:16", "05:42" },  // London
  {  2049,  6, 18,   51.50,    -0.13, "22:09", "02:17", "07:05" },  // London
  {  1999,  6, 14,   39.90,   116.41, "22:12", "04:39", "12:08" },  // Beijing
  {  1999,  6, 15,   39.90,   116.41, "23:16", "05:42", "13:10" },  // Beijing
  {  1999,  6, 16,   39.90,   116.41, "     ", "06:42", "14:03" },  // Beijing
  {  1999,  6, 17,   39.90,   116.41, "00:23", "07:39", "14:49" },  // Beijing
  {  1999,  6, 18,   39.90,   116.41, "01:29", "08:32", "15:28" },  // Beijing
  {  2024,  3, 19,   39.90,   116.41, "04:27", "12:22", "20:09" },  // Beijing
  {  2024,  3, 20,   39.90,   116.41, "05:30", "13:11", "20:41" },  // Beijing
  {  2024,  3, 21,   39.90,   116.41, "06:33", "13:56", "21:08" },  // Beijing
  {  2024,  3, 22,   39.90,   116.41, "07:35", "14:39", "21:31" },  // Beijing
  {  2024,  3, 23,   39.90,   116.41, "08:36", "15:19", "21:51" },  // Beijing
  {  2026,  8, 13,   39.90,   116.41, "22:54", "04:44", "11:35" },  // Beijing
  {  2026,  8, 14,   39.90,   116.41, "     ", "05:33", "12:00" },  // Beijing
  {  2026,  8, 15,   39.90,   116.41, "00:04", "06:19", "12:23" },  // Beijing
  {  2026,  8, 16,   39.90,   116.41, "01:12", "07:04", "12:46" },  // Beijing
  {  2026,  8, 17,   39.90,   116.41, "02:18", "07:49", "13:10" },  // Beijing
  {  2028, 12, 15,   39.90,   116.41, "23:37", "03:15", "07:53" },  // Beijing
  {  2028, 12, 16,   39.90,   116.41, "     ", "04:15", "08:54" },  // Beijing
  {  2028, 12, 17,   39.90,   116.41, "00:29", "05:12", "09:59" },  // Beijing
  {  2028, 12, 18,   39.90,   116.41, "01:12", "06:05", "11:05" },  // Beijing
  {  2028, 12, 19,   39.90,   116.41, "01:47", "06:55", "12:10" },  // Beijing
  {  2049,  6, 14,   39.90,   116.41, "10:10", "15:04", "19:57" },  // Beijing
  {  2049,  6, 15,   39.90,   116.41, "11:15", "16:07", "21:00" },  // Beijing
  {  2049,  6, 16,   39.90,   116.41, "12:13", "17:10", "22:10" },  // Beijing
  {  2049,  6, 17,   39.90,   116.41, "13:04", "18:11", "23:24" },  // Beijing
  {  2049,  6, 18,   39.90,   116.41, "13:47", "19:09", "     " },  // Beijing
  {  1999,  6, 14,   40.71,   -74.01, "10:23", "17:54", "00:20" },  // NewYork
  {  1999,  6, 15,   40.71,   -74.01, "11:25", "18:56", "01:26" },  // NewYork
  {  1999,  6, 16,   40.71,   -74.01, "12:31", "19:54", "02:23" },  // NewYork
  {  1999,  6, 17,   40.71,   -74.01, "13:38", "20:49", "03:12" },  // NewYork
  {  1999,  6, 18,   40.71,   -74.01, "14:43", "21:40", "03:53" },  // NewYork
  {  2024,  3, 19,   40.71,   -74.01, "17:39", "00:39", "08:36" },  // NewYork
  {  2024,  3, 20,   40.71,   -74.01, "18:43", "01:30", "09:11" },  // NewYork
  {  2024,  3, 21,   40.71,   -74.01, "19:46", "02:17", "09:40" },  // NewYork
  {  2024,  3, 22,   40.71,   -74.01, "20:48", "03:01", "10:04" },  // NewYork
  {  2024,  3, 23,   40.71,   -74.01, "21:49", "03:42", "10:24" },  // NewYork
  {  2026,  8, 13,   40.71,   -74.01, "11:00", "17:52", "00:05" },  // NewYork
  {  2026,  8, 14,   40.71,   -74.01, "12:12", "18:40", "00:31" },  // NewYork
  {  2026,  8, 15,   40.71,   -74.01, "13:22", "19:25", "00:54" },  // NewYork
  {  2026,  8, 16,   40.71,   -74.01, "14:30", "20:10", "01:17" },  // NewYork
  {  2026,  8, 17,   40.71,   -74.01, "15:36", "20:54", "01:39" },  // NewYork
  {  2028, 12, 15,   40.71,   -74.01, "11:54", "16:29", "21:03" },  // NewYork
  {  2028, 12, 16,   40.71,   -74.01, "12:50", "17:27", "22:07" },  // NewYork
  {  2028, 12, 17,   40.71,   -74.01, "13:37", "18:22", "23:13" },  // NewYork
  {  2028, 12, 18,   40.71,   -74.01, "14:15", "19:14", "     " },  // NewYork
  {  2028, 12, 19,   40.71,   -74.01, "14:46", "20:01", "00:19" },  // NewYork
  {  2049,  6, 14,   40.71,   -74.01, "23:29", "03:17", "08:09" },  // NewYork
  {  2049,  6, 15,   40.71,   -74.01, "     ", "04:19", "09:08" },  // NewYork
  {  2049,  6, 16,   40.71,   -74.01, "00:31", "05:22", "10:16" },  // NewYork
  {  2049,  6, 17,   40.71,   -74.01, "01:25", "06:24", "11:29" },  // NewYork
  {  2049,  6, 18,   40.71,   -74.01, "02:11", "07:24", "12:44" },  // NewYork
  {  1999,  6, 14,  -33.87,   151.21, "22:02", "02:14", "07:30" },  // Sydney
  {  1999,  6, 15,  -33.87,   151.21, "23:00", "03:16", "08:32" },  // Sydney
  {  1999,  6, 16,  -33.87,   151.21, "23:52", "04:17", "09:36" },  // Sydney
  {  1999,  6, 17,  -33.87,   151.21, "     ", "05:14", "10:40" },  // Sydney
  {  1999,  6, 18,  -33.87,   151.21, "00:37", "06:08", "11:43" },  // Sydney
  {  2024,  3, 19,  -33.87,   151.21, "05:07", "09:58", "14:52" },  // Sydney
  {  2024,  3, 20,  -33.87,   151.21, "05:47", "10:47", "15:52" },  // Sydney
  {  2024,  3, 21,  -33.87,   151.21, "06:20", "11:33", "16:52" },  // Sydney
  {  2024,  3, 22,  -33.87,   151.21, "06:49", "12:15", "17:49" },  // Sydney
  {  2024,  3, 23,  -33.87,   151.21, "07:14", "12:56", "18:45" },  // Sydney
  {  2026,  8, 13,  -33.87,   151.21, "21:22", "02:20", "07:58" },  // Sydney
  {  2026,  8, 14,  -33.87,   151.21, "21:51", "03:09", "09:05" },  // Sydney
  {  2026,  8, 15,  -33.87,   151.21, "22:18", "03:56", "10:10" },  // Sydney
  {  2026,  8, 16,  -33.87,   151.21, "22:45", "04:41", "11:13" },  // Sydney
  {  2026,  8, 17,  -33.87,   151.21, "23:14", "05:25", "12:15" },  // Sydney
  {  2028, 12, 15,  -33.87,   151.21, "18:19", "00:50", "08:21" },  // Sydney
  {  2028, 12, 16,  -33.87,   151.21, "19:21", "01:50", "09:19" },  // Sydney
  {  2028, 12, 17,  -33.87,   151.21, "20:24", "02:47", "10:09" },  // Sydney
  {  2028, 12, 18,  -33.87,   151.21, "21:27", "03:41", "10:51" },  // Sydney
  {  2028, 12, 19,  -33.87,   151.21, "22:27", "04:31", "11:28" },  // Sydney
  {  2049,  6, 14,  -33.87,   151.21, "05:22", "12:39", "19:59" },  // Sydney
  {  2049,  6, 15,  -33.87,   151.21, "06:21", "13:42", "21:02" },  // Sydney
  {  2049,  6, 16,  -33.87,   151.21, "07:27", "14:45", "21:59" },  // Sydney
  {  2049,  6, 17,  -33.87,   151.21, "08:37", "15:46", "22:49" },  // Sydney
  {  2049,  6, 18,  -33.87,   151.21, "09:48", "16:45", "23:33" },  // Sydney
  {  1999,  6, 14,   69.65,    18.96, "00:13", "11:26", "23:13" },  // Tromso
  {  1999,  6, 15,   69.65,    18.96, "00:41", "12:28", "23:39" },  // Tromso
  {  1999,  6, 16,   69.65,    18.96, "02:23", "13:28", "23:42" },  // Tromso
  {  1999,  6, 17,   69.65,    18.96, "04:22", "14:24", "23:40" },  // Tromso
  {  1999,  6, 18,   69.65,    18.96, "06:15", "15:16", "23:37" },  // Tromso
  {  2024,  3, 19,   69.65,    18.96, "     ", "19:05", "     " },  // Tromso
  {  2024,  3, 20,   69.65,    18.96, "     ", "19:53", "     " },  // Tromso
  {  2024,  3, 21,   69.65,    18.96, "09:39", "20:38", "07:10" },  // Tromso
  {  2024,  3, 22,   69.65,    18.96, "12:16", "21:20", "06:03" },  // Tromso
  {  2024,  3, 23,   69.65,    18.96, "14:15", "21:59", "05:30" },  // Tromso
  {  2026,  8, 13,   69.65,    18.96, "02:31", "11:27", "19:23" },  // Tromso
  {  2026,  8, 14,   69.65,    18.96, "04:51", "12:16", "18:56" },  // Tromso
  {  2026,  8, 15,   69.65,    18.96, "06:57", "13:02", "18:32" },  // Tromso
  {  2026,  8, 16,   69.65,    18.96, "08:59", "13:46", "18:05" },  // Tromso
  {  2026,  8, 17,   69.65,    18.96, "11:07", "14:31", "17:32" },  // Tromso
  {  2028, 12, 15,   69.65,    18.96, "     ", "     ", "     " },  // Tromso
  {  2028, 12, 16,   69.65,    18.96, "     ", "     ", "     " },  // Tromso
  {  2028, 12, 17,   69.65,    18.96, "     ", "     ", "     " },  // Tromso
  {  2028, 12, 18,   69.65,    18.96, "11:53", "12:49", "14:00" },  // Tromso
  {  2028, 12, 19,   69.65,    18.96, "10:56", "13:37", "16:38" },  // Tromso
  {  2049,  6, 14,   69.65,    18.96, "     ", "     ", "     " },  // Tromso
  {  2049,  6, 15,   69.65,    18.96, "     ", "     ", "     " },  // Tromso
  {  2049,  6, 16,   69.65,    18.96, "23:20", "23:57", "     " },  // Tromso
  {  2049,  6, 17,   69.65,    18.96, "22:35", "     ", "00:45" },  // Tromso
  {  2049,  6, 18,   69.65,    18.96, "22:18", "00:57", "03:36" },  // Tromso
  {  2026,  8,  8,   69.65,    18.96, "     ", "06:28", "     " },  // Tromso polar day (Moon circumpolar: transit only)
  {  2026,  8,  9,   69.65,    18.96, "     ", "07:32", "     " },  // Tromso polar day (transit only)
  {  2026,  8, 20,   69.65,    18.96, "     ", "     ", "     " },  // Tromso polar night (never rises: all blank)
  {  2026,  8, 21,   69.65,    18.96, "     ", "     ", "     " },  // Tromso polar night (all blank)
  {  2026,  5, 14,   69.65,    18.96, "23:56", "08:20", "17:09" },  // Tromso double-RISE day: USNO lists rises 00:31 AND 23:56 in this
  // cell; this library reports the later one (see header).
  {  2026,  6, 18,   69.65,    18.96, "02:42", "14:11", "23:38" },  // Tromso double-SET day: USNO lists sets 01:07 AND 23:38;
  // the window end dips below the interior minimum — the shape that once hid
  // this day's only rise inside a non-monotone segment.
};
// NOLINTEND(modernize-use-designated-initializers)

}  // namespace


TEST(RiseSetMoonGolden, UsnoRiseTransitSet) {
  for (const auto& row : USNO_ROWS) {
    const auto ymd = util::to_ymd(row.year, row.month, row.day);
    const Result result = moon::calculate(ymd, loc(row.lat, row.lon));
    const auto tag = std::to_string(row.year) + "-" + std::to_string(row.month) + "-"
                   + std::to_string(row.day)
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
