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

#include <cstdint>

#include "celestial.h"
#include "lib.hpp"
#include "lib_civil.hpp"
#include "lib_codes.hpp"
#include "observer.hpp"

extern "C" {

// NOLINTBEGIN(bugprone-easily-swappable-parameters): The target/civil/site/weather order is the published query contract.
auto celestial_topocentric_position(
  const uint32_t target_code,
  const int32_t year,
  const uint32_t month,
  const uint32_t day,
  const double fraction,
  const uint32_t civil_scale_code,
  const uint32_t delta_t_model_code,
  const double latitude_deg,
  const double longitude_deg,
  const double elevation_m,
  const uint32_t refraction_code,
  const double temperature_c,
  const double pressure_hpa
) -> CelestialTopocentricPosition {
  return lib::wrap_export(__func__, [=] {
    lib::civil::validate(year, month, day, fraction);
    const auto observation = astro::observer::query(
      calendar::Datetime { util::to_ymd(year, month, day), fraction },
      lib::civil::scale(civil_scale_code),
      astro::observer::Observer {
        .location = {
          .latitude = astro::toolbox::AngleDeg { latitude_deg },
          .longitude = astro::toolbox::AngleDeg { longitude_deg },
        },
        .elevation = astro::toolbox::DistanceKm { elevation_m / 1000.0 },
      },
      lib::codes::target(target_code),
      lib::codes::refraction_params(refraction_code, temperature_c, pressure_hpa),
      lib::codes::delta_t_model(delta_t_model_code)
    );
    const auto& position = observation.position;
    return CelestialTopocentricPosition {
      .valid = true,
      .target_code = lib::codes::code_of(observation.target),
      .source_code = lib::codes::code_of(observation.source),
      .delta_t_identity_code = lib::codes::code_of(observation.delta_t_model),
      .present_fields = (position.right_ascension.has_value() ? CELESTIAL_PRESENT_RIGHT_ASCENSION : UINT32_C(0))
        | (position.azimuth.has_value() ? CELESTIAL_PRESENT_AZIMUTH : UINT32_C(0))
        | (position.apparent_altitude.has_value() ? CELESTIAL_PRESENT_APPARENT_ALTITUDE : UINT32_C(0)),
      .jd_ut1 = observation.times.jd_ut1,
      .jde_tt = observation.times.jde_tt,
      .right_ascension_deg = position.right_ascension.has_value() ? position.right_ascension->deg() : 0.0,
      .declination_deg = position.declination.deg(),
      .distance_au = position.distance.au(),
      .altitude_deg = position.altitude.deg(),
      .azimuth_deg = position.azimuth.has_value() ? position.azimuth->deg() : 0.0,
      .apparent_altitude_deg = position.apparent_altitude.has_value() ? position.apparent_altitude->deg() : 0.0,
    };
  });
}
// NOLINTEND(bugprone-easily-swappable-parameters)

} // extern "C"
