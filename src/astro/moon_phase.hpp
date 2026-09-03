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
#include <format>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>

#include "toolbox.hpp"
#include "ymd.hpp"
#include "datetime.hpp"
#include "julian_day.hpp"
#include "coord_transform.hpp"

#include "sun.hpp"
#include "moon.hpp"


namespace astro::moon_phase::new_moon {

// In our context, the conjunction is the moment when the Sun and the Moon are at the same apparent longitude,
// which is also called "New Moon". In Chinese, this is called "朔", "合朔", or "新月".


/**
 * @brief Calculate the difference between the apparent longitudes of the Moon and the Sun.
 * @param jde The Julian Ephemeris Day.
 * @return The normalized difference between the apparent longitudes of the Moon and the Sun, in degrees.
 * @see VSOP87D, ELP2000-82B, and Astronomical Algorithms, Jean Meeus, 1998.
 */
[[nodiscard]] inline auto longitude_diff(const double jde) -> double {
  const auto sun_apparent_lon = astro::sun::geocentric_coord::apparent(jde).λ;
  const auto moon_apparent_lon = astro::moon::geocentric_coord::apparent(jde).λ;
  const auto diff = moon_apparent_lon - sun_apparent_lon;
  return diff.normalize().deg();
}


/**
 * @brief How near conjunction a JDE must sit before it can serve as a bracket endpoint,
 *        measured in degrees of Moon-Sun elongation.
 * @note The same figure sets where `f` is unwrapped below, so the bracket that Newton's method
 *       accepts and the interval on which `f` is smooth are one and the same by construction.
 */
inline constexpr double BRACKET_TOLERANCE_DEG = 15.0;

/**
 * @brief Apply Newton's method to find the jde, when the Sun and Moon are at the same apparent longitude.
 * @param left_jde The left bound of the search, inclusive.
 * @param right_jde The right bound of the search, exclusive.
 * @param iterations The maximum number of iterations.
 * @return The jde of the conjunction.
 * @note It is the caller's responsibility to ensure the root exists in the range of [left_jde, right_jde).
 * @throw std::invalid_argument If no root exists in the range of [left_jde, right_jde).
 */
[[nodiscard]] inline auto newton_method(
  const double left_jde,
  const double right_jde,            // NOLINT(bugprone-easily-swappable-parameters)
  const std::size_t iterations = astro::toolbox::NEWTON_MAX_ITERATIONS
) -> double {

  // Make sure the root exists in the range of [left_jde, right_jde).
  const double left_diff = longitude_diff(left_jde);
  const double right_diff = longitude_diff(right_jde);

  if (left_diff <= 360.0 - BRACKET_TOLERANCE_DEG or right_diff >= BRACKET_TOLERANCE_DEG) [[unlikely]] {
    throw std::invalid_argument {
      std::format(
        "No root between jde {} (elongation {} deg) and jde {} (elongation {} deg).",
        left_jde, left_diff, right_jde, right_diff
      )
    };
  }

  // Define the function `f` which is differentiable.
  // We are going to find the root where `f` evaluates to 0.
  // Just before conjunction the Moon still trails the Sun by nearly a full turn, so the raw
  // difference jumps from 360 back to 0 across the root; unwrapping it keeps `f` smooth there.
  const auto f = [](const double jde) -> double {
    const double diff = longitude_diff(jde);
    if (diff > 360.0 - BRACKET_TOLERANCE_DEG) {
      return diff - 360.0;
    }
    return diff;
  };

  return astro::toolbox::newton_method(
    f, left_jde, right_jde, astro::toolbox::MOON_ELONGATION_RATE_DEG_PER_DAY, iterations
  );
}


/**
 * @brief How far the mean-rate extrapolation may miss conjunction and still be worth refining,
 *        in degrees of Moon-Sun elongation.
 * @note Refining divides the miss by the mean rate, so a miss of this size leaves the estimate
 *       about `30 / 12.19 * 0.19` = 0.47 day short of the root — which is what makes
 *       `BRACKET_HALF_WIDTH_DAYS` the binding constraint on how large this may be.
 */
inline constexpr double ESTIMATE_TOLERANCE_DEG = 30.0;

/**
 * @brief Half the width of the bracket handed to Newton's method, in days.
 * @note Chosen between two bounds. Too narrow and the bracket stops containing the root, since
 *       refining leaves an error of up to 0.47 day at the tolerance above. Too wide and the
 *       endpoints drift past `BRACKET_TOLERANCE_DEG`: at the Moon's fastest, 14.5 deg/day, an
 *       endpoint 0.67 day out already sits 9.7 deg from conjunction.
 */
inline constexpr double BRACKET_HALF_WIDTH_DAYS = 0.5;

/**
 * @brief Approximate the range of the first root after the given jde, when the Sun and Moon are at the same apparent longitude.
 * @param jde The jde.
 * @return The range of the first root after the given `jde`.
 * @throw std::invalid_argument If the extrapolation lands further than `ESTIMATE_TOLERANCE_DEG`
 *        from conjunction, i.e. too far off to be worth refining.
 */
[[nodiscard]] inline auto first_root_range_after(const double jde) -> std::pair<double, double> {
  const double cur_diff = longitude_diff(jde);
  const double gap = 360.0 - cur_diff;

  constexpr double deg_per_day = astro::toolbox::MOON_ELONGATION_RATE_DEG_PER_DAY;
  const double est_jde = jde + (gap / deg_per_day); // Estimate the next root jde.

  // Extrapolating at the mean rate lands near the root but not on it: the Moon's true rate runs
  // between about 10.5 and 14.5 deg/day, so a month of it can miss by several degrees either way.
  // Signed, so that overshooting the conjunction reads positive and falling short reads negative.
  const double est_jde_diff = longitude_diff(est_jde);
  const double signed_miss_deg = (est_jde_diff > 180.0) ? est_jde_diff - 360.0 : est_jde_diff;

  if (std::fabs(signed_miss_deg) > ESTIMATE_TOLERANCE_DEG) [[unlikely]] {
    throw std::invalid_argument {
      std::format(
        "Cannot find the first root after jde {}: the estimate at jde {} misses conjunction "
        "by {} deg (elongation {} deg).",
        jde, est_jde, signed_miss_deg, est_jde_diff
      )
    };
  }

  // Step the estimate onto the root before bracketing it. Bracketing the estimate directly would
  // leave the endpoints however far out the extrapolation happened to miss, plus whatever the
  // rate varied over the bracket -- measured, that put an endpoint within 0.67 deg of the
  // tolerance Newton's method demands. Refining first makes the endpoints' distance from
  // conjunction a property of `BRACKET_HALF_WIDTH_DAYS` instead, so the bracket the solver
  // accepts is one this function builds by construction rather than by luck.
  const double root_est = est_jde - (signed_miss_deg / deg_per_day);
  return { root_est - BRACKET_HALF_WIDTH_DAYS, root_est + BRACKET_HALF_WIDTH_DAYS };
}


/**
 * @brief Find the next root jde.
 * @param jde The jde. This is expected to be a root.
 * @return The next root jde.
 */
[[nodiscard]] inline auto next_root(const double jde) -> double {
  const double jde_lon_diff = longitude_diff(jde);
  if (1.0 < jde_lon_diff and jde_lon_diff < 359.0) [[unlikely]] {
    throw std::invalid_argument {
      std::format("The jde {} is not a root.", jde)
    };
  }

  const auto next_root_range = first_root_range_after(jde + 1.0); // Add 1.0 in case `jde_lon_diff` falls into [359.0, 360.0).
  const auto [left, right] = next_root_range;
  return newton_method(left, right);
}

/**
 * @brief Generator for finding the roots (i.e. conjunction moments of the Sun and Moon).
 */
// TODO: Use `std::generator` once every CI leg has it (./checks.py --features).
struct RootGenerator {
private:
  double _root;

public:
  explicit RootGenerator(const double start_jde) {
    const auto [left, right] = first_root_range_after(start_jde);
    const double first_root = newton_method(left, right);
    _root = first_root;
  }

