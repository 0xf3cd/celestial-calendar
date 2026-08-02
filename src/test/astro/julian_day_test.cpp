/*
 * CelestialCalendar: 
 *   A C++23-style library that performs astronomical calculations and date conversions among various calendars,
 *   including Gregorian, Lunar, and Chinese Ganzhi calendars.
 * 
 * Copyright (C) 2024 Ningqi Wang (0xf3cd)
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
#include <limits>
#include <unordered_map>
#include "util.hpp"
#include "datetime.hpp"
#include "julian_day.hpp"

namespace astro::julian_day::test {

// The UTC family (`utc_to_jde` / `jde_to_utc`) is exercised in `leap_second_test.cpp`,
// alongside the ΔAT machinery it composes.

using namespace astro::julian_day;

using namespace util;
using namespace util::ymd_operator;
using namespace calendar;
using namespace std::chrono_literals;

// Provenance (#68): the first row is a textbook anchor (2299160.5 = 1582-10-15, the Gregorian
// adoption, Meeus Ch.7); the other rows below are legacy spot checks introduced by "Julian day
// numbers" #5 (2024-06) with no recorded source, except the last 4 rows (stevegs.com's JD
// calculator, cited inline). The real correctness gate for these conversions is the Consistency
// round-trip suite (5000 random points each direction) plus the InvalidInput boundary tests.
const std::unordered_map<double, Datetime> JDE_TEST_DATASET {
  { 2299160.5,         Datetime { to_ymd(1582, 10, 15), 0.0, } },
  { 2451544.5,         Datetime { to_ymd(2000, 1, 1),   0.0, } },
  { 2443259.9,         Datetime { to_ymd(1977, 4, 26),  0.4, } },
  { 2450084.0,         Datetime { to_ymd(1996, 1, 1),   0.5, } },
  { 2456293.520833,    Datetime { to_ymd(2013, 1, 1),   hh_mm_ss { 30min } } },
  { 2460491.1846759,   Datetime { to_ymd(2024, 6, 29),  hh_mm_ss { 16h + 25min + 56s } } },
  { 2451545.041666667, Datetime { to_ymd(2000, 1, 1),   hh_mm_ss { 13h } } },
  { 2500000.0,         Datetime { to_ymd(2132, 8, 31),  0.5, } },
  { 2305993.3852315,   Datetime { to_ymd(1601, 6, 29),  hh_mm_ss { 21h + 14min + 44s } } },

  // From http://www.stevegs.com/utils/jd_calc/
  { 2458908.7084259,   Datetime { to_ymd(2020, 2, 29),  hh_mm_ss { 5h + 8s } } },
  { 2461436.1508698,   Datetime { to_ymd(2027, 1, 30),  hh_mm_ss { 15h + 37min + 15s + 150ms } } },
  { 2473063.7966088,   Datetime { to_ymd(2058, 12, 1),  hh_mm_ss { 7h + 7min + 7s } } },
  { 2459048.7966177,   Datetime { to_ymd(2020, 7, 18),  hh_mm_ss { 7h + 7min + 7s + 770ms } } }
};

constexpr double EPSILON = 1e-6;


TEST(JulianDay, TTtoJDE) {
  for (const auto& [jde, tt] : JDE_TEST_DATASET) {
    ASSERT_NEAR(tt_to_jde(tt), jde, EPSILON);
  }
}

TEST(JulianDay, JDEtoTT) {
  for (const auto& [jde, expected_dt] : JDE_TEST_DATASET) {
    const auto tt = jde_to_tt(jde);
    ASSERT_EQ(tt.ymd, expected_dt.ymd);
    ASSERT_NEAR(tt.fraction(), expected_dt.fraction(), EPSILON);
  }
}

TEST(JulianDay, Consistency) {  
  const auto random_ymd = [] -> year_month_day {
    using namespace util::ymd_operator;
    const std::chrono::year_month_day ymd = to_ymd(util::random(500, 2100), 1, 1);
    const auto random_days = util::random(0, 365 * 10);
    return ymd + random_days;
  };

  const auto random_hms = [] -> hh_mm_ss<nanoseconds> {
    using namespace util::ymd_operator;
    const auto ns = nanoseconds { 
      util::random<uint64_t>(0, in_a_day<nanoseconds>() - 1)
    };
    return hh_mm_ss { ns };
  };

  for (auto i = 0; i < 5000; ++i) {
    const auto ymd = random_ymd();
    const auto hms = random_hms();
    const Datetime tt { ymd, hms };

    const double jde = tt_to_jde(tt);
    const auto recovered_tt = jde_to_tt(jde);

    ASSERT_EQ(tt.ymd, recovered_tt.ymd);
    ASSERT_NEAR(tt.fraction(), recovered_tt.fraction(), EPSILON);
  }

  const auto random_jde = [] -> double {
    // Return a jde number falls in gregorian year 500-ish ~ 2100-ish.
    return util::random(1903682.686921, 2488069.686921);
  };

  for (auto i = 0; i < 5000; ++i) {
    const double jde = random_jde();
    const auto tt = jde_to_tt(jde);

    const double recovered_jde = tt_to_jde(tt);
    ASSERT_NEAR(recovered_jde, jde, EPSILON);

    const auto recovered_tt = jde_to_tt(recovered_jde);
    ASSERT_EQ(tt.ymd, recovered_tt.ymd);
    ASSERT_NEAR(tt.fraction(), recovered_tt.fraction(), EPSILON);
  }
}

TEST(JulianDay, InvalidInput) {
  // #77: `ut1_to_jd` rejects year < 1 explicitly — its unsigned arithmetic would otherwise
  // wrap around and silently produce a garbage JD (release builds included).
  ASSERT_THROW(ut1_to_jd(Datetime { to_ymd(0, 1, 1), 0.0 }), std::runtime_error);
  ASSERT_THROW(ut1_to_jd(Datetime { to_ymd(0, 2, 29), 0.5 }), std::runtime_error);
  ASSERT_THROW(ut1_to_jd(Datetime { to_ymd(-1, 12, 31), 0.99 }), std::runtime_error);
  ASSERT_THROW(ut1_to_jd(Datetime { to_ymd(-4712, 1, 1), 0.0 }), std::runtime_error);

  // The wrappers propagate the throw (this is what turns the C-ABI garbage into an error).
  ASSERT_THROW(tt_to_jde(Datetime { to_ymd(0, 1, 1), 0.0 }), std::runtime_error);

  // Both directions throw the same exception type on out-of-domain input. Their domains differ:
  // `ut1_to_jd` accepts years 1-400 that sit below `jd_to_ut1`'s bound, so round-trips only
  // close from 401-01-01 onwards.
  ASSERT_THROW(jd_to_ut1(-5.0), std::runtime_error); // Negative finite values throw, not abort.
  ASSERT_THROW(jd_to_ut1(1.0), std::runtime_error);
  ASSERT_THROW(jd_to_ut1(1867522.4999), std::runtime_error); // 400-12-31, just below the bound.

  // Non-finite JDs bypass ordinary range checks (NaN comparisons are false) — rejected first.
  ASSERT_THROW(jd_to_ut1(std::numeric_limits<double>::quiet_NaN()), std::runtime_error);
  ASSERT_THROW(jd_to_ut1(std::numeric_limits<double>::infinity()), std::runtime_error);
  ASSERT_THROW(jd_to_ut1(-std::numeric_limits<double>::infinity()), std::runtime_error);

  // #67: above JD 13689325.5 (conceptually 32768-01-01) `std::chrono::year` overflows, and the
  // uint32 arithmetic wraps into valid-looking but wrong dates.
  ASSERT_NO_THROW(jd_to_ut1(13689325.499999));
  ASSERT_THROW(jd_to_ut1(13689325.5), std::runtime_error);
  ASSERT_THROW(jd_to_ut1(4.0e9), std::runtime_error); // wrapped to 2403-12-05 before #67.

  // Smallest supported year converts correctly: JD of 1-01-01 00:00 (gregorian) is 1721425.5.
  ASSERT_NEAR(ut1_to_jd(Datetime { to_ymd(1, 1, 1), 0.0 }), 1721425.5, EPSILON);

  // The inverse's lower bound is exactly 401-01-01 00:00, and both directions agree on it.
  const auto y401 = jd_to_ut1(1867522.5);
  ASSERT_EQ(y401.ymd, to_ymd(401, 1, 1));
  ASSERT_NEAR(ut1_to_jd(Datetime { to_ymd(401, 1, 1), 0.0 }), 1867522.5, EPSILON);
}

TEST(JulianDay, JulianMillennium) {
  ASSERT_EQ(jde_to_jm(J2000), 0.0);
  ASSERT_EQ(jm_to_jde(0.0), J2000);

  ASSERT_NEAR(jde_to_jm(J2000 + 365250.0), 1.0, EPSILON);
  ASSERT_NEAR(jm_to_jde(1.0), J2000 + 365250.0, EPSILON);

  for (auto i = 0; i < 100; ++i) {
    const double random_jde = util::random(1903682.686921, 2488069.686921);

    const double jm = jde_to_jm(random_jde);
    ASSERT_NEAR(jm_to_jde(jm), random_jde, EPSILON);

    const double jde = jm_to_jde(jm);
    ASSERT_NEAR(jde_to_jm(jde), jm, EPSILON);
  }
}

TEST(JulianDay, JulianCentury) {
  ASSERT_EQ(jde_to_jc(J2000), 0.0);
  ASSERT_EQ(jc_to_jde(0.0), J2000);

  ASSERT_NEAR(jde_to_jc(J2000 + 36525.0), 1.0, EPSILON);
  ASSERT_NEAR(jc_to_jde(1.0), J2000 + 36525.0, EPSILON);

  for (auto i = 0; i < 100; ++i) {
    const double random_jde = util::random(1903682.686921, 2488069.686921);

    const double jc = jde_to_jc(random_jde);
    ASSERT_NEAR(jc_to_jde(jc), random_jde, EPSILON);

    const double jde = jc_to_jde(jc);
    ASSERT_NEAR(jde_to_jc(jde), jc, EPSILON);
  }
}

} // namespace astro::julian_day::test
