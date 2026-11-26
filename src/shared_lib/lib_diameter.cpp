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
#include <optional>

#include "apparent_diameter.hpp"
#include "celestial.h"
#include "lib.hpp"
#include "lib_civil.hpp"
#include "lib_codes.hpp"

namespace {

// NOLINTBEGIN(modernize-use-designated-initializers): Explicit C/core identity columns follow the boundary tables.
constexpr std::array CENTRES {
  lib::codes::Entry { CELESTIAL_DIAMETER_CENTRE_GEOCENTRIC, astro::apparent_diameter::Centre::GEOCENTRIC, "geocentric" },
  lib::codes::Entry { CELESTIAL_DIAMETER_CENTRE_TOPOCENTRIC, astro::apparent_diameter::Centre::TOPOCENTRIC, "topocentric" },
};
constexpr std::array RADIUS_DEFINITIONS {
  lib::codes::Entry { CELESTIAL_RADIUS_DEFINITION_IAU_2015_SOLAR_NOMINAL, astro::apparent_diameter::RadiusDefinition::IAU_2015_SOLAR_NOMINAL, "IAU 2015 nominal solar" },
  lib::codes::Entry { CELESTIAL_RADIUS_DEFINITION_JPL_SSD_SATELLITE_MEAN, astro::apparent_diameter::RadiusDefinition::JPL_SSD_SATELLITE_MEAN, "JPL SSD satellite mean" },
  lib::codes::Entry { CELESTIAL_RADIUS_DEFINITION_JPL_SSD_PLANET_EQUATORIAL, astro::apparent_diameter::RadiusDefinition::JPL_SSD_PLANET_EQUATORIAL, "JPL SSD planetary equatorial" },
};
// NOLINTEND(modernize-use-designated-initializers)
static_assert(lib::codes::detail::unique(CENTRES) and lib::codes::detail::unique(RADIUS_DEFINITIONS));

/** @brief Decode the full-width centre selector before reading optional site storage. */
// NOLINTBEGIN(bugprone-easily-swappable-parameters): Names retain the published selector/latitude/longitude/elevation order.
auto selected_observer(
  const uint32_t centre_code,
  const double latitude_deg,
  const double longitude_deg,
  const double elevation_m
) -> std::optional<astro::observer::Observer> {
  const auto centre = lib::codes::detail::from_code(CENTRES, centre_code).value;
  if (centre == astro::apparent_diameter::Centre::GEOCENTRIC) {
    return std::nullopt;
  }
  return astro::observer::Observer {
    .location = {
      .latitude = astro::toolbox::AngleDeg { latitude_deg },
      .longitude = astro::toolbox::AngleDeg { longitude_deg },
    },
    .elevation = astro::toolbox::DistanceKm { elevation_m / 1000.0 },
  };
}
// NOLINTEND(bugprone-easily-swappable-parameters)

} // namespace

extern "C" {

// NOLINTBEGIN(bugprone-easily-swappable-parameters): The target/civil/centre/site order is the published query contract.
auto celestial_apparent_diameter(
  const uint32_t target_code,
  const int32_t year,
  const uint32_t month,
  const uint32_t day,
  const double fraction,
  const uint32_t civil_scale_code,
  const uint32_t delta_t_model_code,
  const uint32_t centre_code,
  const double latitude_deg,
  const double longitude_deg,
  const double elevation_m
) -> CelestialApparentDiameter {
  return lib::wrap_export(__func__, [=] {
    lib::civil::validate(year, month, day, fraction);
    const auto result = astro::apparent_diameter::query(
      calendar::Datetime { util::to_ymd(year, month, day), fraction },
      lib::civil::scale(civil_scale_code),
      lib::codes::target(target_code),
      selected_observer(centre_code, latitude_deg, longitude_deg, elevation_m),
      lib::codes::delta_t_model(delta_t_model_code)
    );
    return CelestialApparentDiameter {
      .valid = true,
      .target_code = lib::codes::code_of(result.target),
      .centre_code = lib::codes::detail::from_value(CENTRES, result.centre).code,
      .source_code = lib::codes::code_of(result.source),
      .radius_definition_code = lib::codes::detail::from_value(RADIUS_DEFINITIONS, result.radius_definition).code,
      .delta_t_identity_code = lib::codes::code_of(result.delta_t_model),
      .jd_ut1 = result.times.jd_ut1,
      .jde_tt = result.times.jde_tt,
      .radius_km = result.radius.km(),
      .distance_au = result.distance.au(),
      .diameter_arcsec = result.diameter.deg() * astro::toolbox::SEC_PER_DEG,
    };
  });
}
// NOLINTEND(bugprone-easily-swappable-parameters)

} // extern "C"
