/*
 * CelestialCalendar: 
 *   A C++23-style library that performs astronomical calculations and date conversions among various calendars,
 *   including Gregorian, Lunar, and Chinese Ganzhi calendars.
 * 
 * Copyright (C) 2024 Ningqi Wang (0xf3cd)
 * Email: nq.maigre@gmail.com
 * Repo : https://github.com/0xf3cd/celestial-calendar
 *  
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cmath>
#include <vector>
#include <cstdint>
#include <functional>

#include "toolbox.hpp"
#include "julian_day.hpp"
#include "earth.hpp"
#include "coord_transform.hpp"
#include "ymd.hpp"

namespace astro::sun::geocentric_coord {

/**
 * @brief Calculate the geocentric position of the Sun, using VSOP87D.
 * @param jde The Julian Ephemeris Day.
 * @return The geocentric ecliptic position of the Sun, calculated using VSOP87D.
 * @details The function invokes `astro::earth::heliocentric_coord::vsop87d`, and
 *          transforms the heliocentric coordinates to geocentric coordinates.
 */
[[nodiscard]] inline auto vsop87d(const double jde) -> toolbox::SphericalCoordinate {
  const auto& [λ_helio, β_helio, r_helio] = astro::earth::heliocentric_coord::vsop87d(jde);
  return {
    // Convert the heliocentric ecliptic longitude of Earth to geocentric ecliptic longitude of Sun.
    // The formula is: λ_sun_geocentric_deg = λ_earth_heliocentric_deg + 180∘
    .λ = std::invoke([&] {
      using namespace astro::toolbox::literals;
      const auto sum = λ_helio + 180.0_deg;
      return sum.normalize();
    }),

    // Convert the heliocentric ecliptic latitude of Earth to geocentric ecliptic latitude of Sun.
    // The formula is: β_sun_geocentric_deg = -β_earth_heliocentric_deg
    .β = -β_helio,

    // The distance (radius) is the same for both Sun and Earth.
    .r = r_helio 
  };
}


/** @brief The FK5 correction for the coordinate calculated using VSOP87D. */
struct Fk5Correction {
  toolbox::AngleDeg Δλ;
  toolbox::AngleDeg Δβ;
};


/**
 * @brief Calculate the correction for the VSOP87D coordinate, in order to convert it to FK5 system.
 * @param jde The julian ephemeris day number, which is based on TT.
 * @return The correction (i.e. Δlongitude and Δlatitude).
 * @details As per Jean Meeus's Astronomical Algorithms, this correction is applied for accuracy.
 */
[[nodiscard]] inline auto fk5_correction(const double jde, const toolbox::SphericalCoordinate& vsop87d_coord) -> Fk5Correction {
  const double jc = astro::julian_day::jde_to_jc(jde);
  const auto& [vsop_λ, vsop_β, vsop_r] = vsop87d_coord;

  // Calculate the deltas for longitude and latitude, in arcsec.
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
 * @brief Calculate the apparent geocentric position of the Sun, using VSOP87D. 
 *        The position is corrected to FK5 system, considering nutation and aberration. 
 * @param jde The julian ephemeris day number, which is based on TT.
 * @return The geocentric ecliptic position of the Sun, after correction.
 */
[[nodiscard]] inline auto apparent(const double jde) -> toolbox::SphericalCoordinate {
  // Use VSOP87D to calculate the geocentric ecliptic position of the Sun.
  const auto vsop_coord = vsop87d(jde);

  // Calculate the correction for the VSOP87D result, in order to convert it to FK5 system.
  const auto correction = fk5_correction(jde, vsop_coord);

  // Calculate the Earth's nutation in longitude.
  const auto nutation = astro::earth::nutation::longitude(jde);

  // Calculate the Solar aberration, Meeus (25.11).
  const auto aberration = astro::earth::aberration::compute(jde, vsop_coord.r);

  // Calculate the adjusted longitude.
  const auto λ = vsop_coord.λ + correction.Δλ + nutation - aberration;

  // Calculate the adjusted latitude.
  const auto β = vsop_coord.β + correction.Δβ;

  return {
    .λ = λ.normalize(),
    .β = β,
    .r = vsop_coord.r
  };
}

} // namespace astro::sun::geocentric_coord


