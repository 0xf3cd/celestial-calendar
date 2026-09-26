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

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <numeric>
#include <stdexcept>

#include "coord_transform.hpp"
#include "earth.hpp"
#include "geolocation.hpp"
#include "sidereal_time.hpp"
#include "toolbox.hpp"

namespace astro::house {

/** @brief A supported system for dividing the ecliptic into twelve houses. */
enum class System : uint8_t { EQUAL, WHOLE_SIGN, PLACIDUS };

/**
 * @brief The principal angles and twelve cusps of a house calculation.
 * @note `cusps[0]` through `cusps[11]` are the first through twelfth house cusps.
 */
struct Result {
  astro::toolbox::AngleDeg ascendant;
  astro::toolbox::AngleDeg midheaven;
  std::array<astro::toolbox::AngleDeg, 12> cusps;
};

namespace detail {

inline constexpr std::size_t PLACIDUS_BRACKET_SEGMENTS = 360;
inline constexpr double PLACIDUS_ROOT_TOLERANCE_DEG = 1e-11;
inline constexpr std::size_t PLACIDUS_MAX_ITERATIONS = 64;

enum class PlacidusCusp : uint8_t { TWO, THREE, ELEVEN, TWELVE };

/** @see Jean Meeus, "Astronomical Algorithms", Second Edition, Chapter 13, Formula (13.3), with β = 0. */
[[nodiscard]] inline auto midheaven(
  const astro::toolbox::AngleDeg& armc,
  const astro::toolbox::AngleDeg& obliquity
) -> astro::toolbox::AngleDeg {
  return astro::toolbox::AngleDeg {
    astro::toolbox::rad_to_deg(
      std::atan2(std::sin(armc.rad()), std::cos(armc.rad()) * std::cos(obliquity.rad()))
    )
  }.normalize();
}

/** @see Jean Meeus, "Astronomical Algorithms", Second Edition, Chapter 14, Formula (14.2). */
[[nodiscard]] inline auto ascendant(
  const astro::toolbox::AngleDeg& armc,
  const astro::toolbox::AngleDeg& latitude,
  const astro::toolbox::AngleDeg& obliquity
) -> astro::toolbox::AngleDeg {
  const double horizon_intersection = astro::toolbox::rad_to_deg(
    std::atan2(
      -std::cos(armc.rad()),
      (std::sin(obliquity.rad()) * std::tan(latitude.rad()))
        + (std::cos(obliquity.rad()) * std::sin(armc.rad()))
    )
  );
  return astro::toolbox::AngleDeg { horizon_intersection + 180.0 }.normalize();
}

/**
 * @brief Evaluate one Placidus semi-arc equation on its continuous longitude branch.
 * @see Swiss Ephemeris v2.10.3bfinal, `swehouse.c`, Placidus branch (independent fixed-point oracle).
 */
// NOLINTBEGIN(bugprone-easily-swappable-parameters): angle names carry distinct physical roles in the equation.
[[nodiscard]] inline auto placidus_equation(
  const double longitude_deg,
  const astro::toolbox::AngleDeg& latitude,
  const astro::toolbox::AngleDeg& obliquity,
  const PlacidusCusp cusp
) -> double {
  const auto equatorial = astro::coords::ecliptic_to_equatorial(
    astro::toolbox::AngleDeg { longitude_deg },
    astro::toolbox::AngleDeg { 0.0 },
    obliquity
  );
  double right_ascension_deg = equatorial.α.deg();
  if (longitude_deg > 180.0 and right_ascension_deg < 180.0) {
    right_ascension_deg += 360.0;
  }

  const double semidiurnal_arc_deg = astro::toolbox::rad_to_deg(
    std::acos(std::clamp(-std::tan(latitude.rad()) * std::tan(equatorial.δ.rad()), -1.0, 1.0))
  );
  const double nocturnal_arc_deg = 180.0 - semidiurnal_arc_deg;

  switch (cusp) {
    case PlacidusCusp::TWO:
      return right_ascension_deg - semidiurnal_arc_deg - (nocturnal_arc_deg / 3.0);
    case PlacidusCusp::THREE:
      return right_ascension_deg - semidiurnal_arc_deg - ((2.0 * nocturnal_arc_deg) / 3.0);
    case PlacidusCusp::ELEVEN: return right_ascension_deg - (semidiurnal_arc_deg / 3.0);
    case PlacidusCusp::TWELVE: return right_ascension_deg - ((2.0 * semidiurnal_arc_deg) / 3.0);
    default: throw std::invalid_argument { "Unknown Placidus cusp" };
  }
}
// NOLINTEND(bugprone-easily-swappable-parameters)

// NOLINTBEGIN(bugprone-easily-swappable-parameters): angle names carry distinct physical roles in the solver.
[[nodiscard]] inline auto placidus_cusp(
  const astro::toolbox::AngleDeg& armc,
  const astro::toolbox::AngleDeg& latitude,
  const astro::toolbox::AngleDeg& obliquity,
  const PlacidusCusp cusp,
  const std::size_t max_iterations = PLACIDUS_MAX_ITERATIONS
) -> astro::toolbox::AngleDeg {
  const double start_value = placidus_equation(0.0, latitude, obliquity, cusp);
  const double end_value = placidus_equation(360.0, latitude, obliquity, cusp);
  double target_deg = armc.normalize().deg();
  while (target_deg < std::min(start_value, end_value)) {
    target_deg += 360.0;
  }
  while (target_deg > std::max(start_value, end_value)) {
    target_deg -= 360.0;
  }

  double left_deg = 0.0;
  double left_residual = start_value - target_deg;
  if (std::fabs(left_residual) <= PLACIDUS_ROOT_TOLERANCE_DEG) {
    return astro::toolbox::AngleDeg { 0.0 };
  }

  std::size_t segment = 1;
  double right_deg = 1.0;
  double right_residual = 0.0;
  for (; segment <= PLACIDUS_BRACKET_SEGMENTS; ++segment) {
    right_deg = 360.0 * static_cast<double>(segment) / static_cast<double>(PLACIDUS_BRACKET_SEGMENTS);
    right_residual = placidus_equation(right_deg, latitude, obliquity, cusp) - target_deg;
    if (std::fabs(right_residual) <= PLACIDUS_ROOT_TOLERANCE_DEG) {
      return astro::toolbox::AngleDeg { right_deg }.normalize();
    }
    if (std::signbit(left_residual) != std::signbit(right_residual)) {
      break;
    }
    left_deg = right_deg;
    left_residual = right_residual;
  }

  if (segment > PLACIDUS_BRACKET_SEGMENTS) [[unlikely]] {
    throw std::runtime_error { "Failed to bracket a Placidus house cusp" };
  }

  for (std::size_t iteration = 0; iteration < max_iterations; ++iteration) {
    const double midpoint_deg = std::midpoint(left_deg, right_deg);
    const double midpoint_residual = placidus_equation(midpoint_deg, latitude, obliquity, cusp) - target_deg;
    if (std::fabs(midpoint_residual) <= PLACIDUS_ROOT_TOLERANCE_DEG
        or (right_deg - left_deg) <= PLACIDUS_ROOT_TOLERANCE_DEG) {
      return astro::toolbox::AngleDeg { midpoint_deg }.normalize();
    }
    if (std::signbit(left_residual) == std::signbit(midpoint_residual)) {
      left_deg = midpoint_deg;
      left_residual = midpoint_residual;
    } else {
      right_deg = midpoint_deg;
    }
  }

  throw std::runtime_error { "Placidus house-cusp solver did not converge" };
}
// NOLINTEND(bugprone-easily-swappable-parameters)

[[nodiscard]] inline auto uniform_cusps(const astro::toolbox::AngleDeg& first) -> std::array<astro::toolbox::AngleDeg, 12> {
  const auto cusp = [&first](const double offset_deg) {
    return (first + astro::toolbox::AngleDeg { offset_deg }).normalize();
  };
  return {
    cusp(0.0),   cusp(30.0),  cusp(60.0),  cusp(90.0),  cusp(120.0), cusp(150.0),
    cusp(180.0), cusp(210.0), cusp(240.0), cusp(270.0), cusp(300.0), cusp(330.0),
  };
}

[[nodiscard]] inline auto whole_sign_start(const astro::toolbox::AngleDeg& ascendant) -> astro::toolbox::AngleDeg {
  return astro::toolbox::AngleDeg { std::floor(ascendant.normalize().deg() / 30.0) * 30.0 };
}

[[nodiscard]] inline auto calculate(
  const astro::toolbox::AngleDeg& armc,
  const astro::toolbox::AngleDeg& obliquity,
  const astro::toolbox::AngleDeg& latitude,
  const System system
) -> Result {
  const auto asc = ascendant(armc, latitude, obliquity);
  const auto mc = midheaven(armc, obliquity);

  switch (system) {
    case System::EQUAL:
      return { .ascendant = asc, .midheaven = mc, .cusps = uniform_cusps(asc) };
    case System::WHOLE_SIGN:
      return { .ascendant = asc, .midheaven = mc, .cusps = uniform_cusps(whole_sign_start(asc)) };
    case System::PLACIDUS: {
      if (std::fabs(latitude.deg()) >= 90.0 - obliquity.deg()) {
        throw std::invalid_argument {
          std::format(
            "Placidus is undefined at latitude {} degrees for obliquity {} degrees",
            latitude.deg(),
            obliquity.deg()
          )
        };
      }
      const auto cusp2 = placidus_cusp(armc, latitude, obliquity, PlacidusCusp::TWO);
      const auto cusp3 = placidus_cusp(armc, latitude, obliquity, PlacidusCusp::THREE);
      const auto cusp11 = placidus_cusp(armc, latitude, obliquity, PlacidusCusp::ELEVEN);
      const auto cusp12 = placidus_cusp(armc, latitude, obliquity, PlacidusCusp::TWELVE);
      return {
        .ascendant = asc,
        .midheaven = mc,
        .cusps = {
          asc,
          cusp2,
          cusp3,
          (mc + astro::toolbox::AngleDeg { 180.0 }).normalize(),
          (cusp11 + astro::toolbox::AngleDeg { 180.0 }).normalize(),
          (cusp12 + astro::toolbox::AngleDeg { 180.0 }).normalize(),
          (asc + astro::toolbox::AngleDeg { 180.0 }).normalize(),
          (cusp2 + astro::toolbox::AngleDeg { 180.0 }).normalize(),
          (cusp3 + astro::toolbox::AngleDeg { 180.0 }).normalize(),
          mc,
          cusp11,
          cusp12,
        },
      };
    }
    default: throw std::invalid_argument { std::format("Unknown house system {}", static_cast<uint32_t>(system)) };
  }
}

inline void validate(const double jd_ut1, const double jde_tt, const astro::GeoLocation& location) {
  if (not std::isfinite(jd_ut1)) {
    throw std::invalid_argument { std::format("Argument `jd_ut1` is not finite, got {}", jd_ut1) };
  }
  if (not std::isfinite(jde_tt)) {
    throw std::invalid_argument { std::format("Argument `jde_tt` is not finite, got {}", jde_tt) };
  }

  const double latitude_deg = location.latitude.deg();
  const double longitude_deg = location.longitude.deg();
  if (not std::isfinite(latitude_deg) or latitude_deg <= -90.0 or latitude_deg >= 90.0) {
    throw std::invalid_argument {
      std::format("Argument `location.latitude` out of range (-90, 90), got {}", latitude_deg)
    };
  }
  if (not std::isfinite(longitude_deg) or longitude_deg < -180.0 or longitude_deg > 180.0) {
    throw std::invalid_argument {
      std::format("Argument `location.longitude` out of range [-180, 180], got {}", longitude_deg)
    };
  }
}

} // namespace detail

/**
 * @brief Calculate the Ascendant, Midheaven, and house cusps for an observer and instant.
 * @param jd_ut1 The Julian day on the UT1 scale.
 * @param jde_tt The Julian ephemeris day on the TT scale of the same physical instant.
 * @param location The observer's north-positive latitude and east-positive longitude.
 * @param system The house system to calculate.
 * @param model The nutation model used by both apparent sidereal time and true obliquity.
 * @return Tropical longitudes in the true ecliptic and equinox of date, normalized to [0°, 360°).
 * @throw std::invalid_argument If an input is non-finite or outside its range, `system` is not a
 *        named enumerator, or Placidus is requested at or beyond its polar domain.
 * @throw std::runtime_error If a Placidus cusp cannot be bracketed or solved.
 * @note `jd_ut1` and `jde_tt` must represent the same physical instant; their scale suffixes are the guard.
 * @see Jean Meeus, "Astronomical Algorithms", Second Edition, Chapters 12-14.
 */
[[nodiscard]] inline auto calculate(
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the jd_ut1/jde_tt naming is the UT1/TT guard (issue #41).
  const double jd_ut1,
  const double jde_tt,
  const astro::GeoLocation& location,
  const System system,
  const astro::earth::nutation::Model model = astro::earth::nutation::Model::IAU_1980
) -> Result {
  detail::validate(jd_ut1, jde_tt, location);
  const auto armc = astro::sidereal::local_apparent(jd_ut1, jde_tt, -location.longitude, model);
  const auto obliquity = astro::earth::obliquity::true_obliquity(jde_tt, model);
  return detail::calculate(armc, obliquity, location.latitude, system);
}

} // namespace astro::house
