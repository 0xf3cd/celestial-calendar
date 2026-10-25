/*
 * CelestialCalendar: 
 *   A C++23-style library that performs astronomical calculations and date conversions between
 *   Gregorian and Chinese Lunar calendars.
 * 
 * Copyright (C) 2026 Ningqi Wang (0xf3cd)
 * Email: nq.maigre@gmail.com
 * Repo : https://github.com/0xf3cd/celestial-calendar
 *  
 * SPDX-License-Identifier: MIT
 */

// #67: smoke tests for the C-ABI layer — every export is driven across the real
// `extern "C"` boundary (this target links the built shared library), including the
// `valid = false` paths: bad arguments, NaN, null out-pointers, and a closed stdout.
// Retained material boundaries: V02 identifies the sparse HKO lunar-date anchor, V29 the USNO
// Julian-date anchor, and V32 the Meeus worked-example smoke anchors. Each remains under its source
// terms and outside the project MIT grant.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "celestial.h"
#include "delta_t.hpp"

#include "moon.hpp"
#include "moon_phase.hpp"
#include "sun.hpp"
#include "ymd.hpp"

#include "lunar/algo1.hpp"
#include "lunar/algo3.hpp"
#include "lunar/common.hpp"
#include "lunar/converter.hpp"

#ifdef _WIN32
  #include <io.h>
#else
  #include <unistd.h>
#endif


namespace {

#ifdef _WIN32
auto dup_fd(const int fd) -> int { return ::_dup(fd); }
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters) — mirrors `_dup2`'s own signature.
auto dup2_fd(const int from, const int to) -> int { return ::_dup2(from, to); }
auto close_fd(const int fd) -> int { return ::_close(fd); }
auto stdout_fileno() -> int { return ::_fileno(stdout); }
#else
auto dup_fd(const int fd) -> int { return ::dup(fd); }
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters) — mirrors `dup2`'s own signature.
auto dup2_fd(const int from, const int to) -> int { return ::dup2(from, to); }
auto close_fd(const int fd) -> int { return ::close(fd); }
auto stdout_fileno() -> int { return ::fileno(stdout); }
#endif

constexpr double NAN_VALUE = std::numeric_limits<double>::quiet_NaN();

// Restores stdout on destruction, so an early ASSERT return cannot leak the saved fd
// or leave stdout closed.
struct StdoutGuard {
  int saved;

  StdoutGuard()
    : saved { dup_fd(stdout_fileno()) }
  {
    if (saved >= 0) {
      close_fd(stdout_fileno());
    }
  }

  StdoutGuard(const StdoutGuard&) = delete;
  auto operator=(const StdoutGuard&) -> StdoutGuard& = delete;
  StdoutGuard(StdoutGuard&&) = delete;
  auto operator=(StdoutGuard&&) -> StdoutGuard& = delete;

  ~StdoutGuard() {
    if (saved >= 0) {
      dup2_fd(saved, stdout_fileno());
      close_fd(saved);
      std::clearerr(stdout);
    }
  }
};

} // namespace


TEST(CAbiSmoke, LibraryCallsAreSilent) {
  ::testing::internal::CaptureStdout();
  ::testing::internal::CaptureStderr();
  const auto success = celestial_ut1_to_jd(2024, 6, 1, 0.5);
  const auto failure = celestial_ut1_to_jd(2024, 6, 1, NAN_VALUE);
  const std::string error { celestial_last_error() };
  const auto kind = celestial_last_error_kind();
  const auto captured_stderr = ::testing::internal::GetCapturedStderr();
  const auto captured_stdout = ::testing::internal::GetCapturedStdout();

  EXPECT_TRUE(success.valid);
  EXPECT_FALSE(failure.valid);
  EXPECT_FALSE(error.empty());
  EXPECT_EQ(kind, CELESTIAL_ERROR_INVALID_INPUT);
  EXPECT_TRUE(captured_stdout.empty());
  EXPECT_TRUE(captured_stderr.empty());
}


TEST(CAbiSmoke, DeltaT) {
  using astro::delta_t::Model;
  // Compare runtime evaluation on both sides of the ABI, not a compiler-folded polynomial.
  const volatile double year = 2024.5;
  const std::array cases {
    std::pair { CELESTIAL_DELTA_T_MODEL_DEFAULT, Model::ALGO5 },
    std::pair { CELESTIAL_DELTA_T_MODEL_ALGO1, Model::ALGO1 },
    std::pair { CELESTIAL_DELTA_T_MODEL_ALGO2, Model::ALGO2 },
    std::pair { CELESTIAL_DELTA_T_MODEL_ALGO3, Model::ALGO3 },
    std::pair { CELESTIAL_DELTA_T_MODEL_ALGO4, Model::ALGO4 },
    std::pair { CELESTIAL_DELTA_T_MODEL_ALGO5, Model::ALGO5 },
  };
  for (const auto& [selector, model] : cases) {
    const auto result = celestial_delta_t(year, selector);
    ASSERT_TRUE(result.valid) << celestial_last_error();
    EXPECT_EQ(result.value, astro::delta_t::compute(model, year));
    EXPECT_EQ(result.model_identity_code, selector == CELESTIAL_DELTA_T_MODEL_DEFAULT
                ? CELESTIAL_DELTA_T_MODEL_ALGO5 : selector);
  }
}

TEST(CAbiSmoke, DeltaTRejectsNonFiniteYear) {
  for (const uint32_t selector : {
    CELESTIAL_DELTA_T_MODEL_DEFAULT, CELESTIAL_DELTA_T_MODEL_ALGO1, CELESTIAL_DELTA_T_MODEL_ALGO2,
    CELESTIAL_DELTA_T_MODEL_ALGO3, CELESTIAL_DELTA_T_MODEL_ALGO4, CELESTIAL_DELTA_T_MODEL_ALGO5
  }) {
    for (const double year : { NAN_VALUE, HUGE_VAL, -HUGE_VAL }) {
      EXPECT_FALSE(celestial_delta_t(year, selector).valid);
      EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
    }
  }
}

