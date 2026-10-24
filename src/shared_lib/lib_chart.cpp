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

#include "lib.hpp"
#include "lib_chart.hpp"
#include "celestial.h"

#include "datetime.hpp"
#include "ymd.hpp"

extern "C" {

// NOLINTBEGIN(bugprone-easily-swappable-parameters): Parameter order is the published civil/location/selector contract.
auto celestial_chart_snapshot(
  const int32_t year,
  const uint32_t month,
  const uint32_t day,
  const double fraction,
  const uint32_t civil_scale_code,
  const double latitude_deg,
  const double longitude_deg,
  const uint32_t house_system_code,
  const uint32_t delta_t_model_code
) -> CelestialChartSnapshot {
  return lib::wrap_export(__func__, [=]() -> CelestialChartSnapshot {
    lib::chart::validate_civil(year, month, day, fraction);
    const auto scale = lib::chart::civil_scale(civil_scale_code);
    const auto system = lib::chart::house_system(house_system_code);
    const auto model = lib::codes::delta_t_model(delta_t_model_code);

    const calendar::Datetime civil_dt { util::to_ymd(year, month, day), fraction };
    const astro::GeoLocation location {
      .latitude = astro::toolbox::AngleDeg { latitude_deg },
      .longitude = astro::toolbox::AngleDeg { longitude_deg },
    };

    return lib::chart::snapshot_record(
      astro::chart::calculate(civil_dt, scale, location, system, model),
      model,
      astro::house::DEFAULT_NUTATION_MODEL
    );
  });
}
// NOLINTEND(bugprone-easily-swappable-parameters)

} // extern "C"
