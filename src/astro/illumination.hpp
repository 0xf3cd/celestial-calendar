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
#include <cmath>

#include "toolbox.hpp"

namespace astro::illumination {

/** @brief Observer-facing illumination geometry for a target body. */
struct Geometry {
  astro::toolbox::AngleDeg angular_separation;
  astro::toolbox::AngleDeg phase_angle;
  double illuminated_fraction;
};

namespace detail {

struct Separation {
  double cos_ψ;
  double sin_ψ;
};

[[nodiscard]] inline auto separation(
  const astro::toolbox::SphericalCoordinate& source_pos,
  const astro::toolbox::SphericalCoordinate& target_pos
) -> Separation {
  const double βs = source_pos.β.rad();
  const double βt = target_pos.β.rad();
  const double Δλ = (target_pos.λ - source_pos.λ).rad();
  if (Δλ == 0.0 and βs == βt) {
    return { .cos_ψ = 1.0, .sin_ψ = 0.0 };
  }

  const double sin_βs = std::sin(βs);
  const double cos_βs = std::cos(βs);
  const double sin_βt = std::sin(βt);
  const double cos_βt = std::cos(βt);

  const double cos_ψ = std::clamp((sin_βs * sin_βt) + (cos_βs * cos_βt * std::cos(Δλ)), -1.0, 1.0);
  // The equivalent cross-product norm preserves separations too small to survive 1 - cos²ψ.
  const double sin_ψ = std::clamp(
    std::hypot(
      cos_βt * std::sin(Δλ),
      (cos_βs * sin_βt) - (sin_βs * cos_βt * std::cos(Δλ))
    ),
    0.0,
    1.0
  );
  return {
    .cos_ψ = cos_ψ,
    .sin_ψ = sin_ψ,
  };
}

[[nodiscard]] inline auto fraction(const astro::toolbox::AngleDeg& phase_angle) -> double {
  return (1.0 + std::cos(phase_angle.rad())) / 2.0;
}

} // namespace detail

/**
 * @brief Calculate the angular separation, target-centered phase angle, and illuminated fraction.
 * @param source_pos The illuminating body's observer-centered spherical position.
 * @param target_pos The target body's position in the same spherical frame as `source_pos`.
 * @return The target's observer-facing illumination geometry.
 * @note Distances must share one unit; the library's spherical coordinates carry AU.
 * @note Meeus prints these equations for the Moon; the distance-triangle identities are target-neutral,
 *       with the planetary analogues discussed in Chapter 41. His ecliptic form beneath (48.2)
 *       omits the Sun's latitude; the full spherical separation here keeps both latitudes.
 * @see Jean Meeus, "Astronomical Algorithms", Second Edition, (48.1)-(48.3).
 */
[[nodiscard]] inline auto geometry(
  const astro::toolbox::SphericalCoordinate& source_pos,
  const astro::toolbox::SphericalCoordinate& target_pos
) -> Geometry {
  const auto ψ = detail::separation(source_pos, target_pos);
  const auto angular_separation = astro::toolbox::AngleDeg {
    astro::toolbox::AngleRad { std::atan2(ψ.sin_ψ, ψ.cos_ψ) }
  };
  const double R_km = source_pos.r.km();
  const double Δ_km = target_pos.r.km();
  const double one_minus_cos_ψ = ψ.cos_ψ < 0.0
    ? 1.0 - ψ.cos_ψ
    : (ψ.sin_ψ * ψ.sin_ψ) / (1.0 + ψ.cos_ψ);
  const auto phase_angle = astro::toolbox::AngleDeg {
    astro::toolbox::AngleRad {
      std::atan2(R_km * ψ.sin_ψ, (Δ_km - R_km) + (R_km * one_minus_cos_ψ))
    }
  };

  return {
    .angular_separation = angular_separation,
    .phase_angle = phase_angle,
    .illuminated_fraction = detail::fraction(phase_angle),
  };
}

} // namespace astro::illumination