TEST(CAbiSmoke, DeltaTRejectsUnknownSelectorsAndModelDomainViolations) {
  for (const uint32_t selector : { 6U, 256U, 257U, 65537U, UINT32_C(0x10000005), UINT32_MAX }) {
    EXPECT_FALSE(celestial_delta_t(2024.5, selector).valid);
    EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
  }
  for (const auto& [year, selector] : std::array {
    std::pair { -4001.0, CELESTIAL_DELTA_T_MODEL_ALGO1 },
    std::pair { 3000.0, CELESTIAL_DELTA_T_MODEL_ALGO3 },
    std::pair { 2035.0, CELESTIAL_DELTA_T_MODEL_ALGO4 }
  }) {
    EXPECT_FALSE(celestial_delta_t(year, selector).valid);
    EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
  }
  EXPECT_TRUE(celestial_delta_t(2035.0, CELESTIAL_DELTA_T_MODEL_ALGO5).valid);
  EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_NONE);
}


TEST(CAbiSmoke, Ut1ToJd) {
  // USNO API 4.0.1: /api/juliandate?date=2024-06-01&time=12:00:00 returns
  // era=AD, tz=0, JD 2460463.000000 (`src/test/provenance/usno/2026-08-26/v29-juliandate.json`).
  const CelestialJulianDay jd = celestial_ut1_to_jd(2024, 6, 1, 0.5);
  ASSERT_TRUE(jd.valid);
  ASSERT_NEAR(jd.value, 2460463.0, 1e-6);
}

TEST(CAbiSmoke, Ut1ToJdRejectsBadInput) {
  EXPECT_FALSE(celestial_ut1_to_jd(2024, 6, 1, 1.5).valid);        // fraction >= 1
  EXPECT_FALSE(celestial_ut1_to_jd(2024, 6, 1, -0.1).valid);       // fraction < 0
  EXPECT_FALSE(celestial_ut1_to_jd(2024, 6, 1, NAN_VALUE).valid);  // NaN slips past `<`-style checks
  EXPECT_FALSE(celestial_ut1_to_jd(2024, 13, 1, 0.5).valid);       // invalid month
  EXPECT_FALSE(celestial_ut1_to_jd(0, 6, 1, 0.5).valid);           // year < 1
}

TEST(CAbiSmoke, RawDateExportsRejectOriginalMonthAndDay) {
  for (const uint32_t value : { uint32_t { 0 }, uint32_t { 257 }, UINT32_MAX }) {
    EXPECT_FALSE(celestial_ut1_to_jd(2024, value, 1, 0.0).valid);
    EXPECT_STRNE(celestial_last_error(), "");
    EXPECT_FALSE(celestial_ut1_to_jd(2024, 1, value, 0.0).valid);
    EXPECT_STRNE(celestial_last_error(), "");
    EXPECT_FALSE(celestial_ut1_to_jde(2024, value, 1, 0.0).valid);
    EXPECT_FALSE(celestial_ut1_to_jde(2024, 1, value, 0.0).valid);
    EXPECT_FALSE(celestial_apparent_solar_time(2024, value, 1, 0.0, 0.0).valid);
    EXPECT_FALSE(celestial_apparent_solar_time(2024, 1, value, 0.0, 0.0).valid);
    ASSERT_TRUE(celestial_ut1_to_jd(2024, 2, 29, 0.0).valid);
    EXPECT_STREQ(celestial_last_error(), "");
  }
  EXPECT_FALSE(celestial_ut1_to_jd(2023, 2, 29, 0.0).valid);
  EXPECT_FALSE(celestial_ut1_to_jd(2024, 4, 31, 0.0).valid);
  EXPECT_FALSE(celestial_ut1_to_jd(2024, 257, 257, 0.0).valid);
  ASSERT_TRUE(celestial_ut1_to_jd(2024, 2, 29, 0.0).valid);
  EXPECT_STREQ(celestial_last_error(), "");
}

TEST(CAbiSmoke, RawDateExportsRejectUnrepresentableYears) {
  // The recorded error must name the original integer year, not just flag `valid = false`.
  for (const int32_t year : { 65537, 67562, 67421, -63510, INT32_MIN, INT32_MAX }) {
    const std::string original = std::to_string(year);

    EXPECT_FALSE(celestial_ut1_to_jd(year, 1, 1, 0.0).valid);
    EXPECT_NE(std::strstr(celestial_last_error(), original.c_str()), nullptr);
    EXPECT_NE(std::strstr(celestial_last_error(), "std::chrono::year"), nullptr);

    EXPECT_FALSE(celestial_ut1_to_jde(year, 1, 1, 0.5).valid);
    EXPECT_NE(std::strstr(celestial_last_error(), original.c_str()), nullptr);

    EXPECT_FALSE(celestial_apparent_solar_time(year, 6, 1, 0.5, 116.4).valid);
    EXPECT_NE(std::strstr(celestial_last_error(), original.c_str()), nullptr);

    EXPECT_FALSE(celestial_gregorian_to_lunar(1, year, 2, 17).valid);
    EXPECT_NE(std::strstr(celestial_last_error(), original.c_str()), nullptr);
  }

  // Unrelated precedence is preserved: the algorithm is still checked before the year.
  EXPECT_FALSE(celestial_gregorian_to_lunar(9, 67562, 2, 17).valid);
  EXPECT_NE(std::strstr(celestial_last_error(), "Unsupported algorithm"), nullptr);

  // Recovery: a valid call succeeds and clears the recorded error.
  ASSERT_TRUE(celestial_ut1_to_jd(2024, 6, 1, 0.5).valid);
  EXPECT_STREQ(celestial_last_error(), "");
}

