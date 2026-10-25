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
#include <limits>
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

/** @brief Default nutation model for house geometry. */
inline constexpr astro::earth::nutation::Model DEFAULT_NUTATION_MODEL = astro::earth::nutation::Model::IAU_1980;

/** @brief The four principal tropical angles, independent of a house division system. */
struct Axes {
  toolbox::AngleDeg ascendant;
  toolbox::AngleDeg midheaven;
  toolbox::AngleDeg descendant;
  toolbox::AngleDeg imum_coeli;
};

/**
 * @brief The four principal angles and twelve cusps of a house calculation.
 * @note `descendant` and `imum_coeli` are the respective antipodes of `ascendant` and
 *       `midheaven`. `cusps[0]` through `cusps[11]` are the first through twelfth house cusps.
 *       Placidus places the four angles at the first, tenth, seventh, and fourth house cusps,
 *       respectively; Equal places the Ascendant and Descendant at the first and seventh house
 *       cusps, while Whole Sign cusps follow sign boundaries independently of the angles. An
 *       Ascendant exactly on a sign boundary belongs to the following sign.
 */
struct Result {
  toolbox::AngleDeg ascendant;
  toolbox::AngleDeg midheaven;
  toolbox::AngleDeg descendant;
  toolbox::AngleDeg imum_coeli;
  std::array<toolbox::AngleDeg, 12> cusps;
};

