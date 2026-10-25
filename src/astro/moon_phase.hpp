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
#include <vector>
#include <format>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>

#include "toolbox.hpp"
#include "julian_day.hpp"
#include "coord_transform.hpp"
#include "illumination.hpp"

#include "sun.hpp"
#include "moon.hpp"


namespace astro::moon_phase {

/**
 * @brief Calculate the difference between the apparent longitudes of the Moon and the Sun.
 * @param jde The Julian Ephemeris Day on the TT scale.
 * @return Moon minus Sun apparent longitude, in [0°, 360°).
 * @see VSOP87D, ELP2000-82B, and Astronomical Algorithms, Jean Meeus, 1998.
 */
[[nodiscard]] inline auto longitude_diff(const double jde) -> double {
  const auto sun_apparent_lon = astro::sun::geocentric_coord::apparent(jde).λ;
  const auto moon_apparent_lon = astro::moon::geocentric_coord::apparent(jde).λ;
  const auto diff = moon_apparent_lon - sun_apparent_lon;
  return diff.normalize().deg();
}

} // namespace astro::moon_phase


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

} // namespace astro::moon_phase::phase_moments


namespace astro::moon_phase::detail {

/** @brief The existing conjunction and principal-phase numerical conventions. */
enum class NumericPolicy : uint8_t { CONJUNCTION, PHASE };

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
 * @note At a 19% rate offset, correcting a 30° miss leaves about `30 / 12.19 * 0.19` = 0.47 day.
 *       The bracket must also keep endpoints within 15° even at 14.5°/day.
 */
inline constexpr double BRACKET_HALF_WIDTH_DAYS = 0.5;

/**
 * @brief Map elongation to the Newton objective; conjunction uses its local near-360° unwrap.
 * @note Subtracting 360° only above 345° keeps the conjunction branch continuous near the root.
 *       It is not the principal phase's global [-180°, 180°) fold.
 * @pre diff is in [0°, 360°); target_deg is 0° for conjunction, a principal elongation otherwise.
 */
template <NumericPolicy POLICY>
[[nodiscard]] inline auto objective_at(const double diff, const double target_deg) -> double {
  if constexpr (POLICY == NumericPolicy::CONJUNCTION) {
    return diff > 360.0 - BRACKET_TOLERANCE_DEG ? diff - 360.0 : diff;
  } else {
    return toolbox::normalize_pm180(diff - target_deg);
  }
}

/** @brief Signed mean-rate miss; conjunction keeps +180°, principal phases fold it to -180°. */
template <NumericPolicy POLICY>
[[nodiscard]] inline auto miss_at(const double diff, const double target_deg) -> double {
  if constexpr (POLICY == NumericPolicy::CONJUNCTION) {
    return diff > 180.0 ? diff - 360.0 : diff;
  } else {
    return toolbox::normalize_pm180(diff - target_deg);
  }
}

// NOLINTBEGIN(bugprone-easily-swappable-parameters) -- JDE bounds and elongations are named at each call.

/** @brief Reject endpoints outside the policy's local branch; only conjunction accepts right +0. */
template <NumericPolicy POLICY>
[[nodiscard]] inline auto bracket_rejected(
  const double left_diff,
  const double right_diff,
  const double target_deg
) -> bool {
  if constexpr (POLICY == NumericPolicy::CONJUNCTION) {
    return left_diff <= 360.0 - BRACKET_TOLERANCE_DEG or right_diff >= BRACKET_TOLERANCE_DEG;
  } else {
    const double left = toolbox::normalize_pm180(left_diff - target_deg);
    const double right = toolbox::normalize_pm180(right_diff - target_deg);
    const bool misses_target = left >= 0.0 or right <= 0.0;
    const bool endpoints_too_far = std::fabs(left) >= BRACKET_TOLERANCE_DEG
                               or std::fabs(right) >= BRACKET_TOLERANCE_DEG;
    return misses_target or endpoints_too_far;
  }
}

/** @brief Report rejected endpoints using the policy's raw or folded elongations. */
template <NumericPolicy POLICY>
[[noreturn]] inline auto reject_bracket(
  const double left_jde,
  const double left_diff,
  const double right_jde,
  const double right_diff,
  const double target_deg
) -> void {
  if constexpr (POLICY == NumericPolicy::CONJUNCTION) {
    throw std::invalid_argument {
      std::format(
        "No root between jde {} (elongation {} deg) and jde {} (elongation {} deg).",
        left_jde, left_diff, right_jde, right_diff
      )
    };
  } else {
    throw std::invalid_argument {
      std::format(
        "No root between jde {} (elongation {} deg) and jde {} (elongation {} deg) for target {} deg.",
        left_jde,
        objective_at<POLICY>(left_diff, target_deg),
        right_jde,
        objective_at<POLICY>(right_diff, target_deg),
        target_deg
      )
    };
  }
}

/** @brief Report an estimate outside the refinement allowance without reevaluating positions. */
template <NumericPolicy POLICY>
[[noreturn]] inline auto reject_estimate(
  const double jde,
  const double est_jde,
  const double miss_deg,
  const double elongation_deg,
  const double target_deg
) -> void {
  if constexpr (POLICY == NumericPolicy::CONJUNCTION) {
    throw std::invalid_argument {
      std::format(
        "Cannot find the first root after jde {}: the estimate at jde {} misses conjunction "
        "by {} deg (elongation {} deg).",
        jde, est_jde, miss_deg, elongation_deg
      )
    };
  } else {
    throw std::invalid_argument {
      std::format(
        "Cannot find the first root after jde {} for target {} deg: the estimate at jde {} "
        "misses the target by {} deg (elongation {} deg).",
        jde, target_deg, est_jde, miss_deg, elongation_deg
      )
    };
  }
}

/** @brief Report a seed more than one degree from the requested root. */
template <NumericPolicy POLICY>
[[noreturn]] inline auto reject_seed(const double jde, const double target_deg) -> void {
  if constexpr (POLICY == NumericPolicy::CONJUNCTION) {
    throw std::invalid_argument { std::format("The jde {} is not a root.", jde) };
  } else {
    throw std::invalid_argument { std::format("The jde {} is not a root for target {} deg.", jde, target_deg) };
  }
}


/**
 * @brief Refine a root in [left_jde, right_jde), on the TT scale.
 * @pre The endpoints bracket a root on the policy's continuous branch; conjunction has target 0°.
 * @param iterations The Newton iteration cap; zero retains the bracket midpoint.
 * @throw std::invalid_argument If the endpoints fail the policy's bracket check.
 */
template <NumericPolicy POLICY>
[[nodiscard]] inline auto newton_method(
  const double left_jde,
  const double right_jde,
  const double target_deg,
  const std::size_t iterations = toolbox::NEWTON_MAX_ITERATIONS
) -> double {
  const double left_diff = longitude_diff(left_jde);
  const double right_diff = longitude_diff(right_jde);
  if (bracket_rejected<POLICY>(left_diff, right_diff, target_deg)) [[unlikely]] {
    reject_bracket<POLICY>(left_jde, left_diff, right_jde, right_diff, target_deg);
  }

  const auto f = [target_deg](const double jde) -> double {
    return objective_at<POLICY>(longitude_diff(jde), target_deg);
  };

  return toolbox::newton_method(
    f,
    left_jde,
    right_jde,
    toolbox::MOON_ELONGATION_RATE_DEG_PER_DAY,
    iterations
  );
}

/**
 * @brief Approximate the range of the first root after the given JDE for a target elongation.
 * @param jde The JDE on the TT scale.
 * @param target_deg Zero for conjunction, a principal elongation otherwise.
 * @return The bracket [left, right) containing the first root after `jde`.
 * @throw std::invalid_argument If the mean-rate estimate lands too far from the target.
 */
template <NumericPolicy POLICY>
[[nodiscard]] inline auto first_root_range_after(const double jde, const double target_deg) -> std::pair<double, double> {
  const double cur_diff = longitude_diff(jde);

  const double gap = (target_deg > cur_diff)
                   ? (target_deg - cur_diff)
                   : (360.0 - cur_diff + target_deg);

  constexpr double deg_per_day = toolbox::MOON_ELONGATION_RATE_DEG_PER_DAY;
  const double est_jde = jde + (gap / deg_per_day);

  const double est_jde_diff = longitude_diff(est_jde);
  const double signed_miss_deg = miss_at<POLICY>(est_jde_diff, target_deg);
  if (std::fabs(signed_miss_deg) > ESTIMATE_TOLERANCE_DEG) [[unlikely]] {
    reject_estimate<POLICY>(jde, est_jde, signed_miss_deg, est_jde_diff, target_deg);
  }

  // Refine before bracketing so the half-width need not absorb the whole mean-rate miss.
  const double root_est = est_jde - (signed_miss_deg / deg_per_day);
  return { root_est - BRACKET_HALF_WIDTH_DAYS, root_est + BRACKET_HALF_WIDTH_DAYS };
}

/**
 * @brief Find the next root after a known root for the same target elongation.
 * @param jde The known root JDE.
 * @param target_deg The target elongation, in degrees.
 * @return The next root JDE.
 * @throw std::invalid_argument If `jde` is not a root for the target.
 */
template <NumericPolicy POLICY>
[[nodiscard]] inline auto next_root(const double jde, const double target_deg) -> double {
  if (std::fabs(toolbox::normalize_pm180(longitude_diff(jde) - target_deg)) > 1.0) [[unlikely]] {
    reject_seed<POLICY>(jde, target_deg);
  }

  // A one-day step prevents a just-before-root seed from finding the same root again.
  const auto [left, right] = first_root_range_after<POLICY>(jde + 1.0, target_deg);
  return newton_method<POLICY>(left, right, target_deg);
}

/** @brief Estimate, bracket and refine the first root after a JDE(TT). */
template <NumericPolicy POLICY>
[[nodiscard]] inline auto first_root_after(const double jde, const double target_deg) -> double {
  const auto [left, right] = first_root_range_after<POLICY>(jde, target_deg);
  return newton_method<POLICY>(left, right, target_deg);
}

// NOLINTEND(bugprone-easily-swappable-parameters)


/**
 * @brief Eager-by-one Moon-phase roots in JDE(TT); next refines the following root before returning.
 */
// TODO: Use `std::generator` once every CI leg has it (./checks.py --features).
template <NumericPolicy POLICY>
struct RootGenerator {
private:
  double _target;
  double _root;

public:
  explicit RootGenerator(const double start_jde) requires (POLICY == NumericPolicy::CONJUNCTION)
    : _target { 0.0 }, _root { first_root_after<POLICY>(start_jde, _target) } {}