TEST(CAbiSmoke, ScalarDateExportsKeepYear32767) {
  EXPECT_TRUE(celestial_ut1_to_jd(32767, 12, 31, 0.5).valid);
  EXPECT_STREQ(celestial_last_error(), "");
  EXPECT_TRUE(celestial_ut1_to_jde(32767, 6, 1, 0.5).valid);
  EXPECT_TRUE(celestial_apparent_solar_time(32767, 6, 1, 0.5, 116.4).valid);
}

TEST(CAbiSmoke, LunarConverterGregorianCoverageDomain) {
  // These years pass the chrono representation guard, so the rejection must come from the
  // selected algorithm's Gregorian coverage — not the `std::chrono::year` message.
  for (const int32_t year : { -32767, 0, 32767 }) {
    EXPECT_FALSE(celestial_gregorian_to_lunar(1, year, 1, 1).valid);
    EXPECT_NE(std::strstr(celestial_last_error(), "cannot be represented by the selected algorithm"), nullptr);
    EXPECT_EQ(std::strstr(celestial_last_error(), "std::chrono::year"), nullptr);
  }

  // Algorithm 1's Gregorian span closes on lunar 2099's last day, so 2100-01-01 still converts.
  const CelestialLunarDate date = celestial_gregorian_to_lunar(1, 2100, 1, 1);
  ASSERT_TRUE(date.valid);
  EXPECT_EQ(date.year, 2099);
  EXPECT_STREQ(celestial_last_error(), ""); // a valid call clears the recorded error
}


TEST(CAbiSmoke, Ut1ToJde) {
  const CelestialJulianDay jde = celestial_ut1_to_jde(2024, 6, 1, 0.5);
  ASSERT_TRUE(jde.valid);
  // JDE is JD plus ΔT (~69 s ≈ 8e-4 day), so it must sit above the plain JD.
  EXPECT_GT(jde.value, 2460463.0);
}

TEST(CAbiSmoke, JdeToUt1) {
  const CelestialUT1Time ut1 = celestial_jde_to_ut1(2460463.0);
  ASSERT_TRUE(ut1.valid);
  // ΔT ≈ 69 s cannot flip the civil date of a noon moment.
  EXPECT_EQ(ut1.year, 2024);
  EXPECT_EQ(ut1.month, 6U);
  EXPECT_EQ(ut1.day, 1U);
}

TEST(CAbiSmoke, JdeToUt1RejectsBadInput) {
  EXPECT_FALSE(celestial_jde_to_ut1(NAN_VALUE).valid);  // non-finite
  EXPECT_FALSE(celestial_jde_to_ut1(1000.0).valid);     // gregorian year < 401
  EXPECT_FALSE(celestial_jde_to_ut1(13689325.5).valid); // first JD beyond the representable years
  EXPECT_FALSE(celestial_jde_to_ut1(4.0e9).valid);      // far beyond the representable years
}


TEST(CAbiSmoke, LastErrorRecordsAndClears) {
  ASSERT_FALSE(celestial_ut1_to_jd(2024, 6, 1, NAN_VALUE).valid);
  EXPECT_NE(std::strstr(celestial_last_error(), "fraction"), nullptr);
  const char* const pointer = celestial_last_error();
  const std::string message { pointer };
  EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
  EXPECT_EQ(celestial_last_error(), pointer);
  EXPECT_EQ(std::string { celestial_last_error() }, message);
  EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);

  ASSERT_FALSE(celestial_jde_to_ut1(NAN_VALUE).valid);
  EXPECT_NE(std::strstr(celestial_last_error(), "not finite"), nullptr);

  ASSERT_TRUE(celestial_ut1_to_jd(2024, 6, 1, 0.5).valid);
  EXPECT_STREQ(celestial_last_error(), "");
  EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_NONE);
}


TEST(CAbiSmoke, SunMoonCoords) {
  const CelestialSunCoordinate sun = celestial_sun_apparent_geocentric_coord(2460463.0);
  ASSERT_TRUE(sun.valid);
  EXPECT_GE(sun.lon, 0.0);
  EXPECT_LT(sun.lon, 360.0);

  const CelestialMoonCoordinate moon = celestial_moon_apparent_geocentric_coord(2460463.0);
  ASSERT_TRUE(moon.valid);
  EXPECT_GE(moon.lon, 0.0);
  EXPECT_LT(moon.lon, 360.0);

  EXPECT_FALSE(celestial_sun_apparent_geocentric_coord(NAN_VALUE).valid);  // non-finite JDE
  EXPECT_FALSE(celestial_sun_apparent_geocentric_coord(HUGE_VAL).valid);
  EXPECT_FALSE(celestial_moon_apparent_geocentric_coord(NAN_VALUE).valid);
  EXPECT_FALSE(celestial_moon_apparent_geocentric_coord(HUGE_VAL).valid);
  EXPECT_STRNE(celestial_last_error(), "");
}


TEST(CAbiSmoke, MoonIllumination) {
  // Meeus Example 48.a's book values are pinned in moon_phase_test; the C struct must
  // equal the C++ call chain at the same instant (1992-04-12 0h TT).
  const CelestialMoonIllumination mi = celestial_moon_illumination(2448724.5);
  ASSERT_TRUE(mi.valid);
  const auto sun_pos = astro::sun::geocentric_coord::apparent(2448724.5);
  const auto moon_pos = astro::moon::geocentric_coord::apparent(2448724.5);
  EXPECT_EQ(mi.illumination, astro::moon_phase::illumination::fraction(
    astro::moon_phase::illumination::phase_angle(sun_pos, moon_pos)
  ));
  EXPECT_EQ(mi.elongation_deg, (moon_pos.λ - sun_pos.λ).normalize().deg());

  EXPECT_FALSE(celestial_moon_illumination(NAN_VALUE).valid);  // non-finite JDE
  EXPECT_NE(std::strstr(celestial_last_error(), "not finite"), nullptr);
  EXPECT_FALSE(celestial_moon_illumination(HUGE_VAL).valid);
}