  [[nodiscard]] auto next() -> double {
    const double root = _root;
    _root = next_root(_root);
    return root;
  }
};


/**
 * @brief Calculate conjunctions moments of the Sun and Moon in a given Gregorian year.
          计算某一个公历年中日月合朔的时刻。
 * @param year The Gregorian year, in [1, 32766].
 * @return The vector of the conjunction moments, in JDE (Julian Ephemeris Day).
 * @throw std::invalid_argument if `year` is outside [1, 32766].
 * @note The year runs from Jan 1 to Jan 1 in UTC; before 1972 the bounds degrade to UT1.
 * @note A conjunction falling exactly on Jan 1 00:00:00 UTC has no defined owner: whether the
 *       generator seeded at that instant returns it or skips to the next moon turns on
 *       floating-point noise in the longitude difference. Left as is on purpose (#127) —
 *       the set of such instants has measure zero, and no test can pin one.
 * @details The Sun's position is calculated using VSOP87D,
 * @details The Moon's position is calculated using truncated ELP2000-82B.
 * @see VSOP87D, ELP2000-82B, and Astronomical Algorithms, Jean Meeus, 1998.
 */
[[nodiscard]] inline auto moments(const int32_t year) -> std::vector<double> {
  if (year < 1 or year > 32766) {
    throw std::invalid_argument {
      std::format("Year {} is out of range [1, 32766].", year)
    };
  }

  // The first moment of the year, inclusive.
  const calendar::Datetime start_moment_utc {
    util::to_ymd(year, 1, 1),
    0.0,
  };

  // The last moment of the year, exclusive.
  const calendar::Datetime end_moment_utc {
    util::to_ymd(year + 1, 1, 1),
    0.0,
  };

  // #84: the bounds are UTC, not UT1 — a conjunction between the two midnights used to be
  // attributed to the neighbouring year.
  const auto start_jde = astro::julian_day::utc_to_jde(start_moment_utc);
  const auto end_jde = astro::julian_day::utc_to_jde(end_moment_utc);

  RootGenerator gen(start_jde);
  std::vector<double> roots;

  while (true) {
    const auto root = gen.next();
    if (root >= end_jde) {
      break;
    }

    roots.push_back(root);
  }

  return roots;
}

} // namespace astro::moon_phase::new_moon


