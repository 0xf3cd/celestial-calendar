/*
 * CelestialCalendar: 
 *   A C++23-style library that performs astronomical calculations and date conversions between
 *   Gregorian and Chinese Lunar calendars.
 * 
 * Copyright (C) 2024 Ningqi Wang (0xf3cd)
 * Email: nq.maigre@gmail.com
 * Repo : https://github.com/0xf3cd/celestial-calendar
 *  
 * SPDX-License-Identifier: MIT
 */

#include <gtest/gtest.h>

#include <tuple>
#include <array>
#include <cmath>
#include <stdexcept>
#include <string>
#include <string_view>
#include <limits>
#include <vector>
#include <unordered_map>
#include "ymd.hpp"
#include "../random.hpp"
#include "datetime.hpp"
#include "julian_day.hpp"

namespace astro::julian_day::test {

// Retained material boundaries: the V27 NASA/USNO anchors, V28 stevegs.com rows, V32 printed Meeus
// examples, and V37 internal rows with an unrecovered source remain outside the project MIT grant. The
// independently evaluated V31 formulas are project-authored and remain inside its scope.

// The UTC family (`utc_to_jde` / `jde_to_utc`) is exercised in `leap_second_test.cpp`,
// alongside the ΔAT machinery it composes.

using namespace astro::julian_day;

using namespace util;
using namespace util::ymd_operator;
using namespace calendar;
using namespace std::chrono_literals;

TEST(JulianDay, UtcYearWindows) {
  for (const int32_t year : { 1, 1900, 1971, 1972, 2016, 2026, 32766 }) {
    const auto [start_jde_tt, end_jde_tt] = utc_year_jde_window(year);
    const Datetime start_utc { to_ymd(year, 1, 1), 0.0 };
    const Datetime end_utc { to_ymd(year + 1, 1, 1), 0.0 };
    EXPECT_EQ(start_jde_tt, utc_to_jde(start_utc));
    EXPECT_EQ(end_jde_tt, utc_to_jde(end_utc));
    EXPECT_LT(start_jde_tt, end_jde_tt);
    if (year < 1972) {
      EXPECT_EQ(start_jde_tt, ut1_to_jde(start_utc));
    }
    if (year < 32766) {
      EXPECT_EQ(end_jde_tt, utc_year_jde_window(year + 1).front());
    }
  }
  const Datetime modern { to_ymd(2026, 1, 1), 0.0 };
  EXPECT_NE(utc_year_jde_window(2026).front(), ut1_to_jde(modern));

  // Assert the diagnostic names the original year and the window, not just the exception type.
  for (const int32_t year : { -32768, 0, 32767, std::numeric_limits<int32_t>::max() }) {
    try {
      std::ignore = utc_year_jde_window(year);
      FAIL() << "utc_year_jde_window(" << year << ") must be rejected";
    } catch (const std::invalid_argument& e) {
      const std::string_view message { e.what() };
      EXPECT_NE(message.find(std::to_string(year)), std::string_view::npos);
      EXPECT_NE(message.find("[1, 32766]"), std::string_view::npos);
    }
  }
  EXPECT_THROW(std::ignore = utc_year_start_jde(std::chrono::year { -32768 }), std::invalid_argument);
  EXPECT_THROW(std::ignore = utc_year_start_jde(std::chrono::year { 0 }), std::invalid_argument);
}

TEST(JulianDay, AnnualYearValidation) {
  // The owner-level guard behind `utc_year_jde_window` and Sun's annual queries.
  for (const int32_t year : { 1, 1900, 2026, 32766 }) {
    EXPECT_NO_THROW(validate_annual_year(year));
  }
  for (const int32_t year : { -1, 0, 32767, 65537,
                              std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max() }) {
    try {
      validate_annual_year(year);
      FAIL() << "validate_annual_year(" << year << ") must be rejected";
    } catch (const std::invalid_argument& e) {
      const std::string_view message { e.what() };
      EXPECT_NE(message.find(std::to_string(year)), std::string_view::npos);
      EXPECT_NE(message.find("[1, 32766]"), std::string_view::npos);
    }
  }

  // Scalar year-start conversion never forms `year + 1`, so 32767 stays valid there even
  // though the annual window rejects it above.
  EXPECT_TRUE(std::isfinite(utc_year_start_jde(std::chrono::year { 32767 })));
}

// This spot-check family was introduced by da333dd (#5, 2024-06-30); no later numeric shift is
// recorded. The real correctness gate is the Consistency round-trip suite (5000 random points
// each direction) plus the InvalidInput boundary tests.
// Retained data: julian_day-0.
const std::unordered_map<double, Datetime> JDE_TEST_DATASET {
  // Meeus Ch.7 textbook anchor: 2299160.5 = 1582-10-15, the Gregorian adoption.
  { 2299160.5,         Datetime { to_ymd(1582, 10, 15), 0.0, } },
  // Meeus Ch.7 worked value: 2443259.9 = 1977 April 26.4.
  { 2443259.9,         Datetime { to_ymd(1977, 4, 26),  0.4, } },

  // Seven rows are internal regression material with no recovered source,
  // generator, or seed.
  { 2451544.5,         Datetime { to_ymd(2000, 1, 1),   0.0, } },
  { 2450084.0,         Datetime { to_ymd(1996, 1, 1),   0.5, } },
  { 2456293.520833,    Datetime { to_ymd(2013, 1, 1),   hh_mm_ss { 30min } } },
  { 2460491.1846759,   Datetime { to_ymd(2024, 6, 29),  hh_mm_ss { 16h + 25min + 56s } } },
  { 2451545.041666667, Datetime { to_ymd(2000, 1, 1),   hh_mm_ss { 13h } } },
  { 2500000.0,         Datetime { to_ymd(2132, 8, 31),  0.5, } },
  { 2305993.3852315,   Datetime { to_ymd(1601, 6, 29),  hh_mm_ss { 21h + 14min + 44s } } },

  // Four rows from http://www.stevegs.com/utils/jd_calc/.
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
  //   2000-01-01: 63.8285 s from USNO deltat.data, rounded to the stored 63.83 s.
  //               https://maia.usno.navy.mil/ser7/deltat.data
  //   2020-01-01: 69.36 s — USNO observations (https://maia.usno.navy.mil/ser7/deltat.data);
  //               matches this repo's own ACCURATE_DELTA_T_TABLE (delta_t_test_helper.hpp).
  //   year 500:   5710 s from NASA/TP-2006-214141 Table 2-1 (standard error 140 s),
  //               with Morrison-Stephenson (2004) scientific lineage in Section 2.6.
  // The conversion itself is far tighter than these tolerances.
  struct Anchor {
    Datetime tt;
    Datetime ut1;    // tt − observed ΔT
    double tol_sec;  // |model ΔT − observed ΔT| bound at this epoch
  };

  // NOLINTBEGIN(modernize-use-designated-initializers)
  // Retained data: julian_day-1.
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
  ASSERT_THROW(std::ignore = ut1_to_jd(Datetime { to_ymd(0, 1, 1), 0.0 }), std::invalid_argument);
  ASSERT_THROW(std::ignore = ut1_to_jd(Datetime { to_ymd(0, 2, 29), 0.5 }), std::invalid_argument);
  ASSERT_THROW(std::ignore = ut1_to_jd(Datetime { to_ymd(-1, 12, 31), 0.99 }), std::invalid_argument);
  ASSERT_THROW(std::ignore = ut1_to_jd(Datetime { to_ymd(-4712, 1, 1), 0.0 }), std::invalid_argument);

  // The wrappers propagate the same domain error.
  ASSERT_THROW(std::ignore = tt_to_jde(Datetime { to_ymd(0, 1, 1), 0.0 }), std::invalid_argument);

  // `jde_to_ut1` still runs `jd_to_ut1`'s domain gates: the JD of 1-01-01 sits below the
  // 401-01-01 bound pinned below.
  ASSERT_THROW(std::ignore = jde_to_ut1(1721425.5), std::invalid_argument);

  // Both directions throw the same exception type on out-of-domain input. Their domains differ:
  // `ut1_to_jd` accepts years 1-400 that sit below `jd_to_ut1`'s bound, so round-trips only
  // close from 401-01-01 onwards.
  ASSERT_THROW(std::ignore = jd_to_ut1(-5.0), std::invalid_argument); // Negative finite values throw, not abort.
  ASSERT_THROW(std::ignore = jd_to_ut1(1.0), std::invalid_argument);
  ASSERT_THROW(std::ignore = jd_to_ut1(1867522.4999), std::invalid_argument); // 400-12-31, just below the bound.

  // Non-finite JDs bypass ordinary range checks (NaN comparisons are false) — rejected first.
  ASSERT_THROW(std::ignore = jd_to_ut1(std::numeric_limits<double>::quiet_NaN()), std::invalid_argument);
  ASSERT_THROW(std::ignore = jd_to_ut1(std::numeric_limits<double>::infinity()), std::invalid_argument);
  ASSERT_THROW(std::ignore = jd_to_ut1(-std::numeric_limits<double>::infinity()), std::invalid_argument);

  // #67: JD 13689325.5 is conceptually 32768-01-01, beyond `std::chrono::year`.
  ASSERT_NO_THROW(std::ignore = jd_to_ut1(13689325.499999));
  ASSERT_THROW(std::ignore = jd_to_ut1(13689325.5), std::invalid_argument);
  ASSERT_THROW(std::ignore = jd_to_ut1(4.0e9), std::invalid_argument);

  // Smallest supported year converts correctly: JD of 1-01-01 00:00 (gregorian) is 1721425.5.
  ASSERT_NEAR(ut1_to_jd(Datetime { to_ymd(1, 1, 1), 0.0 }), 1721425.5, EPSILON);

  // The inverse's lower bound is exactly 401-01-01 00:00, and both directions agree on it.
  const auto y401 = jd_to_ut1(1867522.5);
  ASSERT_EQ(y401.ymd, to_ymd(401, 1, 1));
  ASSERT_NEAR(ut1_to_jd(Datetime { to_ymd(401, 1, 1), 0.0 }), 1867522.5, EPSILON);
}

TEST(JulianDay, Ut1ToJdeRejectsMutatedInvalidDatetime) {
  const Datetime valid { to_ymd(2026, 2, 28), 0.5 };
  ASSERT_TRUE(std::isfinite(ut1_to_jde(valid)));
  for (const auto ymd : std::array { to_ymd(2026, 2, 30), to_ymd(1900, 2, 29) }) {
    auto invalid = valid;
    invalid.ymd = ymd;
    ASSERT_FALSE(invalid.ok());
    ASSERT_THROW(std::ignore = ut1_to_jde(invalid), std::invalid_argument);
  }
  for (const int64_t elapsed_ns : { -1LL, 86400'000'000'000LL }) {
    auto invalid = valid;
    invalid.time_of_day = std::chrono::hh_mm_ss { std::chrono::nanoseconds { elapsed_ns } };
    ASSERT_FALSE(invalid.ok());
    ASSERT_THROW(std::ignore = ut1_to_jde(invalid), std::invalid_argument);
  }
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