  explicit RootGenerator(
    const double start_jde,
    const phase_moments::PhaseKind kind
  ) requires (POLICY == NumericPolicy::PHASE)
    : _target { phase_moments::target_angle_deg(kind) }, _root { first_root_after<POLICY>(start_jde, _target) } {}

  /** @brief Inspect the pending root without computing another one. */
  [[nodiscard]] auto peek() const -> double { return _root; }

  /** @brief Return the pending root after successfully computing its successor. */
  [[nodiscard]] auto next() -> double {
    const double root = _root;
    _root = next_root<POLICY>(_root, _target);
    return root;
  }
};

/** @brief Collect roots strictly before end_jde, retaining the generator's eager advance. */
template <NumericPolicy POLICY>
[[nodiscard]] inline auto roots_before(RootGenerator<POLICY> gen, const double end_jde) -> std::vector<double> {
  std::vector<double> roots;
  while (true) {
    const double root = gen.next();
    if (root >= end_jde) {
      return roots;
    }
    roots.push_back(root);
  }
}

} // namespace astro::moon_phase::detail


namespace astro::moon_phase::new_moon {

/**
 * @brief Conjunction (朔) roots: equal Moon-Sun apparent longitude.
 * @note The numerical convention differs from phase_moments::NEW_MOON at bracket +0 and the 180°/345° folds.
 */
using RootGenerator = detail::RootGenerator<detail::NumericPolicy::CONJUNCTION>;

/**
 * @brief Calculate conjunctions of the Sun and Moon in a Gregorian year.
 * @param year The Gregorian year, in [1, 32766].
 * @return Conjunction moments in JDE(TT), using VSOP87D and truncated ELP2000-82B positions.
 * @throw std::invalid_argument If year is outside [1, 32766].
 * @note Year boundaries are UTC, degrading to UT1 before 1972 (#84).
 * @note A conjunction exactly at Jan 1 UTC has no defined owner; rounding may retain or skip it (#127).
 */
[[nodiscard]] inline auto moments(const int32_t year) -> std::vector<double> {
  const auto [start_jde, end_jde] = astro::julian_day::utc_year_jde_window(year);
  return detail::roots_before(RootGenerator { start_jde }, end_jde);
}

} // namespace astro::moon_phase::new_moon


