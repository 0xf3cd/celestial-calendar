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

#include <gtest/gtest.h>

#include <tuple>
#include <chrono>
#include <cstdlib>
#include <random>
#include <vector>

#include "util.hpp"
#include "ymd.hpp"
#include "datetime.hpp"
#include "leap_second.hpp"
#include "julian_day.hpp"

namespace astro::leap_second::test {

using namespace astro::leap_second;
using namespace util;
using namespace util::ymd_operator;
using namespace std::chrono_literals;
using calendar::Datetime;


/** @brief Signed difference `a - b`, in nanoseconds. */
inline auto diff_ns(
  const Datetime& a,
  const Datetime& b // NOLINT(bugprone-easily-swappable-parameters) the sign convention is the point
) -> int64_t {
  const auto day_gap = std::chrono::sys_days { a.ymd } - std::chrono::sys_days { b.ymd };
  const auto time_gap = a.time_of_day.to_duration() - b.time_of_day.to_duration();
  return std::chrono::duration_cast<std::chrono::nanoseconds>(day_gap + time_gap).count();
}


TEST(LeapSecond, TableMatchesIERS) {
  // Transcription guard for `LEAP_SECOND_TABLE` (#65).
  // Columns: year, month (day is always 1), ΔAT = TAI − UTC in seconds.
  // Source: IERS Bulletin C, dumped via pyerfa 2.0.1 `erfa.dat` over 1972-2026 (2026-07-28).
  struct Row { int32_t y; uint32_t m; double dat; };
  // NOLINTBEGIN(modernize-use-designated-initializers)
  const std::vector<Row> expected {
    { 1972,  1, 10.0 }, { 1972,  7, 11.0 }, { 1973,  1, 12.0 }, { 1974,  1, 13.0 },
    { 1975,  1, 14.0 }, { 1976,  1, 15.0 }, { 1977,  1, 16.0 }, { 1978,  1, 17.0 },
    { 1979,  1, 18.0 }, { 1980,  1, 19.0 }, { 1981,  7, 20.0 }, { 1982,  7, 21.0 },
    { 1983,  7, 22.0 }, { 1985,  7, 23.0 }, { 1988,  1, 24.0 }, { 1990,  1, 25.0 },
    { 1991,  1, 26.0 }, { 1992,  7, 27.0 }, { 1993,  7, 28.0 }, { 1994,  7, 29.0 },
    { 1996,  1, 30.0 }, { 1997,  7, 31.0 }, { 1999,  1, 32.0 }, { 2006,  1, 33.0 },
    { 2009,  1, 34.0 }, { 2012,  7, 35.0 }, { 2015,  7, 36.0 }, { 2017,  1, 37.0 },
  };
  // NOLINTEND(modernize-use-designated-initializers)

  ASSERT_EQ(LEAP_SECOND_TABLE.size(), expected.size());
  for (std::size_t i = 0; i < expected.size(); ++i) {
    ASSERT_EQ(LEAP_SECOND_TABLE.at(i).start_utc, to_ymd(expected[i].y, expected[i].m, 1));
    ASSERT_EQ(LEAP_SECOND_TABLE.at(i).tai_minus_utc, expected[i].dat);
  }

  // Structure: dates strictly increase, and every step inserts exactly one second.
  for (std::size_t i = 1; i < LEAP_SECOND_TABLE.size(); ++i) {
    ASSERT_LT(LEAP_SECOND_TABLE.at(i - 1).start_utc, LEAP_SECOND_TABLE.at(i).start_utc);
    ASSERT_EQ(LEAP_SECOND_TABLE.at(i).tai_minus_utc - LEAP_SECOND_TABLE.at(i - 1).tai_minus_utc, 1.0);
  }
}


TEST(LeapSecond, TaiMinusUtcLookup) {
  ASSERT_EQ(tai_minus_utc(to_ymd(1972,  1,  1)), 10.0);
  ASSERT_EQ(tai_minus_utc(to_ymd(1972,  6, 30)), 10.0);
  ASSERT_EQ(tai_minus_utc(to_ymd(1972,  7,  1)), 11.0);
  ASSERT_EQ(tai_minus_utc(to_ymd(2016, 12, 31)), 36.0);
  ASSERT_EQ(tai_minus_utc(to_ymd(2017,  1,  1)), 37.0);
  ASSERT_EQ(tai_minus_utc(to_ymd(2026,  7, 28)), 37.0);

  // Past the last entry ΔAT is held (CGPM 2022 Res. 4: no more leap seconds by 2035).
  ASSERT_EQ(tai_minus_utc(to_ymd(5000,  1,  1)), 37.0);

  ASSERT_THROW({ std::ignore = tai_minus_utc(to_ymd(1971, 12, 31)); }, std::invalid_argument);
}


// Columns: year, month, day, fraction-of-day (UTC), expected TT julian date.
// Source: pyerfa 2.0.1 (SOFA) dtf2d("UTC") → utctai → taitt, generated 2026-07-28, seed 42;
// 34 random modern-UTC instants plus both sides of the 2015/2017 leap steps and the table start.
struct UtcGoldenRow { int32_t y; uint32_t m; uint32_t d; double frac; double tt_jd; };
// NOLINTBEGIN(modernize-use-designated-initializers)
const std::vector<UtcGoldenRow> UTC_GOLDEN_ROWS {
  { 2012,  2,  1,    0.9703089467592593, 2455959.4710749653 },
  { 1980, 12,  4,     0.908258449074074, 2444578.4088508566 },
  { 1977, 10, 14,  0.042426180555555554,  2443430.542983866 },
  { 1986,  9, 20,   0.02444363425925926,  2446693.525082338 },
  { 2013, 12, 18,    0.5517008333333333,  2456645.052478426 },
  { 1989,  1, 25,    0.2391823611111111, 2447551.7398326388 },
  { 1989,  3,  7,    0.4208977430555556,  2447592.921548021 },
  { 1978,  6, 28,    0.4849059143518518, 2443687.9854751737 },
  { 1974, 12, 15,    0.7138702083333334, 2442397.2143931715 },
  { 1996,  2, 18,   0.41224210648148146, 2450131.9129618285 },
  { 1995, 10,  7,     0.919476261574074, 2449998.4201844097 },
  { 1986,  5,  3,            0.32993125, 2446553.8305699537 },
  { 1989,  8, 21,   0.46553483796296297, 2447759.9661851157 },
  { 1985, 11,  9,    0.9581135648148148, 2446379.4587522685 },
  { 1976, 10, 21,   0.23245077546296294, 2443072.7329968866 },
  { 1982,  8, 13,   0.37474998842592594,  2445194.875365544 },
  { 2007,  4, 22,   0.45400576388888886, 2454212.9547602083 },
  { 1975,  4, 27,   0.07730239583333333,  2442529.577836933 },
  { 1989,  2,  7,    0.7893874305555555, 2447565.2900377084 },
  { 1985, 11, 16,    0.5395239814814815, 2446386.0401626853 },
  { 2001,  3,  9,   0.17760068287037037,  2451977.678343553 },
  { 2006,  5, 24,    0.7693734722222223, 2453880.2701279167 },
  { 1997,  6,  8,   0.18923159722222224, 2450607.6899513192 },
  { 2020,  1, 28,    0.1316857175925926, 2458876.6324864584 },
  { 2022, 11, 14,    0.7947116435185184, 2459898.2955123843 },
  { 2010,  8, 17,    0.3582366435185185,  2455425.859002662 },
  { 1972, 11, 24,    0.1554755324074074, 2441645.6559753474 },
  { 2020,  5, 25,    0.8479941319444444,  2458995.348794873 },
  { 1999,  3, 15,  0.032552662037037036,  2451252.533295532 },
  { 1988,  9, 25,   0.23118931712962965,  2447429.731839595 },
  { 2012,  5, 27,     0.855978449074074, 2456075.3567444677 },
  { 1981,  6, 25,   0.23260658564814812,  2444780.733198993 },
  { 2005,  1, 20,    0.4382079745370371,  2453390.938950845 },
  { 1995,  5,  8,  0.052693055555555555,  2449845.553401204 },
  { 1972,  1,  1,                   0.0, 2441317.5004882407 },
  { 2016, 12, 31,     0.999988425925926, 2457754.5007775924 },
  { 2017,  1,  1,                   0.0,  2457754.500800741 },
  { 2015,  6, 30,     0.999994212962963, 2457204.5007718056 },
  { 2015,  7,  1, 5.787037037037037e-06, 2457204.5007949537 },
  { 2026,  7, 28,                   0.5,  2461250.000800741 },
};
// NOLINTEND(modernize-use-designated-initializers)


TEST(LeapSecond, UtcToJdeGolden) {
  // Tolerance: measured residual is exactly 0 over all rows (both sides round to the same double);
  // 1e-9 day ≈ 2 ulp of a modern julian date ≈ 86 µs keeps headroom without losing discrimination.
  for (const auto& row : UTC_GOLDEN_ROWS) {
    const Datetime utc_dt { to_ymd(row.y, row.m, row.d), row.frac };
    ASSERT_NEAR(astro::julian_day::utc_to_jde(utc_dt), row.tt_jd, 1e-9);
  }
}


TEST(LeapSecond, JdeToUtcGolden) {
  // The inverse direction exercises the guess-then-refine ΔAT lookup that the production
  // `jde_to_utc8` path depends on — the forward golden alone cannot vouch for it.
  // Tolerance: measured worst residual is 18.2 µs (JD quantization); 100 µs gives 5× headroom.
  for (const auto& row : UTC_GOLDEN_ROWS) {
    const Datetime utc_dt { to_ymd(row.y, row.m, row.d), row.frac };
    const auto recovered = astro::julian_day::jde_to_utc(row.tt_jd);

    if (utc_dt.ymd == MODERN_UTC_START and utc_dt.fraction() == 0.0) {
      // Knife edge: this TT sits exactly on the table-start boundary, where one ulp decides
      // between the table path (exact) and the pre-1972 fallback (off by the ~68 ms seam).
      // Either side is within the documented seam magnitude.
      ASSERT_LT(std::abs(diff_ns(recovered, utc_dt)), 500'000'000);
      continue;
    }

    ASSERT_LT(std::abs(diff_ns(recovered, utc_dt)), 100'000);
  }
}


TEST(LeapSecond, RoundTrip) {
  // Fixed seed: reproducible on failure, and cannot flake by landing inside an inserted second
  // for a different draw (#69 background).
  std::mt19937 rng { 42 };
  std::uniform_int_distribution<int32_t> year_dist { 1972, 2100 };
  std::uniform_int_distribution<uint32_t> month_dist { 1, 12 };
  std::uniform_int_distribution<uint32_t> day_dist { 1, 28 };
  std::uniform_real_distribution<double> frac_dist { 0.0, 1.0 };

  for (int i = 0; i < 1000; ++i) {
    const Datetime utc_dt { to_ymd(year_dist(rng), month_dist(rng), day_dist(rng)), frac_dist(rng) };
    const auto recovered = tt_to_utc(utc_to_tt(utc_dt));

    // Each `Datetime` construction truncates to whole nanoseconds; two hops cost a few ns at most.
    ASSERT_LE(std::abs(diff_ns(recovered, utc_dt)), 10);
  }
}


TEST(LeapSecond, LeapStepBehavior) {
  // Across an inserted leap second, one elapsed UTC second spans two TT seconds.
  const Datetime before_2017 { to_ymd(2016, 12, 31), std::chrono::hh_mm_ss { 23h + 59min + 59s } };
  const Datetime start_2017  { to_ymd(2017,  1,  1), 0.0 };
  const double leap_gap_s = (astro::julian_day::utc_to_jde(start_2017)
                           - astro::julian_day::utc_to_jde(before_2017)) * 86400.0;
  ASSERT_NEAR(leap_gap_s, 2.0, 1e-4);

  // The same wall-clock pair across an ordinary New Year spans exactly one second.
  const Datetime before_2018 { to_ymd(2017, 12, 31), std::chrono::hh_mm_ss { 23h + 59min + 59s } };
  const Datetime start_2018  { to_ymd(2018,  1,  1), 0.0 };
  const double plain_gap_s = (astro::julian_day::utc_to_jde(start_2018)
                            - astro::julian_day::utc_to_jde(before_2018)) * 86400.0;
  ASSERT_NEAR(plain_gap_s, 1.0, 1e-4);
}


TEST(LeapSecond, InsertedSecondMapsForward) {
  // The inserted second 2016-12-31 23:59:60.5 (UTC) is TT 2017-01-01 00:01:08.684. A `Datetime`
  // cannot carry 23:59:60, so `tt_to_utc` lands it in the first second of the next day —
  // documented duplication.
  const Datetime tt_dt { to_ymd(2017, 1, 1), std::chrono::hh_mm_ss { 1min + 8s + 684ms } };
  const auto utc_dt = tt_to_utc(tt_dt);
  ASSERT_EQ(utc_dt.ymd, to_ymd(2017, 1, 1));
  ASSERT_NEAR(utc_dt.fraction() * 86400.0, 0.5, 1e-4);

  // Contract: on that 1 s TT interval the conversion is not invertible — the round trip
  // comes back exactly one second late (the inserted second collapses onto the next one,
  // whose ΔAT is already one higher).
  const auto round_trip = utc_to_tt(tt_to_utc(tt_dt));
  ASSERT_NEAR(static_cast<double>(diff_ns(round_trip, tt_dt)), 1e9, 100.0);
}


TEST(LeapSecond, Pre1972FallsBackToUt1) {
  const Datetime dt { to_ymd(1950, 6, 15), 0.5 };
  ASSERT_EQ(utc_to_tt(dt), delta_t::ut1_to_tt(dt));
  ASSERT_EQ(tt_to_utc(dt), delta_t::tt_to_ut1(dt));

  // A TT instant inside the first 42.184 s of 1972-01-01 still maps below the table start —
  // that band belongs to the fallback as well.
  const Datetime tt_band { to_ymd(1972, 1, 1), std::chrono::hh_mm_ss { 30s } };
  ASSERT_EQ(tt_to_utc(tt_band), delta_t::tt_to_ut1(tt_band));

  // The seam at 1972-01-01 is ΔT(1972.0) − 42.184 s — the two paths agree to well under a second.
  const Datetime seam { to_ymd(1972, 1, 1), 0.0 };
  ASSERT_LT(std::abs(diff_ns(utc_to_tt(seam), delta_t::ut1_to_tt(seam))), 500'000'000);
}


TEST(LeapSecond, UtcBoundaryFormula) {
  // New-year midnight UTC in JDE is the calendar julian date plus TT − UTC — nothing else (#84:
  // the moments() year bound used to add model ΔT here instead, which drifts away from
  // ΔAT + 32.184 s without bound).
  for (const int32_t year : { 1980, 2026, 2500, 4500 }) {
    const Datetime jan1 { to_ymd(year, 1, 1), 0.0 };
    const double calendar_jd = astro::julian_day::ut1_to_jd(jan1); // pure calendar arithmetic
    ASSERT_NEAR(astro::julian_day::utc_to_jde(jan1), calendar_jd + tt_minus_utc(jan1.ymd) / 86400.0, 1e-9);
  }
}

} // namespace astro::leap_second::test
