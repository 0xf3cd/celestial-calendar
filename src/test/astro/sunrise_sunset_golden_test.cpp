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

#include <cmath>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "util.hpp"
#include "sunrise_sunset.hpp"

// End-to-end golden dataset for sunrise/sunset/twilight (#44), collected 2026-07-27 by
// `statistics/sunrise_golden_crawler.py`:
// - Rise / upper transit / set / civil twilight: USNO rstt/oneday API (apiversion 4.0.1),
//   queried per site at the fixed standard-time offset in the `tz` column (no DST), minute
//   precision. Cross-checked against NOAA solcalc `table.php` (independent implementation,
//   DST unwound via IANA zones): worst disagreement 1.0 min over all 42 shared values.
// - Nautical/astronomical twilight: Skyfield 1.54 + JPL DE421, second precision. Skyfield was
//   validated against USNO on every shared quantity of the full matrix first (worst 0.52 min),
//   and its −12°/−18° rows agree with sunrise-sunset.org to ≤ 0.2 min. (That site's own
//   rise/set column is a ~2–3 min outlier vs USNO+NOAA and was rejected as a source.)
// - Empty cell = the source states the event does not occur that day (polar day/night, or the
//   Sun never reaching −18° at London's June solstice). USNO omits the transit time during
//   polar night, so that single cell has no golden value and is skipped.
// Tolerance: ±2 min is the v0.3.0 accuracy contract (#44). Sources are minute-rounded
// (±0.5 min quantization) and agree pairwise within 1 min; this library's measured worst
// residual over all 143 golden time values is 0.55 min (2026-07-27), a 3.6× margin.
// A TT/UT1 mixup (~69 s) is inside this tolerance by design — that axis is pinned by the
// Meeus Example 28.a anchor (±20 s) in sunrise_sunset_test.cpp, not here.

