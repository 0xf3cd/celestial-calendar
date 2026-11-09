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

#include <chrono>
#include <cstdint>
#include <format>
#include <stdexcept>

#include "ephemeris.hpp"
#include "observer.hpp"
#include "rise_set.hpp"
#include "toolbox.hpp"

namespace astro::rise_set::planet {

/**
 * @brief Standard geometric altitude of a planet's centre at rise/set: -34 arcminutes.
 * @note The point-source convention includes standard horizon refraction, without a limb correction.
 *       Pair it with airless topocentric coordinates, with parallax already applied.
 * @see Jean Meeus, "Astronomical Algorithms", Second Edition, Chapter 15, p. 101.
 */
inline constexpr auto STANDARD_CENTER_ALTITUDE = toolbox::AngleDeg::from_arcmin(-34.0);

namespace detail {

/** @brief Bind a planetary target and WGS84 observer to the engine's fixed-default time conversion. */
[[nodiscard]] inline auto provider_for(const astro::observer::Observer& observer, const ephemeris::Target target) {
  if (not ephemeris::target_facts(target).planet.has_value()) {
    throw std::invalid_argument {
      std::format("Planetary rise/set requires Mercury through Pluto, got target {}", static_cast<uint32_t>(target))
    };
  }

  return [observer, target](const double jde_tt) -> coords::EquatorialCoord {
    const julian_day::Times times {
      .jd_ut1 = rise_set::detail::jde_tt_to_jd_ut1(jde_tt),
      .jde_tt = jde_tt,
    };
    const auto position = astro::observer::observe(times, observer, target);
    if (not position.right_ascension.has_value()) {
      throw std::runtime_error {
        std::format("Planetary rise/set: topocentric right ascension is unavailable at JDE(TT) {}", jde_tt)
      };
    }
    return { .α = *position.right_ascension, .δ = position.declination };
  };
}

} // namespace detail

/**
 * @brief Compute planetary rise, upper transit and set for one UT1 calendar day.
 * @param ymd_ut1 The date whose [0h, 24h) UT1 window owns the returned events.
 * @param observer WGS84 site, including signed ellipsoidal elevation.
 * @param target Mercury through Pluto; Sun, Moon, lunar nodes and unknown targets are rejected.
 * @param h0 Geometric altitude of the topocentric centre, in [-90, 90] degrees.
 * @return The representative TT events and polar topology of @ref calculate_day.
 * @note Fixed IAU1980 axes and the engine's default Delta T (@ref delta_t::DEFAULT_MODEL).
 *       Parallax is applied in the coordinates. Standard refraction is in the default threshold;
 *       coordinates are airless. Elevation changes the site vector, not the horizon plane.
 * @note Every provider sample must be supported, including the day endpoints, transit estimates
 *       and Newton derivative samples outside their brackets. Pluto's TT position domain is
 *       therefore not a promise that a date whose midnight is in that domain can be queried.
 * @throw std::invalid_argument For invalid input, a rejected provider sample or time conversion.
 * @throw std::runtime_error For failed evaluation, unresolved RA or a failed event solve.
 */
[[nodiscard]] inline auto calculate(
  const std::chrono::year_month_day& ymd_ut1,
  const astro::observer::Observer& observer,
  const ephemeris::Target target,
  const toolbox::AngleDeg& h0 = STANDARD_CENTER_ALTITUDE
) -> Result {
  try {
    const auto provider = detail::provider_for(observer, target);
    return calculate_day(ymd_ut1, observer.location, h0, provider);
  } catch (const std::invalid_argument& error) {
    throw std::invalid_argument {
      std::format(
        "Planetary rise/set for UT1 date {:04}-{:02}-{:02}: {}",
        static_cast<int32_t>(ymd_ut1.year()),
        static_cast<uint32_t>(ymd_ut1.month()),
        static_cast<uint32_t>(ymd_ut1.day()),
        error.what()
      )
    };
  }
}

} // namespace astro::rise_set::planet