namespace astro::moon_phase::phase_moments {

// The four principal Moon phases, defined by the Moon-Sun apparent geocentric ecliptic
// longitude difference Δλ: New Moon = 0°, First Quarter = 90°, Full Moon = 180°,
// Last Quarter = 270°. Meeus, Astronomical Algorithms, Chapter 49.

/** @brief The four principal Moon phases. */
enum class PhaseKind : uint8_t {
  NEW_MOON = 0,
  FIRST_QUARTER = 1,
  FULL_MOON = 2,
  LAST_QUARTER = 3,
};

/** @brief The target elongation for a given phase, in degrees. */
[[nodiscard]] inline auto target_angle_deg(const PhaseKind kind) -> double {
  switch (kind) {
    case PhaseKind::NEW_MOON:       return 0.0;
    case PhaseKind::FIRST_QUARTER:  return 90.0;
    case PhaseKind::FULL_MOON:      return 180.0;
    case PhaseKind::LAST_QUARTER:   return 270.0;
  }
  std::unreachable();
}

/**
 * @brief Moon-Sun apparent longitude difference normalized to [0°, 360°).
 * @param jde The Julian Ephemeris Day.
 * @return The normalized difference, in degrees.
 */
[[nodiscard]] inline auto longitude_diff(const double jde) -> double {
  return astro::moon_phase::new_moon::longitude_diff(jde);
}

/**
 * @brief Signed difference from the target elongation, folded to [-180°, 180°).
 * @param jde The Julian Ephemeris Day.
 * @param target_deg The target elongation, in degrees.
 * @return The signed, folded difference.
 * @note Folding keeps `f` smooth across the root for any target, not just 0°.
 */
[[nodiscard]] inline auto phase_diff(const double jde, const double target_deg) -> double {
  return astro::toolbox::normalize_pm180(longitude_diff(jde) - target_deg);
}

/**
 * @brief How near the target elongation a bracket endpoint must sit before Newton's method
 *        accepts it, in degrees.
 */
inline constexpr double BRACKET_TOLERANCE_DEG = 15.0;

/**
 * @brief How far the mean-rate extrapolation may miss the root and still be worth refining,
 *        in degrees.
 */
inline constexpr double ESTIMATE_TOLERANCE_DEG = 30.0;

/**
 * @brief Half the width of the bracket handed to Newton's method, in days.
 * @note Same value as `new_moon::BRACKET_HALF_WIDTH_DAYS`; the mean-rate miss dominates
 *       the bracket choice, and that rate is the same for all four phases.
 */
inline constexpr double BRACKET_HALF_WIDTH_DAYS = 0.5;

/**
 * @brief Apply Newton's method to find the JDE at which the Moon-Sun elongation equals
 *        the target value.
 * @param left_jde The left bound of the search, inclusive.
 * @param right_jde The right bound of the search, exclusive.
 * @param target_deg The target elongation, in degrees.
 * @param iterations The maximum number of iterations.
 * @return The JDE of the phase moment.
 * @throw std::invalid_argument If no root exists in the range for the given target.
 */
// The `_jde` / `_deg` / `_iterations` suffixes carry the contract at the call site.
// NOLINTBEGIN(bugprone-easily-swappable-parameters)
[[nodiscard]] inline auto newton_method(
  const double left_jde,
  const double right_jde,
  const double target_deg,
  const std::size_t iterations = astro::toolbox::NEWTON_MAX_ITERATIONS
) -> double {
// NOLINTEND(bugprone-easily-swappable-parameters)

  // The root is between left and right when left trails the target (negative signed diff)
  // and right leads it (positive signed diff). A margin of BRACKET_TOLERANCE_DEG keeps the
  // endpoints safely inside the smooth branch of `phase_diff`.
  const double left_diff = phase_diff(left_jde, target_deg);
  const double right_diff = phase_diff(right_jde, target_deg);

  if (left_diff >= 0.0 or right_diff <= 0.0
      or std::fabs(left_diff) >= BRACKET_TOLERANCE_DEG
      or std::fabs(right_diff) >= BRACKET_TOLERANCE_DEG) [[unlikely]] {
    throw std::invalid_argument {
      std::format(
        "No root between jde {} (elongation {} deg) and jde {} (elongation {} deg) for target {} deg.",
        left_jde, left_diff, right_jde, right_diff, target_deg
      )
    };
  }

  const auto f = [target_deg](const double jde) -> double {
    return phase_diff(jde, target_deg);
  };

  return astro::toolbox::newton_method(
    f, left_jde, right_jde, astro::toolbox::MOON_ELONGATION_RATE_DEG_PER_DAY, iterations
  );
}

/**
 * @brief Approximate the range of the first root after the given JDE for a target elongation.
 * @param jde The JDE.
 * @param target_deg The target elongation, in degrees.
 * @return The bracket [left, right) containing the first root after `jde`.
 * @throw std::invalid_argument If the mean-rate estimate lands too far from the target.
 */
[[nodiscard]] inline auto first_root_range_after(const double jde, const double target_deg) -> std::pair<double, double> {
  const double cur_diff = longitude_diff(jde);

  // The signed distance to the next target, measured along the increasing elongation.
  const double gap = (target_deg > cur_diff)
                   ? (target_deg - cur_diff)
                   : (360.0 - cur_diff + target_deg);

  constexpr double deg_per_day = astro::toolbox::MOON_ELONGATION_RATE_DEG_PER_DAY;
  const double est_jde = jde + (gap / deg_per_day);

  const double est_jde_diff = phase_diff(est_jde, target_deg);
  if (std::fabs(est_jde_diff) > ESTIMATE_TOLERANCE_DEG) [[unlikely]] {
    throw std::invalid_argument {
      std::format(
        "Cannot find the first root after jde {} for target {} deg: the estimate at jde {} "
        "misses the target by {} deg (elongation {} deg).",
        jde, target_deg, est_jde, est_jde_diff, longitude_diff(est_jde)
      )
    };
  }

  const double root_est = est_jde - (est_jde_diff / deg_per_day);
  return { root_est - BRACKET_HALF_WIDTH_DAYS, root_est + BRACKET_HALF_WIDTH_DAYS };
}

/**
 * @brief Find the next root after a known root for the same target elongation.
 * @param jde The known root JDE.
 * @param target_deg The target elongation, in degrees.
 * @return The next root JDE.
 * @throw std::invalid_argument If `jde` is not a root for the target.
 */
[[nodiscard]] inline auto next_root(const double jde, const double target_deg) -> double {
  if (std::fabs(phase_diff(jde, target_deg)) > 1.0) [[unlikely]] {
    throw std::invalid_argument {
      std::format("The jde {} is not a root for target {} deg.", jde, target_deg)
    };
  }

  const auto [left, right] = first_root_range_after(jde + 1.0, target_deg);
  return newton_method(left, right, target_deg);
}

/**
 * @brief Generator for finding phase moments of a given kind.
 */
// TODO: Use `std::generator` once every CI leg has it (./checks.py --features).
struct RootGenerator {
private:
  double _root;
  double _target;

public:
  explicit RootGenerator(const double start_jde, const PhaseKind kind)
    : _target { target_angle_deg(kind) } {
    const auto [left, right] = first_root_range_after(start_jde, _target);
    _root = newton_method(left, right, _target);
  }

