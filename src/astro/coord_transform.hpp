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

#include "julian_day.hpp"
#include "toolbox.hpp"


namespace astro::coords {

/**
 * @brief Represents a position in the equatorial coordinate system.
 * @note α is the right ascension, normalized to [0°, 360°). δ is the declination, in [-90°, 90°].
 */
struct EquatorialCoord {
  toolbox::AngleDeg α; // Right ascension
  toolbox::AngleDeg δ; // Declination
};

/**
 * @brief Represents a position in the ecliptic coordinate system.
 * @note λ is the ecliptic longitude, normalized to [0°, 360°). β is the ecliptic latitude, in [-90°, 90°].
 */
struct EclipticCoord {
  toolbox::AngleDeg λ; // Ecliptic longitude
  toolbox::AngleDeg β; // Ecliptic latitude
};

/**
 * @brief Represents a position in the horizontal coordinate system.
 * @note A is the azimuth, measured from the south point and positive westward (Meeus's convention),
 *       normalized to [0°, 360°). h is the altitude, in [-90°, 90°].
 */
struct HorizontalCoord {
  toolbox::AngleDeg A; // Azimuth, from the south, positive westward
  toolbox::AngleDeg h; // Altitude
};


/** @brief Ecliptic longitude and latitude corrections from VSOP87D to FK5. */
struct Fk5Correction {
  toolbox::AngleDeg Δλ;
  toolbox::AngleDeg Δβ;
};

/**
 * @brief Calculate the FK5 correction for dynamical ecliptic coordinates of date.
 * @param jde_tt Julian Ephemeris Day in TT.
 * @param vsop87d_coord Dynamical ecliptic coordinates at the same epoch; radius is unused.
 * @return Additive longitude and latitude corrections.
 * @see Jean Meeus, "Astronomical Algorithms", Second Edition, Formula (32.3);
 *      Chapter 33, page 225 for geocentric coordinates.
 */
[[nodiscard]] inline auto fk5_correction(
  const double jde_tt,
  const toolbox::SphericalCoordinate& vsop87d_coord
) -> Fk5Correction {
  const double jc = astro::julian_day::jde_to_jc(jde_tt);
  const auto& [vsop_λ, vsop_β, vsop_r] = vsop87d_coord;

  const toolbox::AngleDeg λ_dash = vsop_λ - toolbox::AngleDeg { (1.397 + (0.00031 * jc)) * jc };
  const double λ_dash_rad = λ_dash.rad();

  const double delta_λ_arcsec = -0.09033 + (0.03916 * (std::cos(λ_dash_rad) + std::sin(λ_dash_rad)) * std::tan(vsop_β.rad()));
  const double delta_β_arcsec = 0.03916 * (std::cos(λ_dash_rad) - std::sin(λ_dash_rad));

  return {
    .Δλ = toolbox::AngleDeg::from_arcsec(delta_λ_arcsec),
    .Δβ = toolbox::AngleDeg::from_arcsec(delta_β_arcsec),
  };
}


/**
 * @brief Convert ecliptic coordinates (λ, β) to equatorial coordinates (α, δ).
 * @param λ The ecliptic longitude.
 * @param β The ecliptic latitude.
 * @param ε The obliquity of the ecliptic. Use the true obliquity (ε₀ + Δε) for apparent coordinates.
 * @return The equatorial coordinates (α, δ); α is normalized to [0°, 360°).
 * @note Meeus gives tan α in terms of tan β, which diverges at the ecliptic poles (β = ±90°).
 *       Here the numerator and denominator are multiplied through by cos β, so the result stays finite.
 * @note Assumes β ∈ [-90°, 90°], as ecliptic latitude is by definition; outside that range cos β < 0
 *       and the cos β-multiplied form silently shifts α by 180°.
 * @see Jean Meeus, "Astronomical Algorithms", Second Edition, Chapter 13, Formulas (13.3) and (13.4).
 */
[[nodiscard]] inline auto ecliptic_to_equatorial(
  const toolbox::AngleDeg& λ,
  const toolbox::AngleDeg& β,
  const toolbox::AngleDeg& ε
) -> EquatorialCoord {
  using toolbox::AngleDeg;
  using toolbox::rad_to_deg;

  const double λ_rad = λ.rad();
  const double β_rad = β.rad();
  const double ε_rad = ε.rad();

  const double sin_λ = std::sin(λ_rad);
  const double cos_λ = std::cos(λ_rad);
  const double sin_β = std::sin(β_rad);
  const double cos_β = std::cos(β_rad);
  const double sin_ε = std::sin(ε_rad);
  const double cos_ε = std::cos(ε_rad);

  // Meeus (13.3): tan α = (sin λ cos ε − tan β sin ε) / cos λ, with α taken in the same quadrant as λ.
  // Multiplying through by cos β keeps it finite at β = ±90°.
  const double y = (sin_λ * cos_ε * cos_β) - (sin_β * sin_ε);
  const double x = cos_λ * cos_β;
  const double α_rad = std::atan2(y, x);

  // Meeus (13.4): sin δ = sin β cos ε + cos β sin ε sin λ. The argument is exactly ±1 at the
  // celestial poles, where roundoff can push it just outside asin's domain (yielding NaN) —
  // clamp it into [-1, 1], and then this really is safe everywhere.
  const double δ_rad = std::asin(std::clamp((sin_β * cos_ε) + (cos_β * sin_ε * sin_λ), -1.0, 1.0));

  return {
    .α = AngleDeg { rad_to_deg(α_rad) }.normalize(),
    .δ = AngleDeg { rad_to_deg(δ_rad) },
  };
}

/**
 * @brief Convert equatorial coordinates (H, δ) to horizontal coordinates (A, h) for an observer at latitude φ.
 * @param H The local hour angle of the object, measured westward from the meridian.
 * @param δ The declination of the object.
 * @param φ The observer's geographic latitude.
 * @return The horizontal coordinates (A, h); A is measured from the south point, positive westward,
 *         and normalized to [0°, 360°).
 * @note Meeus gives tan A in terms of tan δ, which diverges at the celestial poles (δ = ±90°).
 *       Here the numerator and denominator are multiplied through by cos δ, so the result stays finite.
 * @note Assumes δ ∈ [-90°, 90°] and φ ∈ [-90°, 90°], as declination and geographic latitude are by
 *       definition; outside the former range the cos δ-multiplied form silently shifts A by 180°.
 * @note The resulting altitude is purely geometric: no atmospheric refraction, parallax, or horizon dip
 *       is taken into account (for refraction see Meeus Chapter 16; for parallax, Chapter 40).
 * @see Jean Meeus, "Astronomical Algorithms", Second Edition, Chapter 13, Formulas (13.5) and (13.6).
 */
[[nodiscard]] inline auto equatorial_to_horizontal(
  const toolbox::AngleDeg& H,
  const toolbox::AngleDeg& δ,
  const toolbox::AngleDeg& φ
) -> HorizontalCoord {
  using toolbox::AngleDeg;
  using toolbox::rad_to_deg;

  const double H_rad = H.rad();
  const double δ_rad = δ.rad();
  const double φ_rad = φ.rad();

  const double sin_H = std::sin(H_rad);
  const double cos_H = std::cos(H_rad);
  const double sin_δ = std::sin(δ_rad);
  const double cos_δ = std::cos(δ_rad);
  const double sin_φ = std::sin(φ_rad);
  const double cos_φ = std::cos(φ_rad);

  // Meeus (13.5): tan A = sin H / (cos H sin φ − tan δ cos φ), azimuth from the south, positive westward.
  // Multiplying through by cos δ keeps it finite at δ = ±90°.
  const double y = sin_H * cos_δ;
  const double x = (cos_H * sin_φ * cos_δ) - (sin_δ * cos_φ);
  const double A_rad = std::atan2(y, x);

  // Meeus (13.6): sin h = sin φ sin δ + cos φ cos δ cos H. The argument is exactly ±1 at the zenith
  // and nadir, where roundoff can push it just outside asin's domain (a few percent of near-zenith
  // evaluations on common libms, yielding NaN) — clamp it into [-1, 1], then this is safe everywhere.
  const double h_rad = std::asin(std::clamp((sin_φ * sin_δ) + (cos_φ * cos_δ * cos_H), -1.0, 1.0));

  return {
    .A = AngleDeg { rad_to_deg(A_rad) }.normalize(),
    .h = AngleDeg { rad_to_deg(h_rad) },
  };
}

} // namespace astro::coords
