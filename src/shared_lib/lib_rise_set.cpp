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

#include <array>
#include <cstdint>
#include <stdexcept>

#include "celestial.h"
#include "lib.hpp"
#include "lib_civil.hpp"
#include "lib_codes.hpp"
#include "planet_rise_set.hpp"
#include "rise_set.hpp"

namespace {

// NOLINTBEGIN(modernize-use-designated-initializers): C/core identity columns follow the boundary tables.
constexpr std::array POLAR_CODES {
  lib::codes::Entry { CELESTIAL_RISE_SET_POLAR_NONE, astro::rise_set::Polar::NONE, "none" },
  lib::codes::Entry { CELESTIAL_RISE_SET_POLAR_DAY, astro::rise_set::Polar::DAY, "day" },
  lib::codes::Entry { CELESTIAL_RISE_SET_POLAR_NIGHT, astro::rise_set::Polar::NIGHT, "night" },
};
// NOLINTEND(modernize-use-designated-initializers)
static_assert(lib::codes::detail::unique(POLAR_CODES));

/** @brief Encode independent event availability, not polar topology. */
auto present_fields(const astro::rise_set::Result& result) -> uint32_t {
  return (result.rise_jde_tt.has_value() ? CELESTIAL_PRESENT_RISE : UINT32_C(0))
    | (result.transit_jde_tt.has_value() ? CELESTIAL_PRESENT_TRANSIT : UINT32_C(0))
    | (result.set_jde_tt.has_value() ? CELESTIAL_PRESENT_SET : UINT32_C(0));
}

} // namespace

extern "C" {

// NOLINTBEGIN(bugprone-easily-swappable-parameters): Names retain the published date/site/altitude/weather order.
auto celestial_solar_rise_set(
  const int32_t year,
  const uint32_t month,
  const uint32_t day,
  const double latitude_deg,
  const double longitude_deg,
  const double h0_deg
) -> CelestialSolarRiseSet {
  return lib::wrap_export(__func__, [=] {
    lib::civil::validate(year, month, day, 0.0);
    const auto result = astro::rise_set::sun::calculate(
      util::to_ymd(year, month, day),
      { .latitude = astro::toolbox::AngleDeg { latitude_deg }, .longitude = astro::toolbox::AngleDeg { longitude_deg } },
      astro::toolbox::AngleDeg { h0_deg }
    );

    return CelestialSolarRiseSet {
      .valid = true,
      .source_code = lib::codes::code_of(astro::ephemeris::source_of(astro::ephemeris::Target::SUN)),
      .delta_t_identity_code = lib::codes::code_of(astro::delta_t::DEFAULT_MODEL),
      .nutation_identity_code = lib::codes::code_of(astro::earth::nutation::Model::IAU_1980),
      .present_fields = present_fields(result),
      .polar_code = lib::codes::detail::from_value(POLAR_CODES, result.polar).code,
      .rise_jde_tt = result.rise_jde_tt.value_or(0.0),
      .transit_jde_tt = result.transit_jde_tt.value_or(0.0),
      .set_jde_tt = result.set_jde_tt.value_or(0.0),
      .h0_deg = h0_deg,
    };
  });
}

auto celestial_lunar_rise_set(
  const int32_t year,
  const uint32_t month,
  const uint32_t day,
  const double latitude_deg,
  const double longitude_deg,
  const uint32_t refraction_code,
  const double temperature_c,
  const double pressure_hpa
) -> CelestialLunarRiseSet {
  return lib::wrap_export(__func__, [=] {
    lib::civil::validate(year, month, day, 0.0);
    const auto params = lib::codes::refraction_params(refraction_code, temperature_c, pressure_hpa);
    if (not params.has_value()) {
      throw std::invalid_argument { "Lunar rise/set requires Bennett or Saemundsson horizon refraction" };
    }

    const auto result = astro::rise_set::moon::calculate(
      util::to_ymd(year, month, day),
      { .latitude = astro::toolbox::AngleDeg { latitude_deg }, .longitude = astro::toolbox::AngleDeg { longitude_deg } },
      *params
    );

    return CelestialLunarRiseSet {
      .valid = true,
      .source_code = lib::codes::code_of(astro::ephemeris::source_of(astro::ephemeris::Target::MOON)),
      .delta_t_identity_code = lib::codes::code_of(astro::delta_t::DEFAULT_MODEL),
      .nutation_identity_code = lib::codes::code_of(astro::earth::nutation::Model::IAU_1980),
      .refraction_code = refraction_code,
      .present_fields = present_fields(result),
      .polar_code = lib::codes::detail::from_value(POLAR_CODES, result.polar).code,
      .rise_jde_tt = result.rise_jde_tt.value_or(0.0),
      .transit_jde_tt = result.transit_jde_tt.value_or(0.0),
      .set_jde_tt = result.set_jde_tt.value_or(0.0),
      .temperature_c = temperature_c,
      .pressure_hpa = pressure_hpa,
    };
  });
}

auto celestial_planet_rise_set(
  const uint32_t target_code,
  const int32_t year,
  const uint32_t month,
  const uint32_t day,
  const double latitude_deg,
  const double longitude_deg,
  const double elevation_m,
  const double h0_deg
) -> CelestialPlanetRiseSet {
  return lib::wrap_export(__func__, [=] {
    lib::civil::validate(year, month, day, 0.0);
    const auto target = lib::codes::target(target_code);
    const auto result = astro::rise_set::planet::calculate(
      util::to_ymd(year, month, day),
      {
        .location = {
          .latitude = astro::toolbox::AngleDeg { latitude_deg },
          .longitude = astro::toolbox::AngleDeg { longitude_deg },
        },
        .elevation = astro::toolbox::DistanceKm { elevation_m / 1000.0 },
      },
      target,
      astro::toolbox::AngleDeg { h0_deg }
    );

    return CelestialPlanetRiseSet {
      .valid = true,
      .target_code = lib::codes::code_of(target),
      .source_code = lib::codes::code_of(astro::ephemeris::source_of(target)),
      .delta_t_identity_code = lib::codes::code_of(astro::delta_t::DEFAULT_MODEL),
      .nutation_identity_code = lib::codes::code_of(astro::earth::nutation::Model::IAU_1980),
      .present_fields = present_fields(result),
      .polar_code = lib::codes::detail::from_value(POLAR_CODES, result.polar).code,
      .rise_jde_tt = result.rise_jde_tt.value_or(0.0),
      .transit_jde_tt = result.transit_jde_tt.value_or(0.0),
      .set_jde_tt = result.set_jde_tt.value_or(0.0),
      .h0_deg = h0_deg,
    };
  });
}
// NOLINTEND(bugprone-easily-swappable-parameters)

} // extern "C"
