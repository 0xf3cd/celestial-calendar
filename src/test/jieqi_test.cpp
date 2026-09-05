/*
 * CelestialCalendar: 
 *   A C++23-style library that performs astronomical calculations and date conversions among various calendars,
 *   including Gregorian, Lunar, and Chinese Ganzhi calendars.
 * 
 * Copyright (C) 2024 Ningqi Wang (0xf3cd)
 * Email: nq.maigre@gmail.com
 * Repo : https://github.com/0xf3cd/celestial-calendar
 *  
 * SPDX-License-Identifier: MIT
 */

#include <gtest/gtest.h>
#include <cmath>
#include <algorithm>
#include <limits>
#include <ranges>
#include <type_traits>
#include <utility>
#include <vector>
#include "util.hpp"
#include "jieqi.hpp"

// gcc 14.2 (the pinned CI leg) mis-fires -Wfree-nonheap-object on the pre-existing ranges
// pipelines below — the false-positive family of gcc bug 108187: `jieqi_jde` becoming an
// inlinable function perturbs this TU's inlining graph and the warning surfaces on an
// impossible path. 14.4 (local) and the sanitizer legs are clean. Scoped to this TU and
// to gcc; clang does not know the warning.
#if defined(__GNUC__) and not defined(__clang__)
#pragma GCC diagnostic ignored "-Wfree-nonheap-object"
#endif