namespace astro::sunrise_sunset::test {

using namespace astro::sunrise_sunset;
using astro::toolbox::Angle;
using astro::toolbox::AngleUnit::DEG;

namespace {

constexpr double TOL_MIN = 2.0;

constexpr auto loc(const double lat_deg, const double lon_deg) -> GeoLocation {
  return { .latitude = Angle<DEG> { lat_deg }, .longitude = Angle<DEG> { lon_deg } };
}

/** @brief Parse "HH:MM" / "HH:MM:SS" into minutes-of-day; blank cell → `nullopt`. */
auto cell_minutes(const std::string_view cell) -> std::optional<double> {
  const auto first = cell.find_first_not_of(' ');
  if (first == std::string_view::npos) {
    return std::nullopt;
  }
  if (cell.size() - first < 5 or cell[first + 2] != ':') {
    throw std::invalid_argument { "malformed golden cell: " + std::string { cell } };
  }
  const auto digits = [&](const size_t pos) { return 10.0 * (cell[pos] - '0') + (cell[pos + 1] - '0'); };
  double minutes = 60.0 * digits(first) + digits(first + 3);
  const auto second_colon = cell.find(':', first + 3);
  if (second_colon != std::string_view::npos) {
    if (second_colon != first + 5 or cell.size() - first < 8) {
      throw std::invalid_argument { "malformed golden cell: " + std::string { cell } };
    }
    minutes += digits(first + 6) / 60.0;
  }
  return minutes;
}

// NOLINTBEGIN(bugprone-easily-swappable-parameters): tiny comparison helpers.

/** @brief Our JDE(TT) result as minutes-of-day on the site's standard-time clock. */
auto jde_to_local_minutes(const double jde, const int tz_hours) -> double {
  const calendar::Datetime ut1 = astro::julian_day::jde_to_ut1(jde);
  return std::fmod(ut1.fraction() * 1440.0 + (tz_hours * 60.0) + 1440.0, 1440.0);
}

/** @brief |a − b| in minutes on the 24h circle, so a source's day and ours may differ. */
auto clock_diff(const double a, const double b) -> double {
  const double diff = std::fabs(a - b);
  return std::min(diff, 1440.0 - diff);
}

// NOLINTEND(bugprone-easily-swappable-parameters)

void expect_event(
  const std::optional<double>& event_jde, const std::string_view cell,
  const int tz, const std::string_view what
) {
  const auto golden = cell_minutes(cell);
  ASSERT_EQ(event_jde.has_value(), golden.has_value()) << what;
  if (golden.has_value()) {
    ASSERT_LE(clock_diff(jde_to_local_minutes(*event_jde, tz), *golden), TOL_MIN) << what;
  }
}

struct GoldenRow {
  int month;
  int day;
  double lat;
  double lon;
  int tz;                       // fixed standard-time offset, hours east of UT
  std::string_view civil_dawn;  // "Begin Civil Twilight"
  std::string_view rise;
  std::string_view transit;    // "Upper Transit"
  std::string_view set;
  std::string_view civil_dusk;  // "End Civil Twilight"
  bool polar_day;
  bool polar_night;
};

// USNO rows, local standard time (see file header for provenance).
const std::vector<GoldenRow> USNO_ROWS {
  {  3, 20,   -0.22,   -78.51,  -5, "05:57", "06:18", "12:21", "18:25", "18:45", false, false },  // Quito
  {  6, 21,   -0.22,   -78.51,  -5, "05:50", "06:13", "12:16", "18:19", "18:42", false, false },  // Quito
  {  9, 23,   -0.22,   -78.51,  -5, "05:42", "06:03", "12:06", "18:10", "18:30", false, false },  // Quito
  { 12, 21,   -0.22,   -78.51,  -5, "05:46", "06:08", "12:12", "18:16", "18:39", false, false },  // Quito
  {  3, 20,    1.35,   103.82,   8, "06:48", "07:09", "13:12", "19:15", "19:36", false, false },  // Singapore
  {  6, 21,    1.35,   103.82,   8, "06:38", "07:00", "13:06", "19:12", "19:35", false, false },  // Singapore
  {  9, 23,    1.35,   103.82,   8, "06:33", "06:54", "12:57", "19:00", "19:21", false, false },  // Singapore
  { 12, 21,    1.35,   103.82,   8, "06:39", "07:01", "13:03", "19:04", "19:27", false, false },  // Singapore
  {  3, 20,   51.50,    -0.13,   0, "05:30", "06:03", "12:08", "18:13", "18:47", false, false },  // London
  {  6, 21,   51.50,    -0.13,   0, "02:55", "03:43", "12:02", "20:21", "21:09", false, false },  // London
  {  9, 23,   51.50,    -0.13,   0, "05:15", "05:48", "11:53", "17:57", "18:30", false, false },  // London
  { 12, 21,   51.50,    -0.13,   0, "07:23", "08:04", "11:59", "15:53", "16:34", false, false },  // London
  {  3, 20,   39.90,   116.41,   8, "05:52", "06:19", "12:22", "18:26", "18:53", false, false },  // Beijing
  {  6, 21,   39.90,   116.41,   8, "04:13", "04:46", "12:16", "19:46", "20:19", false, false },  // Beijing
  {  9, 23,   39.90,   116.41,   8, "05:36", "06:02", "12:07", "18:11", "18:37", false, false },  // Beijing
  { 12, 21,   39.90,   116.41,   8, "07:02", "07:32", "12:12", "16:52", "17:23", false, false },  // Beijing
  {  3, 20,   40.71,   -74.01,  -5, "05:32", "05:59", "12:03", "18:08", "18:36", false, false },  // NewYork
  {  6, 21,   40.71,   -74.01,  -5, "03:52", "04:25", "11:58", "19:31", "20:04", false, false },  // NewYork
  {  9, 23,   40.71,   -74.01,  -5, "05:17", "05:45", "11:48", "17:51", "18:19", false, false },  // NewYork
  { 12, 21,   40.71,   -74.01,  -5, "06:46", "07:17", "11:54", "16:32", "17:03", false, false },  // NewYork
  {  3, 20,  -33.87,   151.21,  10, "05:33", "05:58", "12:03", "18:07", "18:32", false, false },  // Sydney
  {  6, 21,  -33.87,   151.21,  10, "06:32", "07:00", "11:57", "16:54", "17:22", false, false },  // Sydney
  {  9, 23,  -33.87,   151.21,  10, "05:19", "05:44", "11:48", "17:52", "18:17", false, false },  // Sydney
  { 12, 21,  -33.87,   151.21,  10, "04:11", "04:41", "11:53", "19:05", "19:35", false, false },  // Sydney
  {  3, 20,   69.65,    18.96,   1, "04:44", "05:44", "11:52", "18:01", "19:02", false, false },  // Tromso
  {  6, 21,   69.65,    18.96,   1, "     ", "     ", "11:46", "     ", "     ", true , false },  // Tromso
  {  9, 23,   69.65,    18.96,   1, "04:27", "05:28", "11:37", "17:43", "18:43", false, false },  // Tromso
  { 12, 21,   69.65,    18.96,   1, "09:31", "     ", "     ", "     ", "13:53", false, true  },  // Tromso
};

struct TwilightRow {
  int month;
  int day;
  double lat;
  double lon;
  int tz;
  std::string_view nautical_dawn;
  std::string_view nautical_dusk;
  std::string_view astronomical_dawn;
  std::string_view astronomical_dusk;
};

// Skyfield rows, local standard time (see file header for provenance).
const std::vector<TwilightRow> TWILIGHT_ROWS {
  {  6, 21,   51.50,    -0.13,   0, "01:40:47", "22:23:51", "        ", "        " },  // London
  { 12, 21,   51.50,    -0.13,   0, "06:40:11", "17:16:58", "05:59:22", "17:57:46" },  // London
  { 12, 21,   69.65,    18.96,   1, "07:46:42", "15:37:40", "06:28:18", "16:56:03" },  // Tromso
};

}  // namespace


TEST(SunriseSunsetGolden, UsnoRiseTransitSet) {
  for (const auto& row : USNO_ROWS) {
    const auto ymd = util::to_ymd(2026, row.month, row.day);
    const Result result = calculate(ymd, loc(row.lat, row.lon));
    const auto tag = std::to_string(row.month) + "-" + std::to_string(row.day)
                   + " @ " + std::to_string(row.lat);

    ASSERT_EQ(result.is_polar_day, row.polar_day) << tag;
    ASSERT_EQ(result.is_polar_night, row.polar_night) << tag;
    expect_event(result.sunrise_jde, row.rise, row.tz, tag + " rise");
    expect_event(result.sunset_jde, row.set, row.tz, tag + " set");
    if (cell_minutes(row.transit).has_value()) {
      expect_event(result.transit_jde, row.transit, row.tz, tag + " transit");
    }

    // Pin the day axis: `clock_diff` alone is day-blind, and consecutive-day transits differ
    // only by the equation-of-time drift (≪ tolerance). The transit is ~local noon, so its
    // local-standard date must be the queried date on every row.
    const double jd_local = detail::jde_tt_to_jd_ut1(result.transit_jde) + row.tz / 24.0;
    ASSERT_EQ(astro::julian_day::jd_to_ut1(jd_local).ymd, ymd) << tag << " transit date";
  }
}

TEST(SunriseSunsetGolden, UsnoCivilTwilight) {
  for (const auto& row : USNO_ROWS) {
    const auto ymd = util::to_ymd(2026, row.month, row.day);
    const Result result = calculate(ymd, loc(row.lat, row.lon), CIVIL_TWILIGHT);
    const auto tag = std::to_string(row.month) + "-" + std::to_string(row.day)
                   + " @ " + std::to_string(row.lat) + " civil";

    // Expected −6° flags follow from the cells, not the horizon flags: no civil crossings
    // means the Sun stayed on one side of −6° all day — below it only if the day is already
    // a polar night at the horizon, above it otherwise (midnight sun like Tromsø June, or a
    // future "white night" row whose sun sets but never reaches −6°). A polar night at the
    // horizon with crossings (Tromsø December) is no polar anything at −6°.
    const bool has_civil = cell_minutes(row.civil_dawn).has_value()
                        or cell_minutes(row.civil_dusk).has_value();
    ASSERT_EQ(result.is_polar_day, not has_civil and not row.polar_night) << tag;
    ASSERT_EQ(result.is_polar_night, not has_civil and row.polar_night) << tag;
    expect_event(result.sunrise_jde, row.civil_dawn, row.tz, tag + " dawn");
    expect_event(result.sunset_jde, row.civil_dusk, row.tz, tag + " dusk");
  }
}

TEST(SunriseSunsetGolden, SkyfieldDeepTwilights) {
  for (const auto& row : TWILIGHT_ROWS) {
    const auto ymd = util::to_ymd(2026, row.month, row.day);
    const auto tag = std::to_string(row.month) + "-" + std::to_string(row.day)
                   + " @ " + std::to_string(row.lat);

    const Result nautical = calculate(ymd, loc(row.lat, row.lon), NAUTICAL_TWILIGHT);
    expect_event(nautical.sunrise_jde, row.nautical_dawn, row.tz, tag + " nautical dawn");
    expect_event(nautical.sunset_jde, row.nautical_dusk, row.tz, tag + " nautical dusk");

    const Result astronomical = calculate(ymd, loc(row.lat, row.lon), ASTRONOMICAL_TWILIGHT);
    expect_event(astronomical.sunrise_jde, row.astronomical_dawn, row.tz, tag + " astronomical dawn");
    expect_event(astronomical.sunset_jde, row.astronomical_dusk, row.tz, tag + " astronomical dusk");
    // London's June solstice Sun never reaches −18°: "polar day" at that altitude. All rows
    // here keep the transit-time Sun above −18°, so empty cells always mean polar day; a
    // future polar-night-at-−18° row would need a flag column like `GoldenRow`'s.
    ASSERT_EQ(astronomical.is_polar_day, not cell_minutes(row.astronomical_dawn).has_value()) << tag;
    ASSERT_FALSE(astronomical.is_polar_night) << tag;
  }
}

}  // namespace astro::sunrise_sunset::test
