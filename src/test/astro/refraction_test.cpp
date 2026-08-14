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

#include <algorithm>
#include <cmath>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "util.hpp"
#include "astro.hpp"


namespace astro::earth::refraction::test {

using astro::toolbox::AngleDeg;
using astro::earth::refraction::Model;
using astro::earth::refraction::Params;
using astro::earth::refraction::at_horizon;
using astro::earth::refraction::bennett;
using astro::earth::refraction::saemundsson;

/**
 * @brief Checked unwrap for assertions. clang-tidy's bugprone-unchecked-optional-access cannot
 *        see through gtest's ASSERT_TRUE, so tests unwrap through this provably-guarded helper.
 */
template <typename T>
auto req(const std::optional<T>& opt) -> const T& {
  if (not opt.has_value()) {
    throw std::logic_error { "expected optional to hold a value" };
  }
  return *opt;
}


TEST(Refraction, BennettAtHorizonMatchesStandardRefraction) {
  // Bennett at apparent altitude 0°, corrected to default 15°C/1013.25 hPa, should reproduce the
  // standard −34′ of horizon refraction embedded in `STANDARD_ALTITUDE`.
  const auto params = Params {};
  const auto r = at_horizon(params);

  ASSERT_NEAR(r.deg(), AngleDeg::from_arcmin(34.0).deg(), AngleDeg::from_arcmin(0.02).deg());
}


TEST(Refraction, SaemundssonAtHorizonIsConsistentWithBennett) {
  // The two models solve the same physics at the horizon and should agree to well under 1′.
  const auto r_bennett = at_horizon(Params { .model = Model::BENNETT });
  const auto r_saemundsson = at_horizon(Params { .model = Model::SAEMUNDSSON });

  ASSERT_NEAR(r_bennett.deg(), r_saemundsson.deg(), AngleDeg::from_arcmin(1.0).deg());
}


TEST(Refraction, H0FromDefaultsMatchesStandardAltitude) {
  // The default params must reproduce `sunrise_sunset::STANDARD_ALTITUDE` to within 0.02′.
  const auto h0 = astro::sunrise_sunset::h0_from(Params {});

  ASSERT_NEAR(h0.deg(), astro::sunrise_sunset::STANDARD_ALTITUDE.deg(),
              AngleDeg::from_arcmin(0.02).deg());
}


TEST(Refraction, TemperatureDecreasesRefractionAtFixedPressure) {
  // Warmer air is less dense, so refraction decreases as temperature rises (fixed pressure).
  const std::vector<double> temperatures { -20.0, -10.0, 0.0, 10.0, 20.0, 30.0, 40.0 };

  std::vector<double> refractions;
  refractions.reserve(temperatures.size());
  for (const double t : temperatures) {
    refractions.push_back(at_horizon(Params { .temperature_c = t, .pressure_hpa = 1013.25 }).deg());
  }

  ASSERT_TRUE(std::ranges::is_sorted(refractions | std::views::reverse));
}


TEST(Refraction, PressureIncreasesRefractionAtFixedTemperature) {
  // Denser air refracts more, so refraction increases with pressure (fixed temperature).
  const std::vector<double> pressures { 900.0, 950.0, 1013.25, 1050.0, 1100.0 };

  std::vector<double> refractions;
  refractions.reserve(pressures.size());
  for (const double p : pressures) {
    refractions.push_back(at_horizon(Params { .temperature_c = 15.0, .pressure_hpa = p }).deg());
  }

  ASSERT_TRUE(std::ranges::is_sorted(refractions));
}


TEST(Refraction, ExtremeConditionsSpanReasonableRange) {
  // Cold/high-pressure vs hot/low-pressure should change the horizon refraction by ~10–20′.
  const auto cold_high = at_horizon(Params { .temperature_c = -20.0, .pressure_hpa = 1100.0 });
  const auto hot_low = at_horizon(Params { .temperature_c = +40.0, .pressure_hpa = 900.0 });

  const double span_arcmin = (cold_high - hot_low).deg() * 60.0;
  ASSERT_GT(span_arcmin, 10.0);
  ASSERT_LT(span_arcmin, 20.0);
}


TEST(Refraction, BennettIsPositiveForValidAltitudes) {
  // Refraction always lifts the apparent position; the returned value must be positive where
  // Bennett's formula is valid (apparent altitude ≥ 0°; Meeus warns against using it below −5°).
  // At the zenith (90°) the formula rounds to a tiny negative value, but the physical refraction
  // is zero, so we assert it is near zero rather than strictly positive.
  const std::vector<double> altitudes { 0.0, 5.0, 10.0, 30.0, 60.0 };

  for (const double h : altitudes) {
    const auto r = bennett(AngleDeg { h });
    ASSERT_GT(r.deg(), 0.0) << "h=" << h;
  }

  ASSERT_NEAR(bennett(AngleDeg { 90.0 }).deg(), 0.0, AngleDeg::from_arcsec(1.0).deg());
}


TEST(Refraction, SaemundssonIsPositiveForValidAltitudes) {
  // Refraction always lifts the apparent position; the returned value must be positive where
  // Saemundsson's formula is valid (true altitude ≥ 0°; Meeus warns against using it below −5°).
  // At the zenith (90°) the formula rounds to a tiny negative value, but the physical refraction
  // is zero, so we assert it is near zero rather than strictly positive.
  const std::vector<double> altitudes { 0.0, 5.0, 10.0, 30.0, 60.0 };

  for (const double h : altitudes) {
    const auto r = saemundsson(AngleDeg { h });
    ASSERT_GT(r.deg(), 0.0) << "h=" << h;
  }

  ASSERT_NEAR(saemundsson(AngleDeg { 90.0 }).deg(), 0.0, AngleDeg::from_arcsec(1.0).deg());
}


TEST(Refraction, BennettDecreasesWithApparentAltitude) {
  // Higher apparent altitude → smaller refraction.
  const auto r_low = bennett(AngleDeg { 5.0 });
  const auto r_high = bennett(AngleDeg { 45.0 });

  ASSERT_GT(r_low.deg(), r_high.deg());
}


TEST(Refraction, SaemundssonDecreasesWithTrueAltitude) {
  // Higher true altitude → smaller refraction.
  const auto r_low = saemundsson(AngleDeg { 5.0 });
  const auto r_high = saemundsson(AngleDeg { 45.0 });

  ASSERT_GT(r_low.deg(), r_high.deg());
}


TEST(Refraction, DefaultParamsDoNotShiftSunriseSunset) {
  // Using `h0_from(Params{})` instead of the default `STANDARD_ALTITUDE` should not move
  // sunrise/sunset times at low/mid latitudes by more than 0.1 s.
  using astro::sunrise_sunset::calculate;
  using astro::sunrise_sunset::GeoLocation;
  using astro::sunrise_sunset::h0_from;

  const auto ymd = util::to_ymd(2024, 6, 21);
  const std::vector<GeoLocation> locations {
    { .latitude = AngleDeg { 0.0 },  .longitude = AngleDeg { 0.0 } },
    { .latitude = AngleDeg { 40.0 }, .longitude = AngleDeg { 0.0 } },
    { .latitude = AngleDeg { -33.0 }, .longitude = AngleDeg { 151.0 } },
  };

  for (const auto& location : locations) {
    const auto r_default = calculate(ymd, location);
    const auto r_params = calculate(ymd, location, h0_from(Params {}));

    ASSERT_TRUE(r_default.sunrise_jde.has_value());
    ASSERT_TRUE(r_default.sunset_jde.has_value());
    ASSERT_TRUE(r_params.sunrise_jde.has_value());
    ASSERT_TRUE(r_params.sunset_jde.has_value());

    const double tol_days = 0.1 / 86400.0;
    ASSERT_NEAR(req(r_default.sunrise_jde), req(r_params.sunrise_jde), tol_days)
      << "lat=" << location.latitude.deg();
    ASSERT_NEAR(req(r_default.sunset_jde), req(r_params.sunset_jde), tol_days)
      << "lat=" << location.latitude.deg();
  }
}


TEST(Refraction, H0FromSaemundssonIsCloseToBennett) {
  // The two models should produce similar h₀ values at default T/P.
  const auto h0_bennett = astro::sunrise_sunset::h0_from(Params { .model = Model::BENNETT });
  const auto h0_saemundsson = astro::sunrise_sunset::h0_from(Params { .model = Model::SAEMUNDSSON });

  ASSERT_NEAR(h0_bennett.deg(), h0_saemundsson.deg(), AngleDeg::from_arcmin(1.0).deg());
}

} // namespace astro::earth::refraction::test