namespace astro::moon_phase::phase_moments {

/** @brief A generator for one principal phase, using its signed-fold numerical convention. */
using RootGenerator = detail::RootGenerator<detail::NumericPolicy::PHASE>;

/**
 * @brief Calculate the moments of a given Moon phase in a Gregorian year.
 * @param year The Gregorian year, in [1, 32766].
 * @param kind The phase kind.
 * @return The phase moments in the year, in JDE (TT-scale ephemeris time).
 * @throw std::invalid_argument if `year` is outside [1, 32766].
 * @note The year runs from Jan 1 to Jan 1 in UTC; before 1972 the bounds degrade to UT1.
 * @note A phase exactly at Jan 1 UTC has no defined owner; rounding may retain or skip it (#127).
 * @details Positions: VSOP87D (Sun) and truncated ELP2000-82B (Moon), both apparent geocentric.
 */
[[nodiscard]] inline auto moments(const int32_t year, const PhaseKind kind) -> std::vector<double> {
  const auto [start_jde, end_jde] = astro::julian_day::utc_year_jde_window(year);
  return detail::roots_before(RootGenerator { start_jde, kind }, end_jde);
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
 *       as-is — the golden test feeds the book's equatorial worked values directly.
 * @see Jean Meeus, "Astronomical Algorithms", Second Edition, Chapter 48.
 */
[[nodiscard]] inline auto phase_angle(
  const toolbox::SphericalCoordinate& sun_pos,
  const toolbox::SphericalCoordinate& moon_pos
) -> toolbox::AngleDeg {
  return astro::illumination::geometry(sun_pos, moon_pos).phase_angle;
}

/**
 * @brief Compute the Moon's selenocentric phase angle i at a JDE, Meeus (48.2)+(48.3).
 * @param jde The Julian Ephemeris Day, on the TT scale.
 * @return The phase angle i, in [0°, 180°].
 * @details Positions: VSOP87D (Sun) and truncated ELP2000-82B (Moon), both apparent geocentric.
 * @see Jean Meeus, "Astronomical Algorithms", Second Edition, Chapter 48.
 */
[[nodiscard]] inline auto phase_angle(const double jde) -> toolbox::AngleDeg {
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
 * @see Jean Meeus, "Astronomical Algorithms", Second Edition, Chapter 48.
 */
[[nodiscard]] inline auto position_angle(
  const astro::coords::EquatorialCoord& sun_eq, // NOLINT(bugprone-easily-swappable-parameters) -- Sun vs Moon, ordered α₀δ₀ vs αδ.
  const astro::coords::EquatorialCoord& moon_eq
) -> toolbox::AngleDeg {
  const double α0 = sun_eq.α.rad();
  const double δ0 = sun_eq.δ.rad();
  const double α  = moon_eq.α.rad();
  const double δ  = moon_eq.δ.rad();
  const double Δα = α0 - α;

  // Meeus (48.5). atan2 places χ in the correct quadrant; normalize to [0°, 360°).
  const double y = std::cos(δ0) * std::sin(Δα);
  const double x = (std::sin(δ0) * std::cos(δ)) - (std::cos(δ0) * std::sin(δ) * std::cos(Δα));

  const auto χ_rad = toolbox::AngleRad { std::atan2(y, x) }.normalize();
  return toolbox::AngleDeg { χ_rad.deg() };
}

/**
 * @brief Compute the position angle χ of the Moon's bright limb at a JDE, Meeus (48.5).
 * @param jde The Julian Ephemeris Day, on the TT scale.
 * @return χ in [0°, 360°), measured eastward from the north point of the disk.
 * @details Sun: VSOP87D apparent equatorial; Moon: truncated ELP2000-82B apparent ecliptic,
 *          converted to equatorial with the true obliquity.
 * @see Jean Meeus, "Astronomical Algorithms", Second Edition, Chapter 48.
 */
[[nodiscard]] inline auto position_angle(const double jde) -> toolbox::AngleDeg {
  const auto sun_eq = astro::sun::equatorial_coord::apparent(jde);
  const auto moon_eq = astro::moon::equatorial_coord::apparent(jde);
  return position_angle(sun_eq, moon_eq);
}

/**
 * @brief Compute the illuminated fraction k of the Moon's disk from the phase angle, Meeus (48.1).
 * @param i The selenocentric phase angle.
 * @return k in [0, 1].
 * @see Jean Meeus, "Astronomical Algorithms", Second Edition, Chapter 48.
 */
[[nodiscard]] inline auto fraction(const toolbox::AngleDeg& i) -> double {
  return astro::illumination::fraction(i);
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