TEST(CAbiSmoke, MoonPositionAngle) {
  // Meeus Example 48.a's χ = 285°.0 is pinned in moon_phase_test; here the C struct must
  // equal the C++ call at the same instant (1992-04-12 0h TT).
  const CelestialMoonPositionAngle pa = celestial_moon_position_angle(2448724.5);
  ASSERT_TRUE(pa.valid);
  EXPECT_EQ(pa.angle_deg, astro::moon_phase::illumination::position_angle(2448724.5).deg());

  EXPECT_FALSE(celestial_moon_position_angle(NAN_VALUE).valid);  // non-finite JDE
  EXPECT_NE(std::strstr(celestial_last_error(), "not finite"), nullptr);
  EXPECT_FALSE(celestial_moon_position_angle(HUGE_VAL).valid);
}


TEST(CAbiSmoke, MoonPhaseMoments) {
  uint32_t root_count = 0;
  std::array<double, 15> slots {};
  const uint32_t written = celestial_moon_phase_moments(2024, 0, &root_count, slots.data(), slots.size());
  EXPECT_EQ(written, root_count);
  EXPECT_TRUE(root_count == 12U or root_count == 13U);

  // First new moon of 2024: 2024-01-11 11:57 UTC -> JDE ~2460320.998.
  EXPECT_NEAR(slots.at(0), 2460320.998, 0.002);

  // Other phases should also succeed.
  EXPECT_GT(celestial_moon_phase_moments(2024, 1, &root_count, slots.data(), slots.size()), 0U);
  EXPECT_GT(celestial_moon_phase_moments(2024, 2, &root_count, slots.data(), slots.size()), 0U);
  EXPECT_GT(celestial_moon_phase_moments(2024, 3, &root_count, slots.data(), slots.size()), 0U);

  // Failure paths must record a diagnostic and reset the out-parameter to a
  // deterministic value (Codex P2: stale root_count and missing celestial_last_error).
  uint32_t sentinel = 0xDEADBEEFU;

  EXPECT_EQ(celestial_moon_phase_moments(2024, 0, nullptr, slots.data(), slots.size()), 0U); // null root_count
  EXPECT_STRNE(celestial_last_error(), "");

  EXPECT_EQ(celestial_moon_phase_moments(2024, 0, &root_count, nullptr, 15), 0U); // null slots
  EXPECT_STRNE(celestial_last_error(), "");

  sentinel = 0xDEADBEEFU;
  EXPECT_EQ(celestial_moon_phase_moments(2024, 4, &sentinel, slots.data(), slots.size()), 0U); // bad phase_kind
  EXPECT_EQ(sentinel, 0U);
  EXPECT_STRNE(celestial_last_error(), "");

  sentinel = 0xDEADBEEFU;
  EXPECT_EQ(celestial_moon_phase_moments(0, 0, &sentinel, slots.data(), slots.size()), 0U); // year below range
  EXPECT_EQ(sentinel, 0U);
  EXPECT_STRNE(celestial_last_error(), "");

  sentinel = 0xDEADBEEFU;
  EXPECT_EQ(celestial_moon_phase_moments(32767, 0, &sentinel, slots.data(), slots.size()), 0U); // year above range
  EXPECT_EQ(sentinel, 0U);
  EXPECT_STRNE(celestial_last_error(), "");

  sentinel = 0xDEADBEEFU;
  EXPECT_EQ(
    celestial_moon_phase_moments(std::numeric_limits<int32_t>::max(), 0, &sentinel, slots.data(), slots.size()),
    0U
  ); // would overflow year + 1
  EXPECT_EQ(sentinel, 0U);
  EXPECT_STRNE(celestial_last_error(), "");

  sentinel = 0xDEADBEEFU;
  EXPECT_EQ(celestial_moon_phase_moments(65537, 0, &sentinel, slots.data(), slots.size()), 0U); // would alias year 1
  EXPECT_EQ(sentinel, 0U);
  EXPECT_NE(std::strstr(celestial_last_error(), "65537"), nullptr); // the diagnostic names the original year
}


TEST(CAbiSmoke, LocalApparentSiderealTime) {
  const CelestialSiderealTime last = celestial_local_apparent_sidereal_time(2460463.0, 120.0);
  ASSERT_TRUE(last.valid);
  EXPECT_GE(last.value, 0.0);
  EXPECT_LT(last.value, 360.0);
  EXPECT_STREQ(celestial_last_error(), ""); // a valid call clears the message

  // Outside the declared [401, 32766] window: valid = false, and the reason is
  // readable — the widened celestial_last_error boundary (#97). jd 1000000 ≈ year -1975 is
  // below the floor; 13689294.5 is 32767-12-01, above the ceiling.
  EXPECT_FALSE(celestial_local_apparent_sidereal_time(1000000.0, 0.0).valid);
  EXPECT_STRNE(celestial_last_error(), "");
  EXPECT_FALSE(celestial_local_apparent_sidereal_time(13689294.5, 0.0).valid);
  EXPECT_NE(std::strstr(celestial_last_error(), "32766"), nullptr);

  EXPECT_FALSE(celestial_local_apparent_sidereal_time(NAN_VALUE, 0.0).valid);  // non-finite jd
  EXPECT_NE(std::strstr(celestial_last_error(), "not finite"), nullptr);
  EXPECT_FALSE(celestial_local_apparent_sidereal_time(2460463.0, NAN_VALUE).valid);  // non-finite longitude
  EXPECT_FALSE(celestial_local_apparent_sidereal_time(2460463.0, 999.0).valid);      // out of [-180, 180]
}