namespace calendar::jieqi::test {

using namespace calendar::jieqi;
using namespace astro::sun::geocentric_coord::math;

// The UT1Moment test and its DATASET (wolfram/bmcx/weather.gov values of mixed, undocumented
// time scales — "Assume they are in UT1"; split ymd+fraction assertion, #68) were retired
// 2026-07-27: jieqi_golden_test.cpp supersedes them with the official HKO almanac (168
// minute-precision values, single continuous JD assertion) and DE441-derived crossings.

TEST(JieQi, CachedWrapperIsLazilyInitialized) {
  // #67: the shape is the guarantee — a revert to a namespace-scope callable must fail to compile.
  static_assert(std::is_function_v<decltype(jieqi_jde)>);
}

TEST(JieQi, YearGuard) {
  // Without the guard the conversion chain wraps out-of-range years: 65537 wraps to year 1
  // and would be cached under the *unwrapped* key; negative wraps hit the Julian-day layer's
  // lower gate with a misleading message.
  ASSERT_THROW(std::ignore = calc_jieqi_jde(0, Jieqi::春分), std::out_of_range);
  ASSERT_THROW(std::ignore = calc_jieqi_jde(-5, Jieqi::春分), std::out_of_range);
  ASSERT_THROW(std::ignore = calc_jieqi_jde(32767, Jieqi::春分), std::out_of_range);
  ASSERT_THROW(std::ignore = calc_jieqi_jde(65537, Jieqi::春分), std::out_of_range);
  ASSERT_THROW(std::ignore = calc_jieqi_jde(100000, Jieqi::春分), std::out_of_range);

  // The cached wrapper goes through the same guard — a poisoned entry never forms.
  ASSERT_THROW(std::ignore = jieqi_jde(65537, Jieqi::春分), std::out_of_range);
  ASSERT_THROW(std::ignore = jieqi_jde(100000, Jieqi::春分), std::out_of_range);

  // Boundary years stay computable — the ceiling is 32766 because the root search brackets
  // with `year + 1`.
  ASSERT_NO_THROW(std::ignore = calc_jieqi_jde(1, Jieqi::春分));
  ASSERT_NO_THROW(std::ignore = calc_jieqi_jde(32766, Jieqi::春分));
}

TEST(JieQi, Ut1MomentYearGuard) {
  ASSERT_THROW(std::ignore = jieqi_ut1_moment(400, Jieqi::春分), std::out_of_range);
  ASSERT_NO_THROW(std::ignore = jieqi_ut1_moment(401, Jieqi::春分));
}

TEST(JieQi, FromIndexBounds) {
  // 255 is the largest value the uint8_t parameter can carry — the input a checkless
  // `static_cast` implementation would happily accept.
  ASSERT_THROW(std::ignore = from_index(JIEQI_COUNT), std::out_of_range);
  ASSERT_THROW(std::ignore = from_index(std::numeric_limits<uint8_t>::max()), std::out_of_range);

  ASSERT_EQ(from_index(0), Jieqi::立春);
  ASSERT_EQ(from_index(JIEQI_COUNT - 1), Jieqi::大寒);
}

TEST(JieQi, GeneratorExclusiveStart) {
  // The generator's start is exclusive: starting exactly at a jieqi moment yields the *next*
  // jieqi. The start here is the cached wrapper's own return value, so the comparison sees
  // identical bits — this pins the boundary, not float luck.
  const double chunfen = jieqi_jde(2025, Jieqi::春分);

  const auto [jq1, jde1] = JieqiGenerator { chunfen }.next();
  ASSERT_EQ(jq1, Jieqi::清明);
  ASSERT_EQ(jde1, jieqi_jde(2025, Jieqi::清明));

  // One ulp before the moment, the moment itself is still ahead.
  const auto [jq2, jde2] = JieqiGenerator { std::nextafter(chunfen, 0.0) }.next();
  ASSERT_EQ(jq2, Jieqi::春分);
  ASSERT_EQ(jde2, chunfen);

  // The only deterministic way to hit the constructor's roll-over path (`++_year; _jq_index =
  // 小寒`): no jieqi of the start year lies ahead of 冬至, so the next one is next year's 小寒.
  const auto [jq3, jde3] = JieqiGenerator { jieqi_jde(2025, Jieqi::冬至) }.next();
  ASSERT_EQ(jq3, Jieqi::小寒);
  ASSERT_EQ(jde3, jieqi_jde(2026, Jieqi::小寒));
}

TEST(JieQi, NameQuery) {
  ASSERT_EQ(longitude_of(Jieqi::立春), 315.0);
  ASSERT_EQ(longitude_of(Jieqi::雨水), 330.0);
  ASSERT_EQ(longitude_of(Jieqi::惊蛰), 345.0);
  ASSERT_EQ(longitude_of(Jieqi::春分), 0.0);
  ASSERT_EQ(longitude_of(Jieqi::清明), 15.0);
  ASSERT_EQ(longitude_of(Jieqi::秋分), 180.0);
  ASSERT_EQ(longitude_of(Jieqi::小雪), 240.0);

  // Test couple English aliases as well.
  ASSERT_EQ(longitude_of(Jieqi::LICHUN),  315.0);
  ASSERT_EQ(longitude_of(Jieqi::CHUNFEN), 0.0);
  ASSERT_EQ(longitude_of(Jieqi::XIAOHAN), 285.0);
  ASSERT_EQ(longitude_of(Jieqi::DAHAN),   300.0);
}


TEST(JieQi, IsJieOrQi) {
  ASSERT_TRUE(is_jie(Jieqi::立春));
  ASSERT_FALSE(is_qi(Jieqi::立春));

  ASSERT_TRUE(is_jie(Jieqi::小寒));
  ASSERT_FALSE(is_qi(Jieqi::小寒));

  ASSERT_TRUE(is_qi(Jieqi::雨水));
  ASSERT_FALSE(is_jie(Jieqi::雨水));

  ASSERT_TRUE(is_qi(Jieqi::大寒));
  ASSERT_FALSE(is_jie(Jieqi::大寒));
}


TEST(JieQi, JDE) {
  // ~10 of 234 candidate years — matches the old 4.2% filter's expectation, but the
  // empty-sample case is structurally impossible (#69).
  auto candidates = std::views::iota(1800, 2034) | std::ranges::to<std::vector>();
  std::shuffle(candidates.begin(), candidates.end(), util::detail::engine());
  const auto years = candidates | std::views::take(10) | std::ranges::to<std::vector>();

  for (const auto year : years) {
    for (int32_t jq_idx = 0; std::cmp_less(jq_idx, to_index(Jieqi::COUNT)); jq_idx++) {
      const auto jq = from_index(jq_idx);

      const auto jde = jieqi_jde(year, jq); // Use Newton's method to find the root.

      const auto jde_lon = detail::solar_longitude(jde);
      const auto expected_lon = longitude_of(jq);

      const auto lon_diff = std::fabs(std::fmod(jde_lon - expected_lon, 360.0));
      ASSERT_TRUE((lon_diff < 1e-9) or (lon_diff > 360.0 - 1e-9));
    }
  }
}

TEST(JieQi, JDEOrder) {
  const auto year = util::random(1900, 2050);
  
  const auto jdes = GREGORIAN_YEAR_JIEQI_LIST
                  | std::views::transform([&](const auto& jq) { return jieqi_jde(year, jq); })
                  | std::ranges::to<std::vector>();

  ASSERT_TRUE(std::ranges::is_sorted(jdes));

  // Ensure the JDEs are in the given year.
  for (const auto jde : jdes) {
    const auto ut1 = astro::julian_day::jde_to_ut1(jde);
    ASSERT_EQ(ut1.year(), year);
  }
}

TEST(JieQi, Generator) {
  const auto random_year = util::random(1500, 2200);
  const auto random_jq_index = util::random(0, JIEQI_COUNT - 1);
  const auto random_jq = from_index(random_jq_index);

  const auto jde = jieqi_jde(random_year, random_jq);
  const auto start_jde = util::random(-10.0, 0.0) + jde;

  auto year = random_year;
  auto jq_index = random_jq_index;

  std::vector<double> jdes;
  JieqiGenerator jieqi_gen { start_jde };

  for (auto _ = 0; _ < 360; ++_) {
    const auto [jq, jde] = jieqi_gen.next();
    ASSERT_EQ(jq, from_index(jq_index));
    ASSERT_EQ(jde, jieqi_jde(year, jq));

    // Update the Jieqi index.
    ++jq_index;
    if (std::cmp_greater_equal(jq_index, JIEQI_COUNT)) {
      jq_index = 0;
    }

    // Update the year.
    // If current Jieqi is `Jieqi::冬至`, then we know this is the last Jieqi in a Gregorian year,
    // and it's time to move to the next year.
    if (jq == Jieqi::冬至) {
      ++year;
    }

    // Finally, store the JDE.
    jdes.push_back(jde);
  }

  // Check if the JDEs are in order.
  ASSERT_TRUE(std::ranges::is_sorted(jdes));
}

} // namespace calendar::jieqi::test
