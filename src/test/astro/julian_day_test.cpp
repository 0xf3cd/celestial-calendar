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

#include <tuple>
#include <limits>
#include <vector>
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

TEST(JulianDay, JdeUt1Anchors) {
  // The UT1 family composes ΔT on top of the TT conversion: UT1 = TT − ΔT. The anchors pin the
  // composition against *observed* ΔT values, not `delta_t::compute` itself — a sign flip in the
  // composition would err by 2ΔT (~128 s at the modern anchors), far outside any tolerance here.
  //
  // Provenance for the observed ΔT (TT − UT1) at each anchor:
  //   2000-01-01: 63.83 s — NASA eclipse ΔT table (https://eclipse.gsfc.nasa.gov/SEcat5/deltat.html).
  //   2020-01-01: 69.36 s — USNO observations (https://maia.usno.navy.mil/ser7/deltat.data);
  //               matches this repo's own ACCURATE_DELTA_T_TABLE (delta_t_test_helper.hpp).
  //   year 500:   5710 s  — Stephenson & Morrison, the same table the ΔT models fit, so the
  //               models sit within ~1 s of it there.
  // The conversion itself is far tighter than these tolerances.
  struct Anchor {
    Datetime tt;
    Datetime ut1;    // tt − observed ΔT
    double tol_sec;  // |model ΔT − observed ΔT| bound at this epoch
  };

  // NOLINTBEGIN(modernize-use-designated-initializers)
  const std::vector<Anchor> anchors {
    //                  tt                              ut1 (= tt − observed ΔT)                                     tol_sec
    { Datetime { to_ymd(2000, 1, 1), 0.0 }, Datetime { to_ymd(1999, 12, 31), hh_mm_ss { 23h + 58min + 56s + 170ms } }, 0.2 },
    { Datetime { to_ymd(2020, 1, 1), 0.0 }, Datetime { to_ymd(2019, 12, 31), hh_mm_ss { 23h + 58min + 50s + 640ms } }, 0.2 },
    { Datetime { to_ymd( 500, 1, 1), 0.0 }, Datetime { to_ymd( 499, 12, 31), hh_mm_ss { 22h + 24min + 50s } },         2.0 },
  };
  // NOLINTEND(modernize-use-designated-initializers)

  for (const auto& [tt, ut1, tol_sec] : anchors) {
    const double tol_day = tol_sec / 86400.0;
    const double jde = tt_to_jde(tt);

    // Forward: a TT jde lands on the observed UT1 moment.
    const auto converted_ut1 = jde_to_ut1(jde);
    ASSERT_EQ(converted_ut1.ymd, ut1.ymd);
    ASSERT_NEAR(converted_ut1.fraction(), ut1.fraction(), tol_day);

    // Reverse: the observed UT1 moment maps back to the same jde.
    ASSERT_NEAR(ut1_to_jde(ut1), jde, tol_day);
  }
}

TEST(JulianDay, JdeUt1Consistency) {
  // The two directions read ΔT on different dates: `jde_to_ut1` on the TT date, `ut1_to_jde` on
  // the UT1 date. The round-trip residual is the ΔT slope applied to a ΔT-sized shift — ~2e-8 day
  // at the 401-600 end of the span, bit-exact from 1900 on — so EPSILON keeps ~40x of headroom
  // while still catching a flipped sign or a stale/wrong scale in either direction.
  for (auto i = 0; i < 2000; ++i) {
    // 401-01-01 (the `jd_to_ut1` bound) through year 2100.
    const double jde = util::random(1867522.5, 2488070.5);
    ASSERT_NEAR(ut1_to_jde(jde_to_ut1(jde)), jde, EPSILON);
  }

  for (auto i = 0; i < 2000; ++i) {
    // Round trips only close from 401-01-01 onwards (see `JulianDay.InvalidInput`).
    const auto ymd = to_ymd(util::random(401, 2100), util::random(1, 12), util::random(1, 28));
    const Datetime ut1 { ymd, util::random(0.0, 1.0) };

    const auto recovered_ut1 = jde_to_ut1(ut1_to_jde(ut1));
    ASSERT_EQ(ut1.ymd, recovered_ut1.ymd);
    ASSERT_NEAR(ut1.fraction(), recovered_ut1.fraction(), EPSILON);
  }
}

TEST(JulianDay, InvalidInput) {
  // #77: year 1 is the lower bound of the public forward domain.
  ASSERT_THROW(std::ignore = ut1_to_jd(Datetime { to_ymd(0, 1, 1), 0.0 }), std::runtime_error);
  ASSERT_THROW(std::ignore = ut1_to_jd(Datetime { to_ymd(0, 2, 29), 0.5 }), std::runtime_error);
  ASSERT_THROW(std::ignore = ut1_to_jd(Datetime { to_ymd(-1, 12, 31), 0.99 }), std::runtime_error);
  ASSERT_THROW(std::ignore = ut1_to_jd(Datetime { to_ymd(-4712, 1, 1), 0.0 }), std::runtime_error);

  // The wrappers propagate the same domain error.
  ASSERT_THROW(std::ignore = tt_to_jde(Datetime { to_ymd(0, 1, 1), 0.0 }), std::runtime_error);

  // `jde_to_ut1` still runs `jd_to_ut1`'s domain gates: the JD of 1-01-01 sits below the
  // 401-01-01 bound pinned below.
  ASSERT_THROW(std::ignore = jde_to_ut1(1721425.5), std::runtime_error);

  // Both directions throw the same exception type on out-of-domain input. Their domains differ:
  // `ut1_to_jd` accepts years 1-400 that sit below `jd_to_ut1`'s bound, so round-trips only
  // close from 401-01-01 onwards.
  ASSERT_THROW(std::ignore = jd_to_ut1(-5.0), std::runtime_error); // Negative finite values throw, not abort.
  ASSERT_THROW(std::ignore = jd_to_ut1(1.0), std::runtime_error);
  ASSERT_THROW(std::ignore = jd_to_ut1(1867522.4999), std::runtime_error); // 400-12-31, just below the bound.

  // Non-finite JDs bypass ordinary range checks (NaN comparisons are false) — rejected first.
  ASSERT_THROW(std::ignore = jd_to_ut1(std::numeric_limits<double>::quiet_NaN()), std::runtime_error);
  ASSERT_THROW(std::ignore = jd_to_ut1(std::numeric_limits<double>::infinity()), std::runtime_error);
  ASSERT_THROW(std::ignore = jd_to_ut1(-std::numeric_limits<double>::infinity()), std::runtime_error);

  // #67: JD 13689325.5 is conceptually 32768-01-01, beyond `std::chrono::year`.
  ASSERT_NO_THROW(std::ignore = jd_to_ut1(13689325.499999));
  ASSERT_THROW(std::ignore = jd_to_ut1(13689325.5), std::runtime_error);
  ASSERT_THROW(std::ignore = jd_to_ut1(4.0e9), std::runtime_error);

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