TEST(CAbiSmoke, SolarLonRoots) {
  const CelestialDiscriminant disc = celestial_solar_lon_root_discriminant(2024, 0.0);
  ASSERT_TRUE(disc.valid);
  ASSERT_EQ(disc.count, 1U); // The Sun crosses longitude 0° (春分) once a year.

  std::array<double, 2> slots {};
  EXPECT_EQ(celestial_solar_lon_roots(2024, 0.0, slots.data(), slots.size()), 1U);
  EXPECT_EQ(celestial_solar_lon_roots(2024, 0.0, nullptr, 2), 0U); // null out-pointer

  EXPECT_FALSE(celestial_solar_lon_root_discriminant(2024, NAN_VALUE).valid); // non-finite longitude
  EXPECT_FALSE(celestial_solar_lon_root_discriminant(2024, HUGE_VAL).valid);
  EXPECT_EQ(celestial_solar_lon_roots(2024, NAN_VALUE, slots.data(), slots.size()), 0U);
  EXPECT_EQ(celestial_solar_lon_roots(2024, HUGE_VAL, slots.data(), slots.size()), 0U);
  EXPECT_STRNE(celestial_last_error(), "");
}

TEST(CAbiSmoke, SolarLonRootsRejectOutOfWindowYears) {
  // The window rule and the message wording are pinned in sun_test.cpp; here the C layer
  // must agree with the core calls on rejection, and leave the caller's slots untouched.
  using astro::sun::geocentric_coord::math::discriminant;
  using astro::sun::geocentric_coord::math::find_roots;
  for (const int32_t year : { 0, 32767, 65537, 67562, INT32_MIN, INT32_MAX }) {
    EXPECT_FALSE(celestial_solar_lon_root_discriminant(year, 0.0).valid);
    EXPECT_STRNE(celestial_last_error(), "");
    EXPECT_ANY_THROW(std::ignore = discriminant(year, 0.0));

    std::array<double, 2> slots { -777.0, -777.0 };
    EXPECT_EQ(celestial_solar_lon_roots(year, 0.0, slots.data(), slots.size()), 0U);
    EXPECT_EQ(slots.at(0), -777.0); // a rejected query leaves the caller's slots untouched
    EXPECT_EQ(slots.at(1), -777.0);
    EXPECT_ANY_THROW(std::ignore = find_roots(year, 0.0));
  }

  // The window ceiling itself still computes, and clears the recorded error.
  const CelestialDiscriminant edge = celestial_solar_lon_root_discriminant(32766, 0.0);
  ASSERT_TRUE(edge.valid);
  EXPECT_EQ(edge.count, 1U);
  EXPECT_EQ(edge.count, discriminant(32766, 0.0));
  EXPECT_STREQ(celestial_last_error(), "");
}


TEST(CAbiSmoke, NewMoons) {
  std::array<double, 3> after {};
  ASSERT_EQ(celestial_new_moons_after_jde(2460463.0, after.data(), after.size()), 3U);
  EXPECT_LT(after.at(0), after.at(1));
  EXPECT_LT(after.at(1), after.at(2));
  EXPECT_EQ(celestial_new_moons_after_jde(2460463.0, nullptr, 3), 0U); // null out-pointer
  EXPECT_EQ(celestial_new_moons_after_jde(NAN_VALUE, after.data(), after.size()), 0U); // non-finite JDE
  EXPECT_EQ(celestial_new_moons_after_jde(HUGE_VAL, after.data(), after.size()), 0U);

  uint32_t root_count = 0;
  std::array<double, 15> slots {};
  const uint32_t written = celestial_new_moons_in_year(2024, &root_count, slots.data(), slots.size());
  EXPECT_EQ(written, root_count);
  EXPECT_TRUE(root_count == 12U or root_count == 13U);

  // Failure paths must reset root_count to a deterministic value and record the reason.
  uint32_t sentinel = 0xDEADBEEFU;
  EXPECT_EQ(celestial_new_moons_in_year(2024, nullptr, slots.data(), slots.size()), 0U); // null root_count

  EXPECT_EQ(celestial_new_moons_in_year(2024, &root_count, nullptr, 15), 0U); // null slots

  sentinel = 0xDEADBEEFU;
  EXPECT_EQ(celestial_new_moons_in_year(0, &sentinel, slots.data(), slots.size()), 0U); // year below range
  EXPECT_EQ(sentinel, 0U);

  sentinel = 0xDEADBEEFU;
  EXPECT_EQ(celestial_new_moons_in_year(32767, &sentinel, slots.data(), slots.size()), 0U); // year above range
  EXPECT_EQ(sentinel, 0U);
  EXPECT_STRNE(celestial_last_error(), "");

  sentinel = 0xDEADBEEFU;
  EXPECT_EQ(celestial_new_moons_in_year(67562, &sentinel, slots.data(), slots.size()), 0U); // would alias year 2026
  EXPECT_EQ(sentinel, 0U);
  EXPECT_NE(std::strstr(celestial_last_error(), "67562"), nullptr); // the diagnostic names the original year
}


TEST(CAbiSmoke, EquationOfTime) {
  const CelestialEquationOfTime e = celestial_equation_of_time(2460463.0);
  ASSERT_TRUE(e.valid);
  EXPECT_LT(std::fabs(e.value), 5.0); // |E| stays under 5° (Meeus ch. 28).

  EXPECT_FALSE(celestial_equation_of_time(NAN_VALUE).valid); // non-finite JDE
  EXPECT_STRNE(celestial_last_error(), "");
}