  [[nodiscard]] auto next() -> double {
    const double root = _root;
    _root = next_root(_root, _target);
    return root;
  }
};

/**
 * @brief Calculate the moments of a given Moon phase in a Gregorian year.
 * @param year The Gregorian year, in [1, 32766].
 * @param kind The phase kind.
 * @return The phase moments in the year, in JDE (TT-scale ephemeris time).
 * @throw std::invalid_argument if `year` is outside [1, 32766].
 * @note The year runs from Jan 1 to Jan 1 in UTC; before 1972 the bounds degrade to UT1.
 * @note A phase falling exactly on Jan 1 00:00:00 UTC has no defined owner: whether the
 *       generator seeded at that instant returns it or skips to the next moon turns on
 *       floating-point noise in the longitude difference. Left as is on purpose (#127) —
 *       the set of such instants has measure zero, and no test can pin one.
 * @details Positions: VSOP87D (Sun) and truncated ELP2000-82B (Moon), both apparent geocentric.
 */
[[nodiscard]] inline auto moments(const int32_t year, const PhaseKind kind) -> std::vector<double> {
  if (year < 1 or year > 32766) {
    throw std::invalid_argument {
      std::format("Year {} is out of range [1, 32766].", year)
    };
  }

  const calendar::Datetime start_moment_utc { util::to_ymd(year, 1, 1), 0.0 };
  const calendar::Datetime end_moment_utc { util::to_ymd(year + 1, 1, 1), 0.0 };

  const auto start_jde = astro::julian_day::utc_to_jde(start_moment_utc);
  const auto end_jde = astro::julian_day::utc_to_jde(end_moment_utc);

  RootGenerator gen(start_jde, kind);
  std::vector<double> roots;

  while (true) {
    const auto root = gen.next();
    if (root >= end_jde) {
      break;
    }
    roots.push_back(root);
  }

  return roots;
}

} // namespace astro::moon_phase::phase_moments