namespace astro::sun::equatorial_coord {

/**
 * @brief Calculate the apparent geocentric equatorial coordinates of the Sun.
 * @param jde The julian ephemeris day number, which is based on TT.
 * @return The apparent right ascension α (normalized to [0°, 360°)) and declination δ.
 * @details Composes `geocentric_coord::apparent` (ecliptic λ, β, with FK5 + nutation + aberration)
 *          with the true obliquity ε and Meeus (13.3)–(13.4) via `coords::ecliptic_to_equatorial`.
 *          The result is of-date apparent equatorial place of the Sun, suitable as input to
 *          sunrise/sunset hour-angle calculations (Phase 5).
 * @ref Jean Meeus, "Astronomical Algorithms", Second Edition, Chapters 13 and 25.
 */
[[nodiscard]] inline auto apparent(const double jde) -> astro::coords::EquatorialCoord {
  const auto ecl = astro::sun::geocentric_coord::apparent(jde);
  const auto ε = astro::earth::obliquity::true_obliquity(jde);
  return astro::coords::ecliptic_to_equatorial(ecl.λ, ecl.β, ε);
}

} // namespace astro::sun::equatorial_coord


namespace astro::sun::geocentric_coord::math {

// In this namespace, we use Newton's method to approximate the JDE,
// at which time the Sun reaches a given geocentric longitude in a year.
//
// The solar-longitude notebook landed in 202a0bd (#12), Newton inversion in 397422cc (#13), and
// a second Moon-phase copy in 066c28db (#34). Commit 5101b6d2 (#95) introduced the shared toolbox
// helper and best-iterate/bracket policy. The lower nong page supplied conceptual and implementation
// ideas for the solar-longitude solver in this namespace; the upper page is a historical
// Sun-coordinate reference. This record does not claim copied code or byte identity. The rise/set
// golden-section solver landed separately in bc79c991 (#199).
//
// Given a year and a geocentric longitude, our goal is to find the JDE(s) that satisfy the following condition:
// 1. The JDE(s) must fall in the given year.
// 2. At the JDE(s), the Sun's geocentric longitude is equal to the given longitude.
// 
// In our context, a JDE that satisfies the above conditions is called "a root".
//
// For a given year and a given geocentric longitude, there can be 0, 1, or 2 roots.

/** 
 * @see https://github.com/0xf3cd/celestial-calendar/blob/main/statistics/sun_longitude.ipynb
 * @see https://github.com/leetcola/nong/wiki/算法系列之十八：用天文方法计算二十四节气（上）
 * @see https://github.com/leetcola/nong/wiki/算法系列之十八：用天文方法计算二十四节气（下）
 */


namespace detail {

/**
 * @brief Calculate the apparent geocentric longitude of the Sun.
 * @param jde The julian ephemeris day number, which is based on TT.
 * @return The apparent geocentric longitude of the Sun in degrees.
 * @note The solver's own view of the Sun: `newton_method` works on bare doubles, and this is
 *       where the typed astronomy layer is handed over to it. Callers after a solar longitude
 *       want `geocentric_coord::apparent(jde).λ`, which keeps the unit in the type (#125).
 */
[[nodiscard]] inline auto solar_longitude(const double jde) -> double {
  return astro::sun::geocentric_coord::apparent(jde).λ.deg();
}

/** @brief Return the JDE of the start of the year. */
// #115: year boundaries are civil moments — resolve them via the UTC (leap-second-aware)
// path, matching `moments()` (#84). The UT1/UTC model gap (DUT1 ≤ 0.9 s in the leap era;
// ≤ ~21 h at year 6772 with the ΔAT table frozen) stays well under the ~4-day clearance
// between any jieqi and New Year — no year's attribution can move.
[[nodiscard]] inline auto get_start_jde(const int32_t year) -> double {
  return astro::julian_day::utc_to_jde(calendar::Datetime { util::to_ymd(year, 1, 1), 0.0 });
}

/** @brief Return the JDE of the end of the year. */
[[nodiscard]] inline auto get_end_jde(const int32_t year) -> double {
  return astro::julian_day::utc_to_jde(calendar::Datetime { util::to_ymd(year + 1, 1, 1), 0.0 });
}

/** @brief Return the apparent geocentric longitude of the Sun at the start of the year. */
[[nodiscard]] inline auto get_start_lon(const int32_t year) -> double {
  return solar_longitude(get_start_jde(year));
}

/** @brief Return the apparent geocentric longitude of the Sun at the end of the year. */
[[nodiscard]] inline auto get_end_lon(const int32_t year) -> double {
  return solar_longitude(get_end_jde(year));
}

// The `year` / `lon` names carry the contract at the call site.
// NOLINTBEGIN(bugprone-easily-swappable-parameters)

/** @brief Return true if the given year has a root for the given `lon` before the spring equinox. */
[[nodiscard]] inline auto has_root_before_spring_equinox(const int32_t year, const double lon) -> bool {
  const double start_lon = get_start_lon(year);
  return start_lon <= lon and lon < 360.0;
}

/** @brief Return true if the given year has a root for the given `lon` after the spring equinox. */
[[nodiscard]] inline auto has_root_after_spring_equinox(const int32_t year, const double lon) -> bool {
  const double end_lon = get_end_lon(year);
  return 0.0 <= lon and lon < end_lon;
}


// In Newton's method, we will approximate the root with the previous root, iteratively.
// The formula is: Xn+1 = Xn - f(Xn) / f'(Xn).
// Where we can use the following formula to approximate f'(x):
// f'(x) = (f(x+h) - f(x-h)) / (2*h), where h is the central-difference half-width picked by `toolbox::newton_method`.
//
// As mentioned before, our goal is to find the root (JDE) at which the Sun reaches the expected longitude in a given year.
// In our context, f is defined as:
// f(jde) = solar_longitude(jde) - expected_lon,
// and f is defined on a half-open interval [start_jde(year), end_jde(year)).
//
// Newton's method requires f to be differentiable (smooth).
// So we need to modify the function of `solar_longitude`.
// Because the beginning position of Sun in a year is roughly 280.0 degrees, and we need to make it negative.
// Actually, for any JDE before Spring Equinox this year, we need to subtract 360 from `solar_longitude`'s result to make f smooth.
// 
// So the actual f is defined as:
// f(jde) = modified_solar_longitude(jde) - expected_lon

/** @brief Return a `f` that we can apply Newton's method to. */
[[nodiscard]] inline auto make_f(const int32_t year, const double expected_lon) {
  const double apr_1st_jde = astro::julian_day::ut1_to_jde(calendar::Datetime { util::to_ymd(year, 4, 1), 0.0 });

  const auto modified_solar_longitude = [=](const double jde) -> double {
    const double raw_value = solar_longitude(jde);

    // We mostly want to subtract 360.0 from those JDEs before Spring Equinox.
    //
    // We are here using the fact that the beginning of the year is roughly 280.0 degrees,
    // and it continues growing to 360 degrees (which is also 0 degrees) until Spring Equinox.
    // 
    // After Spring Equinox, it grows from 0 to ~280.0 degrees again (the last day of the year), 
    // and then next year comes.

    if (jde < apr_1st_jde and raw_value >= 250.0) {
      return raw_value - 360.0;
    }

    return raw_value; 
  };

  return [=](const double jde) -> double {
    return modified_solar_longitude(jde) - expected_lon;
  };
}

// NOLINTEND(bugprone-easily-swappable-parameters)

} // namespace detail


// The `year` / `lon` names carry the contract at the call site.
// NOLINTBEGIN(bugprone-easily-swappable-parameters)

/** @brief Return the count of the roots for the given `year` and `lon`. */
[[nodiscard]] inline auto discriminant(const int32_t year, const double lon) -> uint32_t {
  uint32_t count = 0;

  if (detail::has_root_before_spring_equinox(year, lon)) {
    count++;
  }
  if (detail::has_root_after_spring_equinox(year, lon)) {
    count++;
  }

  return count;
}

/**
 * @brief Find the roots (i.e. JDEs) for the given `year` and `expected_lon`.
 * @param year The year, in gregorian calendar.
 * @param expected_lon The expected solar longitude, in degrees.
 * @return The roots (i.e. JDEs). There can be 0, 1 or 2 roots.
 */
[[nodiscard]] inline auto find_roots(const int32_t year, const double expected_lon) -> std::vector<double> {
  // Each predicate costs a full apparent-position evaluation at a year boundary. Going through
  // `discriminant` first would put the very same two questions a second time (#81).
  const bool root_before_equinox = detail::has_root_before_spring_equinox(year, expected_lon);
  const bool root_after_equinox  = detail::has_root_after_spring_equinox(year, expected_lon);

  // "nm" here denotes "newton_method".
  const auto apply_nm = [&](const auto& f) {
    const double start_jde = detail::get_start_jde(year);
    const double end_jde   = detail::get_end_jde(year);
    return astro::toolbox::newton_method(
      f, start_jde, end_jde, astro::toolbox::SOLAR_MEAN_MOTION_DEG_PER_DAY
    );
  };

  std::vector<double> roots;

  // If there is a root before Spring Equinox, it means that
  // after modification (for the sake of differentiability of f),
  // the solar longitudes before spring equinox will be negative.
  // And accordingly, we need to subtract 360.0 from the expected_lon.
  if (root_before_equinox) {
    roots.push_back(apply_nm(detail::make_f(year, expected_lon - 360.0)));
  }

  // If there is a root after Spring Equinox, it means that
  // after modification (for the sake of differentiability of f),
  // the solar longitudes after spring equinox will be positive.
  // And accordingly, we have no need to modify the expected_lon.
  if (root_after_equinox) {
    roots.push_back(apply_nm(detail::make_f(year, expected_lon)));
  }

  return roots;
}

// NOLINTEND(bugprone-easily-swappable-parameters)

} // namespace astro::sun::geocentric_coord::math