TEST(CAbiSmoke, ApparentSolarTime) {
  // UTC noon at 116.4°E: apparent time ≈ noon + longitude-in-time ± |E| (< 5°).
  const CelestialApparentSolarTime t = celestial_apparent_solar_time(2024, 6, 1, 0.5, 116.4);
  ASSERT_TRUE(t.valid);
  EXPECT_EQ(t.year, 2024);
  EXPECT_EQ(t.month, 6U);
  EXPECT_EQ(t.day, 1U);
  EXPECT_GT(t.fraction, 0.5 + (111.4 / 360.0));
  EXPECT_LT(t.fraction, 0.5 + (121.4 / 360.0));

  const auto previous_year = celestial_apparent_solar_time(1, 1, 1, 0.0, -180.0);
  ASSERT_TRUE(previous_year.valid);
  EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_NONE);
  EXPECT_EQ(previous_year.year, 0);
  EXPECT_EQ(previous_year.month, 12U);
  EXPECT_EQ(previous_year.day, 31U);

  EXPECT_FALSE(celestial_apparent_solar_time(2024, 6, 1, 0.5, 200.0).valid);     // longitude out of range
  EXPECT_FALSE(celestial_apparent_solar_time(2024, 6, 1, 0.5, NAN_VALUE).valid); // NaN longitude
  EXPECT_FALSE(celestial_apparent_solar_time(2024, 6, 1, NAN_VALUE, 116.4).valid); // NaN fraction
  EXPECT_STRNE(celestial_last_error(), "");
}


TEST(CAbiSmoke, Jieqi) {
  const CelestialJieqiMomentQuery query = celestial_query_jieqi_moment(2024, 0); // 立春
  ASSERT_TRUE(query.valid);
  EXPECT_EQ(query.jq_idx, 0U);
  EXPECT_EQ(query.y, 2024);
  EXPECT_EQ(query.m, 2U);
  EXPECT_FALSE(celestial_query_jieqi_moment(2024, 24).valid); // jq_idx out of range

  std::array<char, 32> buf {};
  ASSERT_TRUE(celestial_get_jieqi_name(0, buf.data(), buf.size()));
  EXPECT_STREQ(buf.data(), "立春");
  EXPECT_EQ(celestial_get_jieqi_name(0, buf.data(), 2), 7U);
  EXPECT_EQ(buf.at(1), '\0');
  EXPECT_FALSE(celestial_get_jieqi_name(0, nullptr, buf.size())); // null out-pointer
  EXPECT_FALSE(celestial_get_jieqi_name(24, buf.data(), buf.size()));
  EXPECT_STRNE(celestial_last_error(), "");
}


TEST(CAbiSmoke, Lunar) {
  const CelestialSupportedLunarYearRange range1 = celestial_get_supported_lunar_year_range(1);
  ASSERT_TRUE(range1.valid);
  EXPECT_LT(range1.start, range1.end);

  const CelestialSupportedLunarYearRange range2 = celestial_get_supported_lunar_year_range(2);
  ASSERT_TRUE(range2.valid);
  EXPECT_LT(range2.start, range2.end);

  // #128: algo=3 is now exported; its window comes from the C++ constants, not re-spelled here.
  const CelestialSupportedLunarYearRange range3 = celestial_get_supported_lunar_year_range(3);
  ASSERT_TRUE(range3.valid);
  EXPECT_EQ(range3.start, calendar::lunar::algo3::START_YEAR);
  EXPECT_EQ(range3.end, calendar::lunar::algo3::END_YEAR);

  EXPECT_FALSE(celestial_get_supported_lunar_year_range(0).valid);
  EXPECT_FALSE(celestial_get_supported_lunar_year_range(4).valid);

  EXPECT_TRUE(celestial_get_lunar_year_info(2, 2024).valid);
  EXPECT_FALSE(celestial_get_lunar_year_info(9, 2024).valid); // unsupported algorithm

  // #70: the advertised window binds. Read the endpoints back from the ABI rather than spelling
  // them again — the point is that the window reported and the window honoured are one thing.
  EXPECT_TRUE(celestial_get_lunar_year_info(1, range1.start).valid);
  EXPECT_TRUE(celestial_get_lunar_year_info(1, range1.end).valid);
  EXPECT_FALSE(celestial_get_lunar_year_info(1, range1.start - 1).valid);
  EXPECT_FALSE(celestial_get_lunar_year_info(1, range1.end + 1).valid);

  EXPECT_TRUE(celestial_get_lunar_year_info(2, range2.start).valid);
  EXPECT_TRUE(celestial_get_lunar_year_info(2, range2.end).valid);
  EXPECT_FALSE(celestial_get_lunar_year_info(2, range2.start - 1).valid);
  EXPECT_FALSE(celestial_get_lunar_year_info(2, range2.end + 1).valid);

  EXPECT_TRUE(celestial_get_lunar_year_info(3, range3.start).valid);
  EXPECT_TRUE(celestial_get_lunar_year_info(3, range3.end).valid);
  EXPECT_FALSE(celestial_get_lunar_year_info(3, range3.start - 1).valid);
  EXPECT_FALSE(celestial_get_lunar_year_info(3, range3.end + 1).valid);
  EXPECT_STRNE(celestial_last_error(), "");
}