namespace astro::moon_phase::illumination {

// The illuminated fraction k and the phase angle i, Meeus Chapter 48. The exact path is
// taken throughout — (48.2) elongation, (48.3) phase angle, (48.1) fraction — because the
// library already carries high-precision apparent positions; the no-positions fallback
// (48.4) would only import a coarser error story.

/**
 * @brief Compute the Moon's selenocentric phase angle i from two positions, Meeus (48.2)+(48.3).
 * @param sun_pos The Sun's position, in any spherical frame shared with `moon_pos`
 *        (in-library: apparent geocentric ecliptic).
 * @param moon_pos The Moon's position, same frame as `sun_pos`.
 * @return The phase angle i, in [0°, 180°] — 0° at full moon, 180° at new moon.
 * @note The spherical law of cosines is frame-agnostic, so equatorial (α, δ) plugs into (λ, β)
 *       as-is — the golden test feeds the book's equatorial worked values directly. Meeus
 *       prints (48.2) with the solar latitude dropped; the sin β sin β₀ term is restored here,
 *       since the library carries the Sun's apparent β.
 * @ref Jean Meeus, "Astronomical Algorithms", Second Edition, Chapter 48.
 */
[[nodiscard]] inline auto phase_angle(
  const astro::toolbox::SphericalCoordinate& sun_pos,
  const astro::toolbox::SphericalCoordinate& moon_pos
) -> astro::toolbox::AngleDeg {
  const double sin_βs = std::sin(sun_pos.β.rad());
  const double cos_βs = std::cos(sun_pos.β.rad());
  const double sin_βm = std::sin(moon_pos.β.rad());
  const double cos_βm = std::cos(moon_pos.β.rad());
  const double Δλ = (moon_pos.λ - sun_pos.λ).rad();

  // Geocentric elongation ψ, (48.2). ψ ∈ [0°, 180°], so sin ψ ≥ 0 and the root is safe.
  // The clamp closes the floating-point corner where cos_ψ lands an ulp outside [-1, 1]
  // (same guard as coord_transform.hpp's asin inputs): sqrt of a hair below zero is NaN.
  const double cos_ψ = std::clamp((sin_βs * sin_βm) + (cos_βs * cos_βm * std::cos(Δλ)), -1.0, 1.0);
  const double sin_ψ = std::sqrt(1.0 - (cos_ψ * cos_ψ));

  // (48.3): atan2 keeps i in [0°, 180°] with no quadrant bookkeeping — near conjunction
  // Δ − R·cos ψ < 0 and i lands above 90°, as a dark disk requires.
  const double R_km = sun_pos.r.km();
  const double Δ_km = moon_pos.r.km();
  const auto i_rad = astro::toolbox::AngleRad { std::atan2(R_km * sin_ψ, Δ_km - (R_km * cos_ψ)) };
  return astro::toolbox::AngleDeg { i_rad };
}

/**
 * @brief Compute the Moon's selenocentric phase angle i at a JDE, Meeus (48.2)+(48.3).
 * @param jde The Julian Ephemeris Day, on the TT scale.
 * @return The phase angle i, in [0°, 180°].
 * @details Positions: VSOP87D (Sun) and truncated ELP2000-82B (Moon), both apparent geocentric.
 * @ref Jean Meeus, "Astronomical Algorithms", Second Edition, Chapter 48.
 */
[[nodiscard]] inline auto phase_angle(const double jde) -> astro::toolbox::AngleDeg {
  return phase_angle(
    astro::sun::geocentric_coord::apparent(jde),
    astro::moon::geocentric_coord::apparent(jde)
  );
}

/**
 * @brief Compute the position angle χ of the Moon's bright limb, Meeus (48.5).
 * @param sun_eq The Sun's apparent geocentric equatorial coordinates (α₀, δ₀).
 * @param moon_eq The Moon's apparent geocentric equatorial coordinates (α, δ).
 * @return χ in [0°, 360°), measured eastward from the north point of the disk.
 * @note The formula uses equatorial coordinates; the spherical geometry is identical to (48.2),
 *       so the ecliptic-to-equatorial conversion must use the true obliquity for apparent places.
 * @ref Jean Meeus, "Astronomical Algorithms", Second Edition, Chapter 48.
 */
[[nodiscard]] inline auto position_angle(
  const astro::coords::EquatorialCoord& sun_eq, // NOLINT(bugprone-easily-swappable-parameters) -- Sun vs Moon, ordered α₀δ₀ vs αδ.
  const astro::coords::EquatorialCoord& moon_eq
) -> astro::toolbox::AngleDeg {
  const double α0 = sun_eq.α.rad();
  const double δ0 = sun_eq.δ.rad();
  const double α  = moon_eq.α.rad();
  const double δ  = moon_eq.δ.rad();
  const double Δα = α0 - α;

  // Meeus (48.5). atan2 places χ in the correct quadrant; normalize to [0°, 360°).
  const double y = std::cos(δ0) * std::sin(Δα);
  const double x = (std::sin(δ0) * std::cos(δ)) - (std::cos(δ0) * std::sin(δ) * std::cos(Δα));

  const auto χ_rad = astro::toolbox::AngleRad { std::atan2(y, x) }.normalize();
  return astro::toolbox::AngleDeg { χ_rad.deg() };
}

/**
 * @brief Compute the position angle χ of the Moon's bright limb at a JDE, Meeus (48.5).
 * @param jde The Julian Ephemeris Day, on the TT scale.
 * @return χ in [0°, 360°), measured eastward from the north point of the disk.
 * @details Sun: VSOP87D apparent equatorial; Moon: truncated ELP2000-82B apparent ecliptic,
 *          converted to equatorial with the true obliquity.
 * @ref Jean Meeus, "Astronomical Algorithms", Second Edition, Chapter 48.
 */
[[nodiscard]] inline auto position_angle(const double jde) -> astro::toolbox::AngleDeg {
  const auto sun_eq = astro::sun::equatorial_coord::apparent(jde);
  const auto moon_eq = astro::moon::equatorial_coord::apparent(jde);
  return position_angle(sun_eq, moon_eq);
}

/**
 * @brief Compute the illuminated fraction k of the Moon's disk from the phase angle, Meeus (48.1).
 * @param i The selenocentric phase angle.
 * @return k in [0, 1].
 * @ref Jean Meeus, "Astronomical Algorithms", Second Edition, Chapter 48.
 */
[[nodiscard]] inline auto fraction(const astro::toolbox::AngleDeg& i) -> double {
  return (1.0 + std::cos(i.rad())) / 2.0;
}

/**
 * @brief Compute the illuminated fraction k of the Moon's disk at a JDE, Meeus (48.1).
 * @param jde The Julian Ephemeris Day, on the TT scale.
 * @return k in [0, 1] — 0 at new moon, 1 at full moon.
 */
[[nodiscard]] inline auto fraction(const double jde) -> double {
  return fraction(phase_angle(jde));
}

} // namespace astro::moon_phase::illumination
