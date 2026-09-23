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

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <stdexcept>
#include <string_view>

#include "earth.hpp"
#include "julian_day.hpp"
#include "sun.hpp"
#include "toolbox.hpp"
#include "vsop87d/jupiter_coeff.hpp"
#include "vsop87d/mars_coeff.hpp"
#include "vsop87d/mercury_coeff.hpp"
#include "vsop87d/neptune_coeff.hpp"
#include "vsop87d/saturn_coeff.hpp"
#include "vsop87d/uranus_coeff.hpp"
#include "vsop87d/venus_coeff.hpp"

namespace astro::planet {

/** @brief A major planet whose geocentric position can be calculated. */
enum class Planet : uint8_t { MERCURY, VENUS, MARS, JUPITER, SATURN, URANUS, NEPTUNE };

namespace detail {

struct RectangularCoordinate {
  double x;
  double y;
  double z;
};

struct AberrationCorrection {
  astro::toolbox::AngleDeg Δλ;
  astro::toolbox::AngleDeg Δβ;
};

inline constexpr std::size_t LIGHT_TIME_MAX_ITERATIONS = 10;
// One day resolves outer-planet motion without extending the marker beyond a local direction test.
inline constexpr double RETROGRADE_DIFFERENCE_HALF_WIDTH_DAYS = 0.5;

[[nodiscard]] inline auto name(const Planet planet) -> std::string_view {
  switch (planet) {
    case Planet::MERCURY: return "Mercury";
    case Planet::VENUS:   return "Venus";
    case Planet::MARS:    return "Mars";
    case Planet::JUPITER: return "Jupiter";
    case Planet::SATURN:  return "Saturn";
    case Planet::URANUS:  return "Uranus";
    case Planet::NEPTUNE: return "Neptune";
    default:
      throw std::invalid_argument {
        std::format("Unknown planet {}", static_cast<uint32_t>(planet))
      };
  }
}

template <astro::vsop87d::Planet planet>
[[nodiscard]] inline auto heliocentric(const double jde_tt) -> astro::toolbox::SphericalCoordinate {
  const auto evaluated = astro::vsop87d::evaluate<planet>(astro::julian_day::jde_to_jm(jde_tt));
  return {
    .λ = astro::toolbox::AngleDeg { astro::toolbox::AngleRad { evaluated.λ }.normalize() },
    .β = astro::toolbox::AngleDeg { astro::toolbox::AngleRad { evaluated.β } },
    .r = astro::toolbox::DistanceAu { evaluated.r },
  };
}

[[nodiscard]] inline auto heliocentric(const Planet planet, const double jde_tt)
  -> astro::toolbox::SphericalCoordinate {
  switch (planet) {
    case Planet::MERCURY: return heliocentric<astro::vsop87d::Planet::MER>(jde_tt);
    case Planet::VENUS:   return heliocentric<astro::vsop87d::Planet::VEN>(jde_tt);
    case Planet::MARS:    return heliocentric<astro::vsop87d::Planet::MAR>(jde_tt);
    case Planet::JUPITER: return heliocentric<astro::vsop87d::Planet::JUP>(jde_tt);
    case Planet::SATURN:  return heliocentric<astro::vsop87d::Planet::SAT>(jde_tt);
    case Planet::URANUS:  return heliocentric<astro::vsop87d::Planet::URA>(jde_tt);
    case Planet::NEPTUNE: return heliocentric<astro::vsop87d::Planet::NEP>(jde_tt);
    default:
      throw std::invalid_argument { std::format("Unknown planet {}", static_cast<uint32_t>(planet)) };
  }
}

[[nodiscard]] inline auto rectangular(const astro::toolbox::SphericalCoordinate& spherical)
  -> RectangularCoordinate {
  const double cos_β = std::cos(spherical.β.rad());
  return {
    .x = spherical.r.au() * cos_β * std::cos(spherical.λ.rad()),
    .y = spherical.r.au() * cos_β * std::sin(spherical.λ.rad()),
    .z = spherical.r.au() * std::sin(spherical.β.rad()),
  };
}

/** @see Jean Meeus, "Astronomical Algorithms", Second Edition, (33.1)-(33.4). */
[[nodiscard]] inline auto light_time_corrected(
  const Planet planet,
  const double jde_tt,
  const astro::toolbox::SphericalCoordinate& earth
) -> astro::toolbox::SphericalCoordinate {
  const auto earth_rect = rectangular(earth);
  double retarded_jde_tt = jde_tt;

  for (std::size_t i = 0; i < LIGHT_TIME_MAX_ITERATIONS; ++i) {
    const auto target = heliocentric(planet, retarded_jde_tt);
    const auto target_rect = rectangular(target);
    const RectangularCoordinate geocentric {
      .x = target_rect.x - earth_rect.x,
      .y = target_rect.y - earth_rect.y,
      .z = target_rect.z - earth_rect.z,
    };
    const double distance_au = std::hypot(geocentric.x, geocentric.y, geocentric.z);
    if (not std::isfinite(distance_au)) [[unlikely]] {
      throw std::runtime_error {
        std::format(
          "Planetary light-time evaluation is not finite for {} at JDE(TT) {}",
          name(planet),
          jde_tt
        )
      };
    }

    const double next_retarded_jde_tt = jde_tt
                                      - (astro::earth::aberration::LIGHT_TIME_DAYS_PER_AU * distance_au);
    if (std::fabs(next_retarded_jde_tt - retarded_jde_tt) <= astro::toolbox::ulp(jde_tt)) {
      return {
        .λ = astro::toolbox::AngleDeg {
          astro::toolbox::rad_to_deg(std::atan2(geocentric.y, geocentric.x))
        }.normalize(),
        .β = astro::toolbox::AngleDeg {
          astro::toolbox::rad_to_deg(std::atan2(geocentric.z, std::hypot(geocentric.x, geocentric.y)))
        },
        .r = astro::toolbox::DistanceAu { distance_au },
      };
    }
    retarded_jde_tt = next_retarded_jde_tt;
  }

  throw std::runtime_error {
    std::format(
      "Planetary light-time iteration did not converge for {} at JDE(TT) {}",
      name(planet),
      jde_tt
    )
  };
}

/** @see Jean Meeus, "Astronomical Algorithms", Second Edition, (23.2). */
[[nodiscard]] inline auto aberration(
  const double jde_tt,
  const astro::toolbox::SphericalCoordinate& coordinate,
  const astro::toolbox::SphericalCoordinate& earth
) -> AberrationCorrection {
  const double jc = astro::julian_day::jde_to_jc(jde_tt);
  const double e = 0.016708634 + (jc * (-0.000042037 - (0.0000001267 * jc)));
  const astro::toolbox::AngleDeg π { 102.93735 + (jc * (1.71946 + (0.00046 * jc))) };
  const auto sun_λ = (earth.λ + astro::toolbox::AngleDeg { 180.0 }).normalize();
  const double λ = coordinate.λ.rad();
  const double β = coordinate.β.rad();

  const double Δλ_arcsec = 20.49552
                         * (-std::cos(sun_λ.rad() - λ) + (e * std::cos(π.rad() - λ)))
                         / std::cos(β);
  const double Δβ_arcsec = -20.49552 * std::sin(β)
                         * (std::sin(sun_λ.rad() - λ) - (e * std::sin(π.rad() - λ)));
  return {
    .Δλ = astro::toolbox::AngleDeg::from_arcsec(Δλ_arcsec),
    .Δβ = astro::toolbox::AngleDeg::from_arcsec(Δβ_arcsec),
  };
}

} // namespace detail

namespace geocentric_coord {

/**
 * @brief Calculate a major planet's apparent geocentric ecliptic position.
 * @param planet The planet to calculate, from Mercury through Neptune.
 * @param jde_tt The Julian Ephemeris Day based on TT.
 * @return Apparent longitude and latitude in the true ecliptic and equinox of date, and the
 *         retarded geocentric distance in AU.
 * @throw std::invalid_argument If `jde_tt` is not finite or `planet` is not a named enumerator.
 * @throw std::runtime_error If the numerical evaluation cannot produce a finite, converged position.
 * @note Includes light-time, annual aberration, FK5 reduction, and nutation. It omits relativistic
 *       light deflection and treats TT as the VSOP87D dynamical-time argument without a TT-TDB model.
 * @see Jean Meeus, "Astronomical Algorithms", Second Edition, (23.2), (32.3), and (33.1)-(33.4).
 */
[[nodiscard]] inline auto apparent(const Planet planet, const double jde_tt)
  -> astro::toolbox::SphericalCoordinate {
  if (not std::isfinite(jde_tt)) [[unlikely]] {
    throw std::invalid_argument {
      std::format("Argument `jde_tt` is not finite, got {}", jde_tt)
    };
  }
  const std::string_view planet_name = detail::name(planet);

  try {
    const auto earth = astro::earth::heliocentric_coord::vsop87d(jde_tt);
    const auto geometric = detail::light_time_corrected(planet, jde_tt, earth);
    const auto aberration = detail::aberration(jde_tt, geometric, earth);
    const astro::toolbox::SphericalCoordinate aberrated {
      .λ = geometric.λ + aberration.Δλ,
      .β = geometric.β + aberration.Δβ,
      .r = geometric.r,
    };
    const auto fk5 = astro::sun::geocentric_coord::fk5_correction(jde_tt, aberrated);
    const auto nutation = astro::earth::nutation::longitude(jde_tt);

    return {
      .λ = (aberrated.λ + fk5.Δλ + nutation).normalize(),
      .β = aberrated.β + fk5.Δβ,
      .r = aberrated.r,
    };
  } catch (const std::invalid_argument& error) {
    throw std::runtime_error {
      std::format(
        "Planetary apparent-position evaluation failed for {} at JDE(TT) {}: {}",
        planet_name,
        jde_tt,
        error.what()
      )
    };
  }
}

/**
 * @brief Return whether a planet's apparent geocentric longitude is decreasing.
 * @param planet The planet to calculate, from Mercury through Neptune.
 * @param jde_tt The Julian Ephemeris Day based on TT.
 * @return `true` when the wrap-aware longitude change over the centered one-day interval is negative.
 * @throw std::invalid_argument If `jde_tt` is not finite or `planet` is not a named enumerator.
 * @throw std::runtime_error If either apparent-position evaluation cannot produce a finite, converged result.
 * @note This marker does not solve for the exact stationary instant, where a boolean direction is not
 *       physically meaningful.
 */
[[nodiscard]] inline auto is_retrograde(const Planet planet, const double jde_tt) -> bool {
  const auto before = apparent(planet, jde_tt - detail::RETROGRADE_DIFFERENCE_HALF_WIDTH_DAYS);
  const auto after = apparent(planet, jde_tt + detail::RETROGRADE_DIFFERENCE_HALF_WIDTH_DAYS);
  return std::remainder(after.λ.deg() - before.λ.deg(), 360.0) < 0.0;
}

} // namespace geocentric_coord

} // namespace astro::planet
