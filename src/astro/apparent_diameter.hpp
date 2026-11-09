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

#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <optional>
#include <stdexcept>

#include "ephemeris.hpp"
#include "observer.hpp"
#include "julian_day.hpp"
#include "toolbox.hpp"

namespace astro::apparent_diameter {

/** @brief Origin of the distance used for a spherical disk. */
enum class Centre : uint8_t { GEOCENTRIC, TOPOCENTRIC };

/** @brief Adopted radius source and convention, independent of the position provider. */
enum class RadiusDefinition : uint8_t {
  IAU_2015_SOLAR_NOMINAL,
  JPL_SSD_SATELLITE_MEAN,
  JPL_SSD_PLANET_EQUATORIAL,
};

/** @brief One physical target's adopted spherical radius. */
struct RadiusFacts {
  ephemeris::Target target;
  toolbox::DistanceKm radius;
  RadiusDefinition definition;
};

// Retained material boundary: adopted IAU/JPL radius facts remain under their source terms,
// outside the project MIT grant. See src/test/provenance/diameter/ATTRIBUTION.md.
/**
 * @brief Nominal solar, mean lunar and equatorial planetary radii in kilometres; Saturn excludes rings.
 * @see Prsa et al. (2016), arXiv:1605.09788v1, Table 1 (IAU 2015 Resolution B3).
 * @see https://ssd.jpl.nasa.gov/planets/phys_par.html equatorial column, reference D (Archinal et al. 2018).
 * @see https://ssd.jpl.nasa.gov/sats/phys_par/ Moon mean radius, reference 1. Collected 2026-11-08.
 */
inline constexpr auto RADIUS_FACTS = std::invoke([] {
  using enum ephemeris::Target;
  using enum RadiusDefinition;
  using toolbox::DistanceKm;
  // NOLINTBEGIN(modernize-use-designated-initializers): Keep source/convention columns aligned across the roster.
  return std::array {
    RadiusFacts { SUN,     DistanceKm { 695700.0 }, IAU_2015_SOLAR_NOMINAL   },
    RadiusFacts { MOON,    DistanceKm {   1737.4 }, JPL_SSD_SATELLITE_MEAN  },
    RadiusFacts { MERCURY, DistanceKm {  2440.53 }, JPL_SSD_PLANET_EQUATORIAL },
    RadiusFacts { VENUS,   DistanceKm {   6051.8 }, JPL_SSD_PLANET_EQUATORIAL },
    RadiusFacts { MARS,    DistanceKm {  3396.19 }, JPL_SSD_PLANET_EQUATORIAL },
    RadiusFacts { JUPITER, DistanceKm {  71492.0 }, JPL_SSD_PLANET_EQUATORIAL },
    RadiusFacts { SATURN,  DistanceKm {  60268.0 }, JPL_SSD_PLANET_EQUATORIAL },
    RadiusFacts { URANUS,  DistanceKm {  25559.0 }, JPL_SSD_PLANET_EQUATORIAL },
    RadiusFacts { NEPTUNE, DistanceKm {  24764.0 }, JPL_SSD_PLANET_EQUATORIAL },
    RadiusFacts { PLUTO,   DistanceKm {   1188.3 }, JPL_SSD_PLANET_EQUATORIAL },
  };
  // NOLINTEND(modernize-use-designated-initializers)
});

namespace detail {

/** @brief Require exactly one radius for each physical provider target and none for nodes or unknown values. */
[[nodiscard]] consteval auto complete_radii() -> bool {
  for (uint16_t code = 0; code <= UINT8_MAX; ++code) {
    const auto target = static_cast<ephemeris::Target>(code);
    const bool physical = ephemeris::is_target(target) and not ephemeris::target_facts(target).node.has_value();
    std::size_t count = 0;
    for (const auto& facts : RADIUS_FACTS) {
      count += facts.target == target ? std::size_t { 1 } : std::size_t { 0 };
    }
    if (count != (physical ? std::size_t { 1 } : std::size_t { 0 })) {
      return false;
    }
  }
  return true;
}

static_assert(complete_radii());

} // namespace detail

/** @brief Find a physical target's adopted radius; nodes and unknown targets are invalid input. */
[[nodiscard]] inline auto radius_facts(const ephemeris::Target target) -> const RadiusFacts& {
  for (const auto& facts : RADIUS_FACTS) {
    if (facts.target == target) {
      return facts;
    }
  }
  throw std::invalid_argument { std::format("Apparent diameter requires a physical target, got {}", static_cast<uint32_t>(target)) };
}

/** @brief Full spherical disk angle with the actual distance used for the calculation. */
struct Size {
  toolbox::DistanceAu distance;
  toolbox::AngleDeg diameter;
};

/** @brief Civil disk query with resolved times, adopted radius and position/model identities. */
// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init): Strong-unit members require values; all fields are supplied.
struct ApparentDiameter {
  julian_day::Times times;
  ephemeris::Target target;
  Centre centre;
  ephemeris::PositionSource source;
  delta_t::Model delta_t_model;
  toolbox::DistanceKm radius;
  RadiusDefinition radius_definition;
  toolbox::DistanceAu distance;
  toolbox::AngleDeg diameter;
};

/**
 * @brief Full spherical disk angle, 2 asin(R/d), from a tangent right triangle.
 * @see https://ssd.jpl.nasa.gov/horizons/manual.html#target-angular-diameter for the full-disk convention.
 * @param radius Finite positive radius in kilometres.
 * @param distance Finite positive centre distance in AU, strictly greater than the radius.
 * @return Finite angle strictly between 0 and 180 degrees, including positive subnormal results.
 * @throw std::invalid_argument For nonfinite/nonpositive input or d <= R.
 * @throw std::runtime_error If the admissible pair cannot produce a positive representable angle.
 */
[[nodiscard]] inline auto diameter(const toolbox::DistanceKm radius, const toolbox::DistanceAu distance) -> toolbox::AngleDeg {
  const double radius_au = radius.au();
  const double distance_au = distance.au();
  if (not std::isfinite(radius.km()) or radius.km() <= 0.0
      or not std::isfinite(distance_au) or distance_au <= 0.0 or distance_au <= radius_au) {
    throw std::invalid_argument { "Disk radius and distance must be finite, positive, and satisfy d > R" };
  }

  const double ratio = radius_au / distance_au;
  const auto angle = toolbox::AngleDeg { toolbox::rad_to_deg(2.0 * std::asin(ratio)) };
  if (not std::isfinite(angle.deg()) or angle.deg() <= 0.0 or angle.deg() >= 180.0) {
    throw std::runtime_error { "Disk angle is not finite, positive and below 180 degrees" };
  }
  return angle;
}

/** @brief Geocentric disk at JDE(TT); provider errors retain their original classification. */
[[nodiscard]] inline auto geocentric(const double jde_tt, const ephemeris::Target target) -> Size {
  const auto& facts = radius_facts(target);
  const auto position = ephemeris::position(target, jde_tt);
  if (not position.distance.has_value()) {
    throw std::runtime_error { "Physical position has no distance for apparent diameter" };
  }
  try {
    return { .distance = *position.distance, .diameter = diameter(facts.radius, *position.distance) };
  } catch (const std::invalid_argument& error) {
    throw std::runtime_error { std::format("Invalid geocentric disk separation: {}", error.what()) };
  }
}

/**
 * @brief Disk using existing observer translation at caller-resolved same-instant UT1/TT.
 * @throw std::invalid_argument For malformed input or a resolved observer at or inside the target radius.
 * @throw std::runtime_error For failed provider/observer evaluation or unrepresentable geometry.
 */
[[nodiscard]] inline auto topocentric(
  const julian_day::Times& times,
  const observer::Observer& observer,
  const ephemeris::Target target
) -> Size {
  const auto& facts = radius_facts(target);
  const auto position = observer::observe(times, observer, target);
  return { .distance = position.distance, .diameter = diameter(facts.radius, position.distance) };
}

/**
 * @brief Resolve a UTC/UT1 civil instant and return a geocentric or explicitly selected topocentric disk.
 * @note Uses the selected provider's distance, not a new retarded-range solve. Provider and Delta T guards
 *       apply without a chart/rate window. Solar radius is nominal; planetary disks are equatorial scalars.
 */
[[nodiscard]] inline auto query(
  const calendar::Datetime& civil_dt,
  const julian_day::Scale scale,
  const ephemeris::Target target,
  const std::optional<observer::Observer>& observer = std::nullopt,
  const delta_t::Model delta_t_model = delta_t::DEFAULT_MODEL
) -> ApparentDiameter {
  const auto times = julian_day::resolve_times(civil_dt, scale, delta_t_model);
  const auto& facts = radius_facts(target);
  const auto size = observer.has_value() ? topocentric(times, *observer, target) : geocentric(times.jde_tt, target);
  return {
    .times = times,
    .target = target,
    .centre = observer.has_value() ? Centre::TOPOCENTRIC : Centre::GEOCENTRIC,
    .source = ephemeris::source_of(target),
    .delta_t_model = delta_t_model,
    .radius = facts.radius,
    .radius_definition = facts.definition,
    .distance = size.distance,
    .diameter = size.diameter,
  };
}

} // namespace astro::apparent_diameter