namespace detail {

inline constexpr std::size_t PLACIDUS_BRACKET_SEGMENTS = 360;
inline constexpr double PLACIDUS_ROOT_TOLERANCE_DEG = 1e-11;
inline constexpr std::size_t PLACIDUS_MAX_ITERATIONS = 64;
inline constexpr double ASCENDANT_ROUNDOFF_FACTOR = 32.0 * std::numeric_limits<double>::epsilon();

enum class PlacidusCusp : uint8_t { TWO, THREE, ELEVEN, TWELVE };

/** @see Jean Meeus, "Astronomical Algorithms", Second Edition, Chapter 13, Formula (13.3), with β = 0. */
[[nodiscard]] inline auto midheaven(
  const toolbox::AngleDeg& armc,
  const toolbox::AngleDeg& obliquity
) -> toolbox::AngleDeg {
  return toolbox::AngleDeg {
    toolbox::rad_to_deg(
      std::atan2(std::sin(armc.rad()), std::cos(armc.rad()) * std::cos(obliquity.rad()))
    )
  }.normalize();
}

[[nodiscard]] inline auto antipode(const toolbox::AngleDeg& angle) -> toolbox::AngleDeg {
  return (angle + toolbox::AngleDeg { 180.0 }).normalize();
}

/** @see Jean Meeus, "Astronomical Algorithms", Second Edition, Chapter 14, Formulas (14.2)-(14.3). */
// NOLINTBEGIN(bugprone-easily-swappable-parameters): angle names carry distinct physical roles in the formula.
[[nodiscard]] inline auto ascendant(
  const toolbox::AngleDeg& armc,
  const toolbox::AngleDeg& latitude,
  const toolbox::AngleDeg& obliquity
) -> toolbox::AngleDeg {
  const auto normalized_armc = armc.normalize();
  // Preserve cos θ = 0 at exact quadrature; one-ulp drift can cross a Whole Sign boundary (#59).
  const double cos_armc = (normalized_armc.deg() == 90.0 or normalized_armc.deg() == 270.0)
                        ? 0.0
                        : std::cos(normalized_armc.rad());
  const double latitude_term = std::sin(obliquity.rad()) * std::tan(latitude.rad());
  const double armc_term = std::cos(obliquity.rad()) * std::sin(normalized_armc.rad());
  const double atan2_y = -cos_armc;
  const double atan2_x = latitude_term + armc_term;

  // Both the intersection line and its rising antipode must be resolved above round-off (#321).
  const double intersection_scale = 1.0 + std::abs(latitude_term) + std::abs(armc_term);
  const double east_first = std::cos(latitude.rad()) * std::cos(obliquity.rad());
  const double east_second = std::sin(latitude.rad()) * std::sin(obliquity.rad()) * std::sin(normalized_armc.rad());
  // E = -(horizon normal × ecliptic normal) · local east.
  const double E = east_first + east_second;
  const double rising_scale = 1.0 + std::abs(east_first) + std::abs(east_second);
  if (std::hypot(atan2_x, atan2_y) <= ASCENDANT_ROUNDOFF_FACTOR * intersection_scale
      or std::abs(E) <= ASCENDANT_ROUNDOFF_FACTOR * rising_scale) {
    throw std::invalid_argument {
      std::format(
        "Ascendant direction is unresolved at ARMC {} degrees, latitude {} degrees, obliquity {} degrees",
        normalized_armc.deg(),
        latitude.deg(),
        obliquity.deg()
      )
    };
  }

  const auto horizon_intersection = toolbox::AngleDeg {
    toolbox::rad_to_deg(
      std::atan2(atan2_y, atan2_x)
    )
  }.normalize();
  const auto equatorial = astro::coords::ecliptic_to_equatorial(
    horizon_intersection,
    toolbox::AngleDeg { 0.0 },
    obliquity
  );
  const auto hour_angle = (armc - equatorial.α).normalize();

  // Formula (14.2) gives either antipode inside the polar circles; the rising branch has H >= 180°.
  if (hour_angle.deg() >= 180.0) {
    return horizon_intersection;
  }
  return antipode(horizon_intersection);
}
// NOLINTEND(bugprone-easily-swappable-parameters)

/**
 * @brief Evaluate one Placidus semi-arc equation on its continuous longitude branch.
 * @see Swiss Ephemeris General Documentation, Section 6.2.1, "Placidus" (semi-arc definition) -
 *      https://www.astro.com/swisseph/swisseph.htm
 */
// NOLINTBEGIN(bugprone-easily-swappable-parameters): angle names carry distinct physical roles in the equation.
[[nodiscard]] inline auto placidus_equation(
  const double longitude_deg,
  const toolbox::AngleDeg& latitude,
  const toolbox::AngleDeg& obliquity,
  const PlacidusCusp cusp
) -> double {
  const auto equatorial = astro::coords::ecliptic_to_equatorial(
    toolbox::AngleDeg { longitude_deg },
    toolbox::AngleDeg { 0.0 },
    obliquity
  );
  double right_ascension_deg = equatorial.α.deg();
  // α wraps at 360° while λ scans one continuous branch; lift α to keep the equation seam-free.
  if (longitude_deg > 180.0 and right_ascension_deg < 180.0) {
    right_ascension_deg += 360.0;
  }

  const double semidiurnal_arc_deg = toolbox::rad_to_deg(
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
  const toolbox::AngleDeg& armc,
  const toolbox::AngleDeg& latitude,
  const toolbox::AngleDeg& obliquity,
  const PlacidusCusp cusp,
  const std::size_t max_iterations = PLACIDUS_MAX_ITERATIONS
) -> toolbox::AngleDeg {
  const double start_value = placidus_equation(0.0, latitude, obliquity, cusp);
  const double end_value = placidus_equation(360.0, latitude, obliquity, cusp);
  double target_deg = armc.normalize().deg();
  // Put the target on the same 360° branch as the endpoint values before sign-bracketing.
  while (target_deg < std::min(start_value, end_value)) {
    target_deg += 360.0;
  }
  while (target_deg > std::max(start_value, end_value)) {
    target_deg -= 360.0;
  }

  const auto residual = [&latitude, &obliquity, cusp, target_deg](const double longitude_deg) {
    return placidus_equation(longitude_deg, latitude, obliquity, cusp) - target_deg;
  };
  const auto refine = [&residual, max_iterations](
    double left_deg,
    double left_residual,
    double right_deg
  ) -> toolbox::AngleDeg {
    for (std::size_t iteration = 0; iteration < max_iterations; ++iteration) {
      const double midpoint_deg = std::midpoint(left_deg, right_deg);
      const double midpoint_residual = residual(midpoint_deg);
      if (std::fabs(midpoint_residual) <= PLACIDUS_ROOT_TOLERANCE_DEG
          or (right_deg - left_deg) <= PLACIDUS_ROOT_TOLERANCE_DEG) {
        return toolbox::AngleDeg { midpoint_deg }.normalize();
      }
      if (std::signbit(left_residual) == std::signbit(midpoint_residual)) {
        left_deg = midpoint_deg;
        left_residual = midpoint_residual;
      } else {
        right_deg = midpoint_deg;
      }
    }

    throw std::runtime_error { "Placidus house-cusp solver did not converge" };
  };

  double left_deg = 0.0;
  double left_residual = start_value - target_deg;
  if (std::fabs(left_residual) <= PLACIDUS_ROOT_TOLERANCE_DEG) {
    return toolbox::AngleDeg { 0.0 };
  }

  for (std::size_t segment = 1; segment <= PLACIDUS_BRACKET_SEGMENTS; ++segment) {
    const double right_deg = 360.0 * static_cast<double>(segment) / static_cast<double>(PLACIDUS_BRACKET_SEGMENTS);
    const double right_residual = residual(right_deg);
    if (std::fabs(right_residual) <= PLACIDUS_ROOT_TOLERANCE_DEG) {
      return toolbox::AngleDeg { right_deg }.normalize();
    }
    if (std::signbit(left_residual) != std::signbit(right_residual)) {
      return refine(left_deg, left_residual, right_deg);
    }
    left_deg = right_deg;
    left_residual = right_residual;
  }

  throw std::runtime_error { "Failed to bracket a Placidus house cusp" };
}
// NOLINTEND(bugprone-easily-swappable-parameters)

// NOLINTBEGIN(bugprone-easily-swappable-parameters): angle names carry distinct physical roles in the house frame.
[[nodiscard]] inline auto placidus_cusps(
  const toolbox::AngleDeg& armc,
  const toolbox::AngleDeg& latitude,
  const toolbox::AngleDeg& obliquity,
  const toolbox::AngleDeg& ascendant,
  const toolbox::AngleDeg& midheaven
) -> std::array<toolbox::AngleDeg, 12> {
  const double obliquity_deg = obliquity.deg();
  if (std::fabs(latitude.deg()) >= 90.0 - obliquity_deg) {
    throw std::invalid_argument {
      std::format(
        "Placidus is undefined at latitude {} degrees for obliquity {} degrees",
        latitude.deg(),
        obliquity_deg
      )
    };
  }

  const auto cusp2 = placidus_cusp(armc, latitude, obliquity, PlacidusCusp::TWO);
  const auto cusp3 = placidus_cusp(armc, latitude, obliquity, PlacidusCusp::THREE);
  const auto cusp11 = placidus_cusp(armc, latitude, obliquity, PlacidusCusp::ELEVEN);
  const auto cusp12 = placidus_cusp(armc, latitude, obliquity, PlacidusCusp::TWELVE);
  return {
    ascendant,
    cusp2,
    cusp3,
    antipode(midheaven),
    antipode(cusp11),
    antipode(cusp12),
    antipode(ascendant),
    antipode(cusp2),
    antipode(cusp3),
    midheaven,
    cusp11,
    cusp12,
  };
}
// NOLINTEND(bugprone-easily-swappable-parameters)

[[nodiscard]] inline auto uniform_cusps(const toolbox::AngleDeg& first) -> std::array<toolbox::AngleDeg, 12> {
  const auto cusp = [&first](const double offset_deg) {
    return (first + toolbox::AngleDeg { offset_deg }).normalize();
  };
  return {
    cusp(0.0),   cusp(30.0),  cusp(60.0),  cusp(90.0),  cusp(120.0), cusp(150.0),
    cusp(180.0), cusp(210.0), cusp(240.0), cusp(270.0), cusp(300.0), cusp(330.0),
  };
}

[[nodiscard]] inline auto whole_sign_start(const toolbox::AngleDeg& ascendant) -> toolbox::AngleDeg {
  return toolbox::AngleDeg { std::floor(ascendant.normalize().deg() / 30.0) * 30.0 };
}

/** @brief Resolve the horizon/ecliptic angles before any cusp construction. */
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): Angle names distinguish the house-frame roles.
[[nodiscard]] inline auto axes(
  const toolbox::AngleDeg& armc,
  const toolbox::AngleDeg& latitude,
  const toolbox::AngleDeg& obliquity
) -> Axes {
  const auto asc = ascendant(armc, latitude, obliquity);
  const auto mc = midheaven(armc, obliquity);
  return { .ascendant = asc, .midheaven = mc, .descendant = antipode(asc), .imum_coeli = antipode(mc) };
}

[[nodiscard]] inline auto calculate(
  const toolbox::AngleDeg& armc,
  const toolbox::AngleDeg& latitude,
  const toolbox::AngleDeg& obliquity,
  const System system
) -> Result {
  const double obliquity_deg = obliquity.deg();
  if (system == System::PLACIDUS
      and (not std::isfinite(obliquity_deg) or obliquity_deg <= 0.0 or obliquity_deg >= 90.0)) {
    throw std::invalid_argument {
      std::format("Placidus requires obliquity in (0, 90) degrees, got {}", obliquity_deg)
    };
  }

  const auto [asc, mc, desc, ic] = axes(armc, latitude, obliquity);

  switch (system) {
    case System::EQUAL:
      return {
        .ascendant = asc,
        .midheaven = mc,
        .descendant = desc,
        .imum_coeli = ic,
        .cusps = uniform_cusps(asc),
      };
    case System::WHOLE_SIGN:
      return {
        .ascendant = asc,
        .midheaven = mc,
        .descendant = desc,
        .imum_coeli = ic,
        .cusps = uniform_cusps(whole_sign_start(asc)),
      };
    case System::PLACIDUS:
      return {
        .ascendant = asc,
        .midheaven = mc,
        .descendant = desc,
        .imum_coeli = ic,
        .cusps = placidus_cusps(armc, latitude, obliquity, asc, mc),
      };
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
 * @brief Calculate the four principal angles without constructing house cusps.
 * @param jd_ut1 Julian day on UT1.
 * @param jde_tt JDE(TT) of the same physical instant.
 * @param location North-positive latitude in (-90, 90), east-positive longitude in [-180, 180].
 * @param model Nutation for both apparent sidereal time and true obliquity.
 * @return The four axes, normalized to [0, 360) degrees.
 * @throw std::invalid_argument For invalid time/site/model input or an unresolved Ascendant direction.
 * @note The numeric operation has no civil-year or target/rate window.
 */
[[nodiscard]] inline auto axes(
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): The scale suffixes distinguish the same-instant pair.
  const double jd_ut1,
  const double jde_tt,
  const astro::GeoLocation& location,
  const astro::earth::nutation::Model model = DEFAULT_NUTATION_MODEL
) -> Axes {
  detail::validate(jd_ut1, jde_tt, location);

  const auto armc = astro::sidereal::local_apparent(jd_ut1, jde_tt, -location.longitude, model);
  const auto obliquity = astro::earth::obliquity::true_obliquity(jde_tt, model);
  return detail::axes(armc, location.latitude, obliquity);
}

/**
 * @brief Calculate the four principal angles and house cusps for an observer and instant.
 * @param jd_ut1 The Julian day on the UT1 scale.
 * @param jde_tt The Julian ephemeris day on the TT scale of the same physical instant.
 * @param location The observer's north-positive latitude in (-90°, 90°) and east-positive
 *        longitude in [-180°, 180°].
 * @param system The house system to calculate.
 * @param model The nutation model used by both apparent sidereal time and true obliquity. Defaults
 *        to DEFAULT_NUTATION_MODEL.
 * @return Tropical longitudes in the true ecliptic and equinox of date, normalized to [0°, 360°).
 * @throw std::invalid_argument If an input is non-finite or outside its range, `system` is not a
 *        named enumerator, the Ascendant direction is unresolved at round-off, or Placidus is
 *        requested with a non-physical obliquity or at or beyond its polar domain.
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
  const astro::earth::nutation::Model model = DEFAULT_NUTATION_MODEL
) -> Result {
  detail::validate(jd_ut1, jde_tt, location);
  // `local_apparent` wants Meeus's west-positive longitude; `GeoLocation` carries east-positive.
  const auto armc = astro::sidereal::local_apparent(jd_ut1, jde_tt, -location.longitude, model);
  const auto obliquity = astro::earth::obliquity::true_obliquity(jde_tt, model);
  return detail::calculate(armc, location.latitude, obliquity, system);
}

} // namespace astro::house