namespace {

// Parity helpers for LunarConverter below. The C structs speak traditional numbering + leap
// flag (celestial.h); the converter core speaks positional month indices (#128). The
// translation rules are pinned in lunar/common_test (MonthPosition*/MonthAtPosition); the
// 2023 rows' value correctness lives in the registered year-row blocks (the retained HKO
// encoding), verified against the HKO reconstruction by automation/hko_identity.py and
// cross-checked between the two algorithms in LunarAlgo3.HkoTableCopiesAgree —
// converter_test's per-day enumeration covers converter/table consistency, not the values —
// so here the dates are inputs only, and the C result must equal the C++ call.
// The C boundary turns every core throw into `valid = false` (wrap_export contract), so a
// throwing core call is the C++ face of the same rejection and maps to std::nullopt here.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters) -- y/m/d is the natural date order, as in every neighbour.
template <calendar::lunar::common::Algo algo>
auto core_gregorian_to_lunar(const int32_t year, const uint32_t month, const uint32_t day) -> std::optional<CelestialLunarDate> {
  try {
    const auto converted = calendar::lunar::converter::Converter<algo>::gregorian_to_lunar(
      util::to_ymd(util::checked_year(year), month, day)
    );
    if (not converted) {
      return std::nullopt;
    }
    const auto [y, m, d] = util::from_ymd(*converted);
    const auto& info = calendar::lunar::common::AlgoMetadata<algo>::get_info_for_year(y);
    const auto traditional = calendar::lunar::common::month_at_position(info, static_cast<uint8_t>(m));
    if (not traditional) {
      return std::nullopt;
    }
    return CelestialLunarDate {
      .valid   = true,
      .year    = y,
      .month   = traditional->month,
      .is_leap = traditional->is_leap,
      .day     = static_cast<uint8_t>(d),
    };
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

template <calendar::lunar::common::Algo algo>
auto core_lunar_to_gregorian(
  const int32_t year, // NOLINT(bugprone-easily-swappable-parameters) -- y/m/d(+leap) is the library's date order.
  const uint32_t month,
  const bool is_leap,
  const uint32_t day
) -> std::optional<CelestialGregorianDate> {
  try {
    const auto& info = calendar::lunar::common::AlgoMetadata<algo>::get_info_for_year(year);
    const auto position = calendar::lunar::common::month_position(info, static_cast<uint8_t>(month), is_leap);
    if (not position) {
      return std::nullopt;
    }
    const auto converted = calendar::lunar::converter::Converter<algo>::lunar_to_gregorian(util::to_ymd(year, *position, day));
    if (not converted) {
      return std::nullopt;
    }
    const auto [y, m, d] = util::from_ymd(*converted);
    return CelestialGregorianDate { .valid = true, .year = y, .month = static_cast<uint8_t>(m), .day = static_cast<uint8_t>(d) };
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

auto core_gregorian_to_lunar_for(
  const uint8_t algo, // NOLINT(bugprone-easily-swappable-parameters) -- algo/y/m/d follows the C entry point.
  const int32_t year,
  const uint32_t month,
  const uint32_t day
) -> std::optional<CelestialLunarDate> {
  switch (algo) {
    case 1: return core_gregorian_to_lunar<calendar::lunar::common::Algo::ALGO_1>(year, month, day);
    case 3: return core_gregorian_to_lunar<calendar::lunar::common::Algo::ALGO_3>(year, month, day);
    default: throw std::logic_error { "test dispatch covers algo 1 and 3 only" };
  }
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters) -- y/m/d(+leap) is the natural date order, as in the library.
auto core_lunar_to_gregorian_for(const uint8_t algo, const int32_t year, const uint32_t month, const bool is_leap,
                                 const uint32_t day) -> std::optional<CelestialGregorianDate> {
  switch (algo) {
    case 1: return core_lunar_to_gregorian<calendar::lunar::common::Algo::ALGO_1>(year, month, is_leap, day);
    case 3: return core_lunar_to_gregorian<calendar::lunar::common::Algo::ALGO_3>(year, month, is_leap, day);
    default: throw std::logic_error { "test dispatch covers algo 1 and 3 only" };
  }
}

auto expect_lunar_parity(const CelestialLunarDate& actual, const std::optional<CelestialLunarDate>& expected) -> void {
  ASSERT_EQ(actual.valid, expected.has_value());
  if (expected.has_value()) {
    EXPECT_EQ(actual.year, expected->year);
    EXPECT_EQ(actual.month, expected->month);
    EXPECT_EQ(actual.is_leap, expected->is_leap);
    EXPECT_EQ(actual.day, expected->day);
  }
}

auto expect_gregorian_parity(const CelestialGregorianDate& actual, const std::optional<CelestialGregorianDate>& expected) -> void {
  ASSERT_EQ(actual.valid, expected.has_value());
  if (expected.has_value()) {
    EXPECT_EQ(actual.year, expected->year);
    EXPECT_EQ(actual.month, expected->month);
    EXPECT_EQ(actual.day, expected->day);
  }
}

} // namespace

// 2023's leap 2nd month (闰二月): 闰二月初一 = 2023-03-22, 三月初一 = 2023-04-20 — the HKO rows
// are pinned in the C++ suites; these dates are inputs only, and the C structs must equal
// the C++ converter calls.
TEST(CAbiSmoke, LunarConverter) {
  for (const uint8_t algo : { 1U, 3U }) {
    expect_lunar_parity(celestial_gregorian_to_lunar(algo, 2023, 3, 22), core_gregorian_to_lunar_for(algo, 2023, 3, 22));
    expect_lunar_parity(celestial_gregorian_to_lunar(algo, 2023, 4, 20), core_gregorian_to_lunar_for(algo, 2023, 4, 20));
    expect_gregorian_parity(celestial_lunar_to_gregorian(algo, 2023, 2, true, 1),
                            core_lunar_to_gregorian_for(algo, 2023, 2, true, 1));
    expect_gregorian_parity(celestial_lunar_to_gregorian(algo, 2023, 3, false, 1),
                            core_lunar_to_gregorian_for(algo, 2023, 3, false, 1));
  }

  EXPECT_FALSE(celestial_gregorian_to_lunar(9, 2023, 3, 22).valid);
  // The accepted Gregorian window does not line up with Gregorian years 1901-2099: it opens on
  // lunar 1901's first day (1901-02-19) and closes on lunar 2099's last day (2100-02-08).
  // (These edges are algo 1's; algo 3's window is [1600, 2199] and is exercised by the
  // definition-side constants and the C++ suites, not repeated here.)
  expect_lunar_parity(celestial_gregorian_to_lunar(1, 1901, 1, 1), core_gregorian_to_lunar_for(1, 1901, 1, 1));
  expect_lunar_parity(celestial_gregorian_to_lunar(1, 1901, 2, 18), core_gregorian_to_lunar_for(1, 1901, 2, 18));
  expect_lunar_parity(celestial_gregorian_to_lunar(1, 1901, 2, 19), core_gregorian_to_lunar_for(1, 1901, 2, 19));
  expect_lunar_parity(celestial_gregorian_to_lunar(1, 2100, 1, 25), core_gregorian_to_lunar_for(1, 2100, 1, 25));
  expect_lunar_parity(celestial_gregorian_to_lunar(1, 2100, 2, 8), core_gregorian_to_lunar_for(1, 2100, 2, 8));
  expect_lunar_parity(celestial_gregorian_to_lunar(1, 2100, 2, 9), core_gregorian_to_lunar_for(1, 2100, 2, 9));
  expect_lunar_parity(celestial_gregorian_to_lunar(1, 2101, 1, 1), core_gregorian_to_lunar_for(1, 2101, 1, 1));
  expect_lunar_parity(celestial_gregorian_to_lunar(1, 2023, 13, 1), core_gregorian_to_lunar_for(1, 2023, 13, 1));
  for (const uint8_t algo : { 1U, 3U }) {
    // lunar year 2100 is outside algo 1's window ([1901, 2099]) but inside algo 3's
    // ([1600, 2199]) — the two layers must agree on which one rejects it.
    expect_gregorian_parity(celestial_lunar_to_gregorian(algo, 2100, 1, false, 1),
                            core_lunar_to_gregorian_for(algo, 2100, 1, false, 1));
    expect_gregorian_parity(celestial_lunar_to_gregorian(algo, 2024, 2, true, 1),
                            core_lunar_to_gregorian_for(algo, 2024, 2, true, 1)); // 2024 has no leap month
    expect_gregorian_parity(celestial_lunar_to_gregorian(algo, 2023, 5, true, 1),
                            core_lunar_to_gregorian_for(algo, 2023, 5, true, 1)); // 2023's leap month is the 2nd
    expect_gregorian_parity(celestial_lunar_to_gregorian(algo, 2023, 2, true, 30),
                            core_lunar_to_gregorian_for(algo, 2023, 2, true, 30)); // 闰二月 has 29 days
  }
  EXPECT_FALSE(celestial_lunar_to_gregorian(9, 2023, 2, true, 1).valid);
  EXPECT_STRNE(celestial_last_error(), "");
}


TEST(CAbiSmoke, CallsSurviveClosedStdout) {
  ASSERT_EQ(std::fflush(stdout), 0);
  bool saved_stdout = false;
  bool success = false;
  bool failure = false;
  uint32_t kind = CELESTIAL_ERROR_NONE;
  {
    const StdoutGuard guard {};
    saved_stdout = guard.saved >= 0;
    if (saved_stdout) {
      success = celestial_ut1_to_jd(2024, 6, 1, 0.5).valid;
      failure = not celestial_ut1_to_jd(2024, 6, 1, NAN_VALUE).valid;
      kind = celestial_last_error_kind();
    }
  }

  ASSERT_TRUE(saved_stdout);
  EXPECT_TRUE(success);
  EXPECT_TRUE(failure);
  EXPECT_EQ(kind, CELESTIAL_ERROR_INVALID_INPUT);
}

TEST(CAbiSmoke, EveryJieqiNameUsesTheSizedUtf8Protocol) {
  constexpr std::array<std::string_view, 24> NAMES {
    "立春", "雨水", "惊蛰", "春分", "清明", "谷雨", "立夏", "小满",
    "芒种", "夏至", "小暑", "大暑", "立秋", "处暑", "白露", "秋分",
    "寒露", "霜降", "立冬", "小雪", "大雪", "冬至", "小寒", "大寒",
  };
  uint32_t maximum = 0;
  for (std::size_t index = 0; index < NAMES.size(); ++index) {
    const auto code = static_cast<uint8_t>(index);
    const auto required = celestial_get_jieqi_name(code, nullptr, 0);
    ASSERT_EQ(required, NAMES.at(index).size() + 1);
    EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_NONE);
    maximum = std::max(maximum, required);

    std::vector<char> buffer(required + 1, '!');
    EXPECT_EQ(celestial_get_jieqi_name(code, buffer.data(), required), required);
    EXPECT_EQ(std::string_view { buffer.data() }, NAMES.at(index));
    EXPECT_EQ(buffer.at(required - 1), '\0');
    EXPECT_EQ(buffer.at(required), '!');
    for (const uint32_t capacity : { 1U, 2U, 6U }) {
      buffer.assign(required + 1, '!');
      EXPECT_EQ(celestial_get_jieqi_name(code, buffer.data(), capacity), required);
      EXPECT_EQ(buffer.at(capacity - 1), '\0');
      EXPECT_EQ(buffer.at(capacity), '!');
      EXPECT_EQ(std::string_view(buffer.data(), capacity - 1), NAMES.at(index).substr(0, capacity - 1));
    }
    EXPECT_EQ(celestial_get_jieqi_name(code, nullptr, 1), 0U);
    EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
  }
  EXPECT_EQ(maximum, 7U);
  EXPECT_EQ(celestial_get_jieqi_name(24, nullptr, 0), 0U);
  EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
}

TEST(CAbiSmoke, SolarLonRootsRejectOutOfRangeLongitudes) {
  for (const double longitude : { -1.0, 360.0, 720.0 }) {
    EXPECT_FALSE(celestial_solar_lon_root_discriminant(2024, longitude).valid);
    EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
    std::array<double, 2> slots { -777.0, -777.0 };
    EXPECT_EQ(celestial_solar_lon_roots(2024, longitude, slots.data(), slots.size()), 0U);
    EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
    EXPECT_EQ(slots.at(0), -777.0);
    EXPECT_EQ(slots.at(1), -777.0);
  }
}
