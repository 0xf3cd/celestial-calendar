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
#include "julian_day.hpp"
#include "coord_transform.hpp"


namespace astro::earth::precession {

// Precession is the secular, conical motion of Earth's rotation axis (period ~25770 years): the
// celestial equator and the equinox points drift westward along the ecliptic by ~50.3"/year. It is
// the smooth counterpart to nutation (astro::earth::nutation), the short-period oscillation riding
// on top of it. This module carries a direction between two epochs — fixed-epoch catalogue
// positions (e.g. J2000) to the equinox of the date — precession only, no proper motion, nutation,
// or aberration. Note the VSOP87D solar/lunar pipeline does NOT need this: VSOP87D is already
// referred to the mean equinox of the date, so precession here is a standalone catalogue capability.

/// The three equatorial precession angles of Meeus (21.2): ζ (zeta), z, θ (theta), in degrees.
struct EquatorialAngles {
  astro::toolbox::AngleDeg ζ; // Meeus zeta
  astro::toolbox::AngleDeg z; // Meeus z
  astro::toolbox::AngleDeg θ; // Meeus theta
};

/// The three ecliptic precession quantities of Meeus Ch.21: η (eta), Π (Pi), p, in degrees.
struct EclipticAngles {
  astro::toolbox::AngleDeg η; // Meeus eta
  astro::toolbox::AngleDeg Π; // Meeus Pi — longitude of the ascending node of the moving ecliptic
  astro::toolbox::AngleDeg p; // Meeus p — general precession in longitude from the initial epoch
};


/**
 * @brief Compute the equatorial precession angles ζ, z, θ between two epochs.
 * @param jde_from The Julian ephemeris day of the initial epoch (pass J2000 for catalogue-to-date).
 * @param jde_to   The Julian ephemeris day of the final epoch.
 * @return {ζ, z, θ} in degrees; ζ and z nearly coincide for a small Δt, θ is the tilting angle.
 * @ref Jean Meeus, "Astronomical Algorithms", Second Edition, Chapter 21, Formula (21.2).
 */
[[nodiscard]] inline auto equatorial_angles(const double jde_from, const double jde_to) -> EquatorialAngles {
  using astro::toolbox::AngleDeg;
  using astro::julian_day::jde_to_jc;

  // T0: Julian centuries from J2000 to the initial epoch; t: centuries between the two epochs.
  const double t0 = jde_to_jc(jde_from);
  const double t  = (jde_to - jde_from) / 36525.0;

  // Meeus (21.2): ζ, z, θ in arcseconds. ζ and z share their leading term (2306.2181) and the
  // T₀-linear part (1.39656 T₀ − 0.000139 T₀²); they differ in the t-linear and t-quadratic parts.
  const double lead_ζz = 2306.2181 + (t0 * (1.39656 - (0.000139 * t0)));
  const double ζ_arcsec = t * (lead_ζz + (t * ((0.30188 - (0.000344 * t0)) + (0.017998 * t))));
  const double z_arcsec  = t * (lead_ζz + (t * ((1.09468 + (0.000066 * t0)) + (0.018203 * t))));
  const double θ_arcsec = t * (2004.3109
                             + (t0 * (-0.85330 - (0.000217 * t0)))
                             + (t * (-(0.42665 + (0.000217 * t0)) - (0.041833 * t))));

  return {
    .ζ = AngleDeg::from_arcsec(ζ_arcsec),
    .z = AngleDeg::from_arcsec(z_arcsec),
    .θ = AngleDeg::from_arcsec(θ_arcsec),
  };
}


/**
 * @brief Apply equatorial precession, carrying (α₀, δ₀) from one epoch to another.
 * @param α0 Right ascension at the initial epoch.
 * @param δ0 Declination at the initial epoch.
 * @param jde_from The Julian ephemeris day of the initial epoch.
 * @param jde_to   The Julian ephemeris day of the final epoch.
 * @return The precessed coordinates (α, δ); α is normalized to [0°, 360°), δ lies in [-90°, 90°].
 * @note This is precession only — proper motion, nutation, and aberration are not applied.
 * @note sin δ = C (Meeus 21.7); C is clamped to [-1, 1] against the roundoff that, exactly at the
 *       celestial pole, would push it just past ±1 and make asin return NaN.
 * @ref Jean Meeus, "Astronomical Algorithms", Second Edition, Chapter 21, Formula (21.7).
 */
[[nodiscard]] inline auto equatorial(
  const astro::toolbox::AngleDeg& α0,
  const astro::toolbox::AngleDeg& δ0,
  const double jde_from,
  const double jde_to
) -> astro::coords::EquatorialCoord {
  using astro::toolbox::AngleDeg;
  using astro::toolbox::rad_to_deg;

  const auto [ζ, z, θ] = equatorial_angles(jde_from, jde_to);

  const double α0_plus_ζ = α0.rad() + ζ.rad();
  const double cos_δ0 = std::cos(δ0.rad());
  const double sin_δ0 = std::sin(δ0.rad());
  const double cos_θ  = std::cos(θ.rad());
  const double sin_θ  = std::sin(θ.rad());

  // Meeus (21.7): tan(α − z) = A/B and sin δ = C, with α taken in atan2's quadrant.
  const double A = cos_δ0 * std::sin(α0_plus_ζ);
  const double B = (cos_θ * cos_δ0 * std::cos(α0_plus_ζ)) - (sin_θ * sin_δ0);
  const double C = (sin_θ * cos_δ0 * std::cos(α0_plus_ζ)) + (cos_θ * sin_δ0);

  const double α_rad = std::atan2(A, B) + z.rad();
  const double δ_rad = std::asin(std::clamp(C, -1.0, 1.0));

  return {
    .α = AngleDeg { rad_to_deg(α_rad) }.normalize(),
    .δ = AngleDeg { rad_to_deg(δ_rad) },
  };
}


/**
 * @brief Compute the ecliptic precession quantities η, Π, p between two epochs.
 * @param jde_from The Julian ephemeris day of the initial epoch (pass J2000 for catalogue-to-date).
 * @param jde_to   The Julian ephemeris day of the final epoch.
 * @return {η, Π, p} in degrees; Π carries the constant 174.876384°, p's linear term is the
 *         ~50.29"/year general precession in longitude.
 * @ref Jean Meeus, "Astronomical Algorithms", Second Edition, Chapter 21.
 */
[[nodiscard]] inline auto ecliptic_angles(const double jde_from, const double jde_to) -> EclipticAngles {
  using astro::toolbox::AngleDeg;
  using astro::toolbox::arcsec_to_deg;
  using astro::julian_day::jde_to_jc;

  const double t0 = jde_to_jc(jde_from);
  const double t  = (jde_to - jde_from) / 36525.0;

  // Meeus Ch.21 ecliptic method: η and p are t-scaled; Π's variable part is added to 174.876384°.
  const double η_arcsec = t * ((47.0029 + (t0 * (-0.06603 + (0.000598 * t0))))
                            + (t * ((-0.03302 + (0.000598 * t0)) + (0.00006 * t))));
  const double pie_var_arcsec = (t0 * (3289.4789 + (0.60622 * t0)))
                             + (t * (-(869.8089 + (0.50491 * t0)) + (0.03536 * t)));
  const double p_arcsec = t * (5029.0966
                            + (t0 * (2.22226 - (0.000042 * t0)))
                            + (t * (1.11113 - (0.000042 * t0) - (0.000006 * t))));

  // Π = 174°52'34.9824" + variable part; the constant is in degrees while the polynomial is in arcseconds.
  const AngleDeg Π { 174.876384 + arcsec_to_deg(pie_var_arcsec) };

  return {
    .η = AngleDeg::from_arcsec(η_arcsec),
    .Π = Π,
    .p = AngleDeg::from_arcsec(p_arcsec),
  };
}


/**
 * @brief Apply ecliptic precession, carrying (λ₀, β₀) from one epoch to another.
 * @param λ0 Ecliptic longitude at the initial epoch.
 * @param β0 Ecliptic latitude at the initial epoch.
 * @param jde_from The Julian ephemeris day of the initial epoch.
 * @param jde_to   The Julian ephemeris day of the final epoch.
 * @return The precessed coordinates (λ, β); λ is normalized to [0°, 360°), β lies in [-90°, 90°].
 * @note This is precession only — proper motion, nutation, and aberration are not applied. Unlike
 *       declination, ecliptic longitude of a fixed body increases monotonically under precession
 *       (~50"/year), since precession rotates the equinox along the ecliptic in one direction.
 * @ref Jean Meeus, "Astronomical Algorithms", Second Edition, Chapter 21.
 */
[[nodiscard]] inline auto ecliptic(
  const astro::toolbox::AngleDeg& λ0,
  const astro::toolbox::AngleDeg& β0,
  const double jde_from,
  const double jde_to
) -> astro::coords::EclipticCoord {
  using astro::toolbox::AngleDeg;
  using astro::toolbox::rad_to_deg;

  const auto [η, Π, p] = ecliptic_angles(jde_from, jde_to);

  const double Π_minus_λ0 = Π.rad() - λ0.rad();
  const double cos_β0 = std::cos(β0.rad());
  const double sin_β0 = std::sin(β0.rad());
  const double cos_η  = std::cos(η.rad());
  const double sin_η  = std::sin(η.rad());

  // Meeus Ch.21: λ = p + Π − atan2(A, B) and sin β = C.
  const double A = (cos_η * cos_β0 * std::sin(Π_minus_λ0)) - (sin_η * sin_β0);
  const double B = cos_β0 * std::cos(Π_minus_λ0);
  const double C = (cos_η * sin_β0) + (sin_η * cos_β0 * std::sin(Π_minus_λ0));

  const double λ_rad = p.rad() + Π.rad() - std::atan2(A, B);
  const double β_rad = std::asin(std::clamp(C, -1.0, 1.0));

  return {
    .λ = AngleDeg { rad_to_deg(λ_rad) }.normalize(),
    .β = AngleDeg { rad_to_deg(β_rad) },
  };
}


} // namespace astro::earth::precession
