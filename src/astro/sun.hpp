/*
 * CelestialCalendar: 
 *   A C++23-style library that performs astronomical calculations and date conversions between
 *   Gregorian and Chinese Lunar calendars.
 * 
 * Copyright (C) 2024 Ningqi Wang (0xf3cd)
 * Email: nq.maigre@gmail.com
 * Repo : https://github.com/0xf3cd/celestial-calendar
 *  
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cmath>
#include <chrono>
#include <vector>
#include <cstdint>
#include <functional>

#include "toolbox.hpp"
#include "julian_day.hpp"
#include "earth.hpp"
#include "event_search.hpp"
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
      using namespace toolbox::literals;
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


using Fk5Correction = astro::coords::Fk5Correction;


/**
 * @brief Compatibility entry for the VSOP87D-to-FK5 correction.
 * @param jde The julian ephemeris day number, which is based on TT.
 * @param vsop87d_coord Dynamical ecliptic coordinates at the same epoch.
 * @return The correction (i.e. Δlongitude and Δlatitude).
 * @see astro::coords::fk5_correction
 */
[[nodiscard]] inline auto fk5_correction(const double jde, const toolbox::SphericalCoordinate& vsop87d_coord) -> Fk5Correction {
  return astro::coords::fk5_correction(jde, vsop87d_coord);
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
  const auto correction = astro::coords::fk5_correction(jde, vsop_coord);

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
 * @see Jean Meeus, "Astronomical Algorithms", Second Edition, Chapters 13 and 25.
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

/**
 * @brief Return the JDE of the start of the year.
 * @param year The Gregorian year, in [1, 32767]; see @ref astro::julian_day::utc_year_start_jde.
 */
// #115: year boundaries are civil moments — resolve them via the UTC (leap-second-aware)
// path, matching `moments()` (#84). The UT1/UTC model gap (DUT1 ≤ 0.9 s in the leap era;
// ≤ ~21 h at year 6772 with the ΔAT table frozen) stays well under the ~4-day clearance
// between any jieqi and New Year — no year's attribution can move.
[[nodiscard]] inline auto get_start_jde(const int32_t year) -> double {
  return astro::julian_day::utc_year_start_jde(
    util::checked_year(year)
  );
}

/**
 * @brief Return the JDE of the end of the year.
 * @param year The Gregorian year, in [1, 32766]; see @ref astro::julian_day::validate_annual_year.
 */
[[nodiscard]] inline auto get_end_jde(const int32_t year) -> double {
  astro::julian_day::validate_annual_year(year);
  return astro::julian_day::utc_year_start_jde(std::chrono::year { year + 1 });
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


/**
 * @brief Lift solar longitude from the year's start for Newton refinement.
 * @note The mean-motion predictor must stay within 180 degrees of the continuous lift over the search span.
 */
[[nodiscard]] inline auto make_f(const int32_t year, const double expected_lon) {
  const double start_jde = get_start_jde(year);
  const double start_longitude = solar_longitude(start_jde) - astro::detail::event_search::LONGITUDE_PERIOD_DEG;
  return [=](const double jde) -> double {
    const double raw = astro::detail::event_search::canonical_longitude_difference(
      toolbox::AngleDeg { solar_longitude(jde) }
    ).deg();
    const double predicted = std::fma(toolbox::SOLAR_MEAN_MOTION_DEG_PER_DAY, jde - start_jde, start_longitude);
    const double turns = std::round((predicted - raw) / astro::detail::event_search::LONGITUDE_PERIOD_DEG);
    return (raw + (astro::detail::event_search::LONGITUDE_PERIOD_DEG * turns)) - expected_lon;
  };
}

// NOLINTEND(bugprone-easily-swappable-parameters)

} // namespace detail


// The `year` / `lon` names carry the contract at the call site.
// NOLINTBEGIN(bugprone-easily-swappable-parameters)

/**
 * @brief Return the count of the roots for the given `year` and `lon`.
 * @param year The Gregorian year, in [1, 32766]; see @ref astro::julian_day::validate_annual_year.
 * @param lon The expected solar longitude, in degrees.
 * @throw std::invalid_argument If `year` leaves [1, 32766].
 */
[[nodiscard]] inline auto discriminant(const int32_t year, const double lon) -> uint32_t {
  astro::julian_day::validate_annual_year(year);

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
 * @param year The Gregorian year, in [1, 32766]; see @ref astro::julian_day::validate_annual_year.
 * @param expected_lon The expected solar longitude, in degrees.
 * @return The roots (i.e. JDEs). There can be 0, 1 or 2 roots.
 * @throw std::invalid_argument If `year` leaves [1, 32766].
 */
[[nodiscard]] inline auto find_roots(const int32_t year, const double expected_lon) -> std::vector<double> {
  astro::julian_day::validate_annual_year(year);

  // Each predicate costs a full apparent-position evaluation at a year boundary. Going through
  // `discriminant` first would put the very same two questions a second time (#81).
  const bool root_before_equinox = detail::has_root_before_spring_equinox(year, expected_lon);
  const bool root_after_equinox  = detail::has_root_after_spring_equinox(year, expected_lon);

  // "nm" here denotes "newton_method".
  const auto apply_nm = [&](const auto& f) {
    const double start_jde = detail::get_start_jde(year);
    const double end_jde   = detail::get_end_jde(year);
    return toolbox::newton_method(
      f, start_jde, end_jde, toolbox::SOLAR_MEAN_MOTION_DEG_PER_DAY
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
