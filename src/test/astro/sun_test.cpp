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
#include <array>
#include <cmath>
#include <chrono>
#include <bit>
#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>
#include "ymd.hpp"
#include "../random.hpp"
#include "astro.hpp"
#include "sun_test_data.hpp"

namespace astro::sun::test {

using namespace std::chrono;
using namespace std::literals;

using namespace astro::sun::geocentric_coord;
using namespace astro::sun::geocentric_coord::math;
using astro::toolbox::AngleUnit::DEG;

TEST(Sun, Fk5CompatibilityEntry) {
  static_assert(std::is_same_v<Fk5Correction, astro::coords::Fk5Correction>);
  for (const double jde_tt : { 2409543.5, 2451545.0, 2488069.0 }) {
    for (const double latitude : { -45.0, 0.0, 45.0 }) {
      const astro::toolbox::SphericalCoordinate coordinate {
        .λ = astro::toolbox::AngleDeg { 359.75 },
        .β = astro::toolbox::AngleDeg { latitude },
        .r = astro::toolbox::DistanceAu { 1.0 },
      };
      const auto shared = astro::coords::fk5_correction(jde_tt, coordinate);
      const auto legacy = fk5_correction(jde_tt, coordinate);
      EXPECT_EQ(std::bit_cast<uint64_t>(shared.Δλ.deg()), std::bit_cast<uint64_t>(legacy.Δλ.deg()));
      EXPECT_EQ(std::bit_cast<uint64_t>(shared.Δβ.deg()), std::bit_cast<uint64_t>(legacy.Δβ.deg()));
    }
  }
}

TEST(Sun, GeocentricPosition) {
  for (const auto& [jde, expected] : data::geocentric_position()) {
    const auto& [λ, β, r] = vsop87d(jde);
    ASSERT_NEAR(λ.deg(), std::get<0>(expected), 1e-11);
    ASSERT_NEAR(β.deg(), std::get<1>(expected), 1e-15);
    ASSERT_NEAR(r.au(),  std::get<2>(expected), 1e-15);
  }
}


TEST(Sun, FK5Correction) {
  for (const auto& [jde, expected] : data::fk5_correction()) {
    const auto vsop_result = vsop87d(jde);
    const auto& [Δλ, Δβ] = astro::coords::fk5_correction(jde, vsop_result);
    ASSERT_NEAR(Δλ.as<DEG>(), std::get<0>(expected), 1e-15);
    ASSERT_NEAR(Δβ.as<DEG>(), std::get<1>(expected), 1e-15);
  }
}


TEST(Sun, CorrectedPosition) {
  for (const auto& [jde, expected] : data::corrected_position()) {
    const auto result = apparent(jde);
    ASSERT_NEAR(result.λ.as<DEG>(), std::get<0>(expected), 7e-7);

    const double epsilon = std::invoke([] {
      #if defined(__i386__) || defined(_M_IX86)
        return 2e-14;
      #else
        return 1e-15;
      #endif
    });

    ASSERT_NEAR(result.β.deg(), std::get<1>(expected), epsilon);
    ASSERT_NEAR(result.r.au(),  std::get<2>(expected), epsilon);
  }
}

TEST(Sun, RandomApparentPosition) {
  for (int32_t i = 0; i < 100; ++i) {
    const auto jde = astro::julian_day::J2000 + util::random(-365250.0 * 5, 365250.0 * 5);
    const auto coord = apparent(jde);

    const auto lon = coord.λ.deg();
    const auto lat = coord.β.deg();

    ASSERT_TRUE(lon >= 0.0 and lon <= 360.0);
    ASSERT_TRUE(lat >= -90.0 and lat <= 90.0);
  }
}


TEST(Sun, EquatorialApparentComposition) {
  // The equatorial place uses the same TT instant and true obliquity as the ecliptic place.
  for (int32_t i = 0; i < 64; ++i) {
    const double jde = astro::julian_day::J2000 + util::random(-365250.0 * 5, 365250.0 * 5);
    const auto ecl = geocentric_coord::apparent(jde);
    const auto ε = astro::earth::obliquity::true_obliquity(jde);
    const auto expected = astro::coords::ecliptic_to_equatorial(ecl.λ, ecl.β, ε);
    const auto got = equatorial_coord::apparent(jde);

    ASSERT_NEAR(got.α.deg(), expected.α.deg(), 1e-12);
    ASSERT_NEAR(got.δ.deg(), expected.δ.deg(), 1e-12);
  }
}


TEST(Sun, EquatorialSolsticeEquinoxDeclination) {
  // At the cardinal longitudes, declination follows the instantaneous obliquity.
  // Solar ecliptic latitude contributes a few arcseconds at equinox and solstice.
  constexpr double EQUINOX_δ_TOL_DEG = 1.0 / 60.0;
  constexpr double SOLSTICE_δ_TOL_DEG = 5.0 / 3600.0;

  for (const int32_t year : {1984, 1997, 2000, 2008, 2023, 2024, 2026, 2027}) {
    struct Sample { double δ_deg; double ε_deg; double jde; };

    const auto check_cardinal = [&](const double lon_deg, const auto& expect_δ) {
      const auto roots = find_roots(year, lon_deg);
      ASSERT_EQ(roots.size(), 1) << "year=" << year << " lon=" << lon_deg;
      const double jde = roots[0];
      const auto eq = equatorial_coord::apparent(jde);
      const auto ε = astro::earth::obliquity::true_obliquity(jde);
      expect_δ(Sample { .δ_deg = eq.δ.deg(), .ε_deg = ε.deg(), .jde = jde });
    };

    check_cardinal(0.0, [](const Sample& s) {
      ASSERT_NEAR(s.δ_deg, 0.0, EQUINOX_δ_TOL_DEG) << "春分 jde=" << s.jde;
    });
    check_cardinal(90.0, [](const Sample& s) {
      ASSERT_NEAR(s.δ_deg, s.ε_deg, SOLSTICE_δ_TOL_DEG) << "夏至 jde=" << s.jde;
      ASSERT_NEAR(s.δ_deg, 23.44, 0.05) << "夏至 rough ε check jde=" << s.jde;
    });
    check_cardinal(180.0, [](const Sample& s) {
      ASSERT_NEAR(s.δ_deg, 0.0, EQUINOX_δ_TOL_DEG) << "秋分 jde=" << s.jde;
    });
    check_cardinal(270.0, [](const Sample& s) {
      ASSERT_NEAR(s.δ_deg, -s.ε_deg, SOLSTICE_δ_TOL_DEG) << "冬至 jde=" << s.jde;
      ASSERT_NEAR(s.δ_deg, -23.44, 0.05) << "冬至 rough ε check jde=" << s.jde;
    });
  }
}


TEST(Sun, EquatorialApparentRange) {
  for (int32_t i = 0; i < 100; ++i) {
    const double jde = astro::julian_day::J2000 + util::random(-365250.0 * 5, 365250.0 * 5);
    const auto coord = equatorial_coord::apparent(jde);
    ASSERT_GE(coord.α.deg(), 0.0);
    ASSERT_LT(coord.α.deg(), 360.0);
    ASSERT_GE(coord.δ.deg(), -90.0);
    ASSERT_LE(coord.δ.deg(), 90.0);
  }
}


TEST(Sun, EquatorialApparentVsJplHorizons) {
  constexpr double α_TOL_DEG = 0.4 / 3600.0;
  constexpr double δ_TOL_DEG = 0.2 / 3600.0;

  for (const auto& [jde, expected] : data::equatorial_apparent()) {
    const auto& [expected_α, expected_δ] = expected;
    const auto got = equatorial_coord::apparent(jde);
    ASSERT_NEAR(got.α.deg(), expected_α, α_TOL_DEG) << "jde=" << jde;
    ASSERT_NEAR(got.δ.deg(), expected_δ, δ_TOL_DEG) << "jde=" << jde;
  }
}


TEST(Sun, FindRoots) {
  for (int32_t i = 0; i < 64; i++) {
    const double jde = astro::julian_day::J2000 + util::random(-300 * 365.25, 33 * 365.25);
    const auto ut1_dt = astro::julian_day::jde_to_ut1(jde);

    const auto expected_lon = math::detail::solar_longitude(jde);
    const auto [y, _ignored1, _ignored2] = util::from_ymd(ut1_dt.ymd);

    const auto roots = find_roots(y, expected_lon);
    ASSERT_TRUE(roots.size() <= 2);
    ASSERT_EQ(roots.size(), discriminant(y, expected_lon));

    if (roots.size() == 2) {
      ASSERT_NEAR(expected_lon, 280.0, 5.0);
    }

    for (const auto root : roots) {
      const auto calculated_lon = math::detail::solar_longitude(root);
      ASSERT_NEAR(calculated_lon, expected_lon, 1e-8);
    }
  }
}


TEST(Sun, FindRootsAcrossTheUlpStep) {
  // At JD 2^22 (6771-07-07), the JDE ulp doubles. The solver's derivative step must remain
  // representable on both sides (#76); each returned instant must still satisfy its target.
  const std::vector<int32_t> years { 6770, 6771, 6772, 6773, 8000, 9420, 9999 };

  for (const auto year : years) {
    for (int32_t idx = 0; idx < 24; idx++) {
      const double expected_lon = 15.0 * idx;

      for (const auto root : find_roots(year, expected_lon)) {
        ASSERT_GE(root, math::detail::get_start_jde(year));
        ASSERT_LT(root, math::detail::get_end_jde(year));

        // Past the step one ulp carries 9.1e-10 deg of solar motion; 1e-8 leaves rounding margin.
        const auto lon_diff = std::fabs(std::fmod(math::detail::solar_longitude(root) - expected_lon, 360.0));
        ASSERT_TRUE((lon_diff < 1e-8) or (lon_diff > 360.0 - 1e-8));
      }
    }
  }
}

TEST(Sun, FindRootsAtTheLegacyYearLimitUseTheRequestedLongitudeBranch) {
  // A calendar month does not locate the angular cut at the year limit.
  for (const int32_t year : std::array<int32_t, 2> { 32765, 32766 }) {
    const double start_jde_tt = math::detail::get_start_jde(year);
    const double end_jde_tt = math::detail::get_end_jde(year);
    // Target longitude and its two-day TT bracket, not an independent physical reference.
    const std::array cases {
      std::array { math::detail::solar_longitude(start_jde_tt) + 1.0, start_jde_tt, start_jde_tt + 2.0 },
      std::array { math::detail::solar_longitude(end_jde_tt) - 1.0, end_jde_tt - 2.0, end_jde_tt },
    };
    for (const auto& [target, lower_jde_tt, upper_jde_tt] : cases) {
      const auto roots = find_roots(year, target);
      ASSERT_EQ(roots.size(), 1);
      ASSERT_GE(roots.at(0), lower_jde_tt);
      ASSERT_LT(roots.at(0), upper_jde_tt);
      const double residual = std::remainder(math::detail::solar_longitude(roots.at(0)) - target, 360.0);
      ASSERT_NEAR(residual, 0.0, 1e-8);
    }
  }
}

TEST(Sun, AnnualQueriesRejectUnrepresentableAndWideYears) {
  const auto expect_rejection = [](const int32_t year, const std::string_view range,
                                   const auto& query) {
    try {
      std::ignore = query(year);
      FAIL() << "year " << year << " must not be admitted";
    } catch (const std::invalid_argument& e) {
      const std::string_view message { e.what() };
      EXPECT_NE(message.find(std::to_string(year)), std::string_view::npos);
      EXPECT_NE(message.find(range), std::string_view::npos);
    }
  };

  // Annual queries form the `year + 1` boundary, so their window is [1, 32766].
  const std::string_view annual { "[1, 32766]" };
  for (const int32_t year : { -1, 0, 32767, 65537, 67562, 67421, -63510,
                              std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max() }) {
    expect_rejection(year, annual, [](const int32_t y) { return discriminant(y, 0.0); });
    expect_rejection(year, annual, [](const int32_t y) { return find_roots(y, 0.0); });
    // `get_end_jde` must throw before forming `year + 1`; INT32_MAX is the overflow control.
    expect_rejection(year, annual, [](const int32_t y) { return detail::get_end_jde(y); });
  }

  const std::string_view scalar { "std::chrono::year" };
  for (const int32_t year : { 32768, -32768, 65537, 67562, 67421, -63510,
                              std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max() }) {
    expect_rejection(year, scalar, [](const int32_t y) { return detail::get_start_jde(y); });
    expect_rejection(year, scalar, [](const int32_t y) { return detail::make_f(y, 0.0); });
  }
}

TEST(Sun, ScalarStartKeeps32767WhileAnnualQueriesStopAt32766) {
  // The annual window's ceiling exists only because of the `year + 1` end boundary.
  EXPECT_TRUE(std::isfinite(detail::get_start_jde(32766)));
  EXPECT_TRUE(std::isfinite(detail::get_end_jde(32766)));
  EXPECT_EQ(discriminant(32766, 0.0), 1U);

  EXPECT_TRUE(std::isfinite(detail::get_start_jde(32767)));
  EXPECT_EQ(detail::get_start_jde(32767),
            astro::julian_day::utc_year_start_jde(std::chrono::year { 32767 }));
  EXPECT_NO_THROW(std::ignore = detail::make_f(32767, 0.0));
}


} // namespace astro::sun::test
