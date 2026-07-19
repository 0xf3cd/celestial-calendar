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

#pragma once

#include <cmath>

#include "toolbox.hpp"


namespace astro::coords {

/**
 * @brief Represents a position in the equatorial coordinate system.
 * @note α is the right ascension, normalized to [0°, 360°). δ is the declination, in [-90°, 90°].
 */
struct EquatorialCoord {
  astro::toolbox::Angle<astro::toolbox::AngleUnit::DEG> α; // Right ascension
  astro::toolbox::Angle<astro::toolbox::AngleUnit::DEG> δ; // Declination
};

/**
 * @brief Represents a position in the horizontal coordinate system.
 * @note A is the azimuth, measured from the south point and positive westward (Meeus's convention),
 *       normalized to [0°, 360°). h is the altitude, in [-90°, 90°].
 */
struct HorizontalCoord {
  astro::toolbox::Angle<astro::toolbox::AngleUnit::DEG> A; // Azimuth, from the south, positive westward
  astro::toolbox::Angle<astro::toolbox::AngleUnit::DEG> h; // Altitude
};


/**
 * @brief Convert ecliptic coordinates (λ, β) to equatorial coordinates (α, δ).
 * @param λ The ecliptic longitude.
 * @param β The ecliptic latitude.
 * @param ε The obliquity of the ecliptic. Use the true obliquity (ε₀ + Δε) for apparent coordinates.
 * @return The equatorial coordinates (α, δ); α is normalized to [0°, 360°).
 * @note Meeus gives tan α in terms of tan β, which diverges at the ecliptic poles (β = ±90°).
 *       Here the numerator and denominator are multiplied through by cos β, so the result stays finite.
 * @ref Jean Meeus, "Astronomical Algorithms", Second Edition, Chapter 13, Formulas (13.3) and (13.4).
 */
inline auto ecliptic_to_equatorial(
  const astro::toolbox::Angle<astro::toolbox::AngleUnit::DEG>& λ,
  const astro::toolbox::Angle<astro::toolbox::AngleUnit::DEG>& β,
  const astro::toolbox::Angle<astro::toolbox::AngleUnit::DEG>& ε
) -> EquatorialCoord {
  using astro::toolbox::Angle;
  using astro::toolbox::AngleUnit::DEG;
  using astro::toolbox::rad_to_deg;

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
  const double y = sin_λ * cos_ε * cos_β - sin_β * sin_ε;
  const double x = cos_λ * cos_β;
  const double α_rad = std::atan2(y, x);

  // Meeus (13.4): sin δ = sin β cos ε + cos β sin ε sin λ. No division, safe everywhere.
  const double δ_rad = std::asin(sin_β * cos_ε + cos_β * sin_ε * sin_λ);

  return {
    .α = Angle<DEG> { rad_to_deg(α_rad) }.normalize(),
    .δ = Angle<DEG> { rad_to_deg(δ_rad) },
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
 * @note The resulting altitude is purely geometric: no atmospheric refraction, parallax, or horizon dip
 *       is taken into account (for refraction see Meeus Chapter 16; for parallax, Chapter 40).
 * @ref Jean Meeus, "Astronomical Algorithms", Second Edition, Chapter 13, Formulas (13.5) and (13.6).
 */
inline auto equatorial_to_horizontal(
  const astro::toolbox::Angle<astro::toolbox::AngleUnit::DEG>& H,
  const astro::toolbox::Angle<astro::toolbox::AngleUnit::DEG>& δ,
  const astro::toolbox::Angle<astro::toolbox::AngleUnit::DEG>& φ
) -> HorizontalCoord {
  using astro::toolbox::Angle;
  using astro::toolbox::AngleUnit::DEG;
  using astro::toolbox::rad_to_deg;

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
  const double x = cos_H * sin_φ * cos_δ - sin_δ * cos_φ;
  const double A_rad = std::atan2(y, x);

  // Meeus (13.6): sin h = sin φ sin δ + cos φ cos δ cos H. No division, safe everywhere.
  const double h_rad = std::asin(sin_φ * sin_δ + cos_φ * cos_δ * cos_H);

  return {
    .A = Angle<DEG> { rad_to_deg(A_rad) }.normalize(),
    .h = Angle<DEG> { rad_to_deg(h_rad) },
  };
}

} // namespace astro::coords
