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
#include <format>
#include <limits>
#include <cstddef>
#include <numbers>
#include <numeric>
#include <cstdint>
#include <concepts>
#include <algorithm>
#include <stdexcept>
#include <type_traits>

namespace astro::toolbox {

#pragma region Angle Stuff

/**
 * @brief Normalize degree to [0, 360).
 * @throw std::invalid_argument If `deg` is not finite — a non-finite angle has no normal form,
 *        and returning one would pass it off as already normalized (#88).
 */
[[nodiscard]] constexpr auto normalize_deg(const double deg) -> double {
  if (not std::isfinite(deg)) [[unlikely]] {
    throw std::invalid_argument {
      std::format("Argument `deg` is not finite, got {}", deg)
    };
  }

  const double rem     = std::remainder(deg, 360.0);
  const double wrapped = rem < 0.0 ? rem + 360.0 : rem;

  // Two values escape [0, 360) without this line: `rem + 360.0` rounds up to exactly 360.0 once
  // `rem` is within ulp(360)/2 of zero, and -0.0 skips the wrap altogether (`-0.0 < 0.0` is false).
  return (wrapped >= 360.0 or wrapped == 0.0) ? 0.0 : wrapped;
}

/**
 * @brief Normalize radian to [0, 2π).
 * @throw std::invalid_argument If `rad` is not finite.
 * @note Twin of `normalize_deg` — same three edges, same order; the reasoning is written out there.
 */
[[nodiscard]] constexpr auto normalize_rad(const double rad) -> double {
  if (not std::isfinite(rad)) [[unlikely]] {
    throw std::invalid_argument {
      std::format("Argument `rad` is not finite, got {}", rad)
    };
  }

  constexpr double two_pi = 2.0 * std::numbers::pi;

  const double rem     = std::remainder(rad, two_pi);
  const double wrapped = rem < 0.0 ? rem + two_pi : rem;

  return (wrapped >= two_pi or wrapped == 0.0) ? 0.0 : wrapped;
}

/**
 * @brief Normalize degree to [-180, 180). Useful for hour angles, where sign carries meaning.
 * @throw std::invalid_argument If `deg` is not finite — inherited from `normalize_deg`.
 * @see remainder_deg for signed zero and even-quotient ties; canonical_longitude_difference for (-180°, 180°].
 */
[[nodiscard]] constexpr auto normalize_pm180(const double deg) -> double {
  const double _deg = normalize_deg(deg);
  return _deg >= 180.0 ? _deg - 360.0 : _deg;
}

/** @brief The number of degrees in a radian. */
inline constexpr double DEG_PER_RAD = 180.0 / std::numbers::pi;

/** @brief The angular period of an ecliptic longitude, in degrees. */
inline constexpr double LONGITUDE_PERIOD_DEG = 360.0;

/**
 * @brief Return the IEEE remainder in [-180°, 180°], retaining signed zero and either sign at 180°.
 * @note Half-turn ties use the even quotient. Non-finite inputs follow std::remainder's domain behavior.
 * @see normalize_pm180 for [-180°, 180°); canonical_longitude_difference for (-180°, 180°].
 */
[[nodiscard]] inline auto remainder_deg(const double difference_deg) -> double {
  return std::remainder(difference_deg, LONGITUDE_PERIOD_DEG);
}

/**
 * @brief Shift an angle by whole turns toward a reference, without normalizing either input.
 * @note Half-turn ties follow std::round (away from zero); the arithmetic does not validate finiteness.
 */
[[nodiscard]] inline auto unwrap_near(const double wrapped_deg, const double reference_deg) -> double {
  return wrapped_deg + (LONGITUDE_PERIOD_DEG * std::round((reference_deg - wrapped_deg) / LONGITUDE_PERIOD_DEG));
}

/** @brief Convert degree to radian. */
[[nodiscard]] constexpr auto deg_to_rad(const double deg) -> double {
  return deg / DEG_PER_RAD;
}

/** @brief Convert radian to degree. */
[[nodiscard]] constexpr auto rad_to_deg(const double rad) -> double {
  return rad * DEG_PER_RAD;
}

/**
 * @brief The mean angular rate of Earth's rotation relative to the stars, in degrees per day.
 *        Used to convert between time offsets and hour angle offsets.
 * @see Jean Meeus, "Astronomical Algorithms", Ch.12.
 */
inline constexpr double SIDEREAL_RATE_DEG_PER_DAY = 360.98564736629;

/**
 * @brief The Sun's mean apparent motion along the ecliptic, in degrees per day.
 *        A tropical year of 365.2422 days carries it through a full 360°.
 * @note This is a mean rate — the true rate varies by about ±1.7% over a year, since Earth's
 *       orbit is an ellipse and perihelion passage is faster than aphelion passage.
 */
inline constexpr double SOLAR_MEAN_MOTION_DEG_PER_DAY = 360.0 / 365.2422;

/**
 * @brief The mean rate at which the Moon's apparent longitude pulls away from the Sun's,
 *        in degrees per day. A synodic month of 29.530588853 days closes a full 360°.
 * @note This is a mean rate — near perigee the Moon runs at roughly 1.2× this value.
 */
inline constexpr double MOON_ELONGATION_RATE_DEG_PER_DAY = 360.0 / 29.530588853;

/** @brief The number of minutes in a degree. */
inline constexpr uint32_t MIN_PER_DEG = 60;

/** @brief The number of seconds in a minute. */
inline constexpr uint32_t SEC_PER_MIN = 60;

/** @brief The number of seconds in a degree. */
inline constexpr uint32_t SEC_PER_DEG = SEC_PER_MIN * MIN_PER_DEG;

/** @brief Convert minutes to degrees. */
[[nodiscard]] constexpr auto arcmin_to_deg(const double arcmin) -> double {
  return arcmin / MIN_PER_DEG;
}

/** @brief Convert seconds to degrees. */
[[nodiscard]] constexpr auto arcsec_to_deg(const double arcsec) -> double {
  return arcsec / SEC_PER_DEG;
}


/** @enum AngleUnit the angle's unit, either degree or radian. */
enum class AngleUnit : uint8_t { RAD, DEG };

/** 
 * @brief Represents an angle.
 * @tparam Unit The angle's unit, either degree or radian.
 */
template <AngleUnit Unit>
struct Angle {
  constexpr explicit Angle(const double value) : _value { value } {}

  /**
   * @brief Build an angle from a count of arcminutes, carried in this angle's own unit.
   * @note Arcminutes subdivide the degree, so the count lands in DEG and converts from there.
   */
  [[nodiscard]] constexpr static auto from_arcmin(const double value) -> Angle<Unit> {
    return Angle<Unit> { Angle<AngleUnit::DEG> { arcmin_to_deg(value) } };
  }

  /** @brief Build an angle from a count of arcseconds, carried in this angle's own unit. */
  [[nodiscard]] constexpr static auto from_arcsec(const double value) -> Angle<Unit> {
    return Angle<Unit> { Angle<AngleUnit::DEG> { arcsec_to_deg(value) } };
  }

  /** @brief Convert to the other unit — explicit, so a unit change never happens silently. */
  template <AngleUnit As>
  constexpr explicit operator Angle<As>() const {
    return Angle<As> { as<As>() };
  }

  [[nodiscard]] constexpr auto operator+(const Angle<Unit>& other) const -> Angle<Unit> {
    return Angle<Unit> { _value + other._value };
  }

  [[nodiscard]] constexpr auto operator-(const Angle<Unit>& other) const -> Angle<Unit> {
    return Angle<Unit> { _value - other._value };
  }

  [[nodiscard]] constexpr auto operator-() const -> Angle<Unit> {
    return Angle<Unit> { -_value };
  }

  [[nodiscard]] constexpr auto operator*(const double other) const -> Angle<Unit> {
    return Angle<Unit> { _value * other };
  }

  /**
   * @brief Divide the angle by a bare (dimensionless) factor.
   * @throws std::invalid_argument if the divisor is zero — an infinite angle is never the intent (#48).
   */
  [[nodiscard]] constexpr auto operator/(const double other) const -> Angle<Unit> {
    if (other == 0.0) {
      throw std::invalid_argument { "Division by zero." };
    }
    return Angle<Unit> { _value / other };
  }

  /**
   * @brief Convert the angle to another unit.
   * @tparam As The unit to convert to.
   * @return The converted angle.
   */
  template <AngleUnit As>
  [[nodiscard]] constexpr auto as() const -> double {
    if constexpr (Unit == As) { // No conversion needed.
      return _value;
    }

    if constexpr (As == AngleUnit::DEG) {
      return rad_to_deg(_value);
    } else {
      return deg_to_rad(_value);
    }
  }

  /**
   * @brief Normalize the angle to [0, 360) / [0, 2π), depending on the angle's unit.
   * @return The normalized angle. The returned angle is of the same unit as the original angle.
   * @throw std::invalid_argument If the angle is not finite — inherited from `normalize_deg` / `normalize_rad`.
   */
  [[nodiscard]] constexpr auto normalize() const -> Angle<Unit> {
    if constexpr (Unit == AngleUnit::DEG) {
      return Angle<Unit> { normalize_deg(_value) };
    } else {
      return Angle<Unit> { normalize_rad(_value) };
    }
  }

  /** @brief Return the angle in degrees. */
  [[nodiscard]] constexpr auto deg() const -> double {
    return as<AngleUnit::DEG>();
  }

  /** @brief Return the angle in radians. */
  [[nodiscard]] constexpr auto rad() const -> double {
    return as<AngleUnit::RAD>();
  }

 private:
  // Private, not const: `const` would also make the type unassignable, which breaks
  // `std::optional<Angle>` and every sort/erase over aggregates holding one.
  // Immutability is carried by the interface instead — every operator returns a new value.
  double _value;
};

/**
 * @brief An angle measured in degrees.
 * @note `Angle<DEG>` needs both names in scope, which outside `astro::toolbox` only a
 *       namespace-scope `using` can arrange — and headers may not have one (#51). These
 *       aliases keep the unit in the type name without asking the reader's namespace for
 *       anything: `toolbox::AngleDeg`.
 */
using AngleDeg = Angle<AngleUnit::DEG>;

/**
 * @brief Canonicalize a finite directed longitude difference to (-180, 180] degrees.
 * @throw std::invalid_argument If the angle is not finite.
 * @see normalize_pm180 for [-180°, 180°); remainder_deg for signed zero and even-quotient ties.
 */
[[nodiscard]] inline auto canonical_longitude_difference(const AngleDeg difference) -> AngleDeg {
  if (not std::isfinite(difference.deg())) {
    throw std::invalid_argument { "Longitude difference must be finite" };
  }
  const double remainder = remainder_deg(difference.deg());
  if (remainder == -LONGITUDE_PERIOD_DEG / 2.0) {
    return AngleDeg { LONGITUDE_PERIOD_DEG / 2.0 };
  }
  return AngleDeg { remainder == 0.0 ? 0.0 : remainder };
}
using AngleRad = Angle<AngleUnit::RAD>;

#pragma endregion


#pragma region Literals

namespace literals {

[[nodiscard]] constexpr auto operator""_deg(const long double value) -> AngleDeg {
  return AngleDeg { static_cast<double>(value) };
}

[[nodiscard]] constexpr auto operator""_arcmin(const long double value) -> AngleDeg {
  return AngleDeg::from_arcmin(static_cast<double>(value));
}

[[nodiscard]] constexpr auto operator""_arcsec(const long double value) -> AngleDeg {
  return AngleDeg::from_arcsec(static_cast<double>(value));
}

[[nodiscard]] constexpr auto operator""_rad(const long double value) -> AngleRad {
  return AngleRad { static_cast<double>(value) };
}

}  // namespace literals

#pragma endregion


#pragma region Coordinate Definitions

/** @brief The unit of distance, either AU or KM. */
enum class DistanceUnit : uint8_t { AU, KM };

/**
 * @brief The scaling factor from AU to KM.
 * @see NIST SP 330 (2019), Table 8: the astronomical unit is exactly 149'597'870'700 m.
 * @see ERFA v2.0.1 `erfam.h`, `ERFA_DAU`.
 */
// Retained material boundary (R34): this exact NIST/ERFA AU definition remains under its source terms
// and outside the project MIT grant; the unit wrapper is project-authored.
inline constexpr double AU_KM_SCALE = 149597870.700;

/** @brief Convert from AU to KM. */
[[nodiscard]] constexpr auto au_to_km(const double au) -> double { 
  return au * AU_KM_SCALE; 
}

/** @brief Convert from KM to AU. */
[[nodiscard]] constexpr auto km_to_au(const double km) -> double { 
  return km / AU_KM_SCALE; 
}


/** @brief Represents a distance. */
template <DistanceUnit Unit>
struct Distance {
  constexpr explicit Distance(const double value) : _value { value } {}

  /** @brief Convert to the other unit. */
  template <DistanceUnit As>
  constexpr explicit operator Distance<As>() const {
    return Distance<As> { as<As>() };
  }

  template <DistanceUnit As>
  [[nodiscard]] constexpr auto as() const -> double {
    if constexpr (Unit == As) { // No conversion needed.
      return _value;
    } else {
      if constexpr (As == DistanceUnit::AU) {
        return km_to_au(_value);
      } else {
        return au_to_km(_value);
      }
    }
  }

  [[nodiscard]] constexpr auto au() const -> double {
    return as<DistanceUnit::AU>();
  }

  [[nodiscard]] constexpr auto km() const -> double {
    return as<DistanceUnit::KM>();
  }

 private:
  double _value;
};

/** @brief A distance measured in astronomical units. */
using DistanceAu = Distance<DistanceUnit::AU>;
using DistanceKm = Distance<DistanceUnit::KM>;


/**
 * @brief Represents a position in a spherical coordinate system.
 */
struct SphericalCoordinate {
  AngleDeg   λ; // Longitude
  AngleDeg   β; // Latitude
  DistanceAu r; // Radius/Distance
};

#pragma endregion


#pragma region Root Finding

/**
 * @brief The gap between `x` and the next representable double — one unit in the last place.
 * @param x The value to measure at.
 * @return The ulp at `x`, always positive.
 * @note Doubles carry 52 fraction bits, so the ulp doubles every time the exponent does.
 *       At JDE 2451545 (J2000.0) it is 4.7e-10 day (~40 μs); once `jde` crosses
 *       2^22 = 4194304 — that is 6771-07-07 — it steps up to 9.3e-10 day.
 */
[[nodiscard]] inline auto ulp(const double x) -> double {
  const double magnitude = std::fabs(x);
  return std::nextafter(magnitude, std::numeric_limits<double>::infinity()) - magnitude;
}

/** @brief The initial half-width of the central difference approximating f', in days. */
inline constexpr double NEWTON_INITIAL_STEP_DAYS = 5e-4;

/** @brief The floor on that half-width, as a multiple of `ulp(jde)`. */
inline constexpr double NEWTON_MIN_STEP_ULP = 8.0;

/**
 * @brief The residual tolerance, as a multiple of the angle swept during one `ulp(jde)`.
 * @note About as tight as it goes — `f` bottoms out near half an ulp's worth.
 */
inline constexpr double NEWTON_RESIDUAL_TOL_ULP = 1.0;

/** @brief The Newton iteration budget. */
inline constexpr std::size_t NEWTON_MAX_ITERATIONS = 30;

/**
 * @brief Apply Newton's method to find the JDE at which `f` crosses zero.
 * @param f The function to find the root of. Must be smooth over [start_jde, end_jde) — callers
 *          that work with a wrapping angle are responsible for unwrapping it first.
 * @param start_jde The left bound of the search, inclusive.
 * @param end_jde The right bound of the search, exclusive.
 * @param mean_rate_deg_per_day The mean rate at which `f` sweeps. This sets the scale of the
 *        residual tolerance; it is not used as the derivative.
 * @param max_iterations The iteration budget.
 * @return The JDE of the root, always within [start_jde, end_jde).
 * @note It is the caller's responsibility to ensure a root exists in the range.
 */
// The `_jde` / `_deg_per_day` suffixes carry the contract at the call site.
// NOLINTBEGIN(bugprone-easily-swappable-parameters)
template <typename Func>
requires std::invocable<const Func&, double>
     and std::convertible_to<std::invoke_result_t<const Func&, double>, double>
[[nodiscard]] inline auto newton_method(
  const Func& f,
  const double start_jde,
  const double end_jde,
  const double mean_rate_deg_per_day,
  const std::size_t max_iterations = NEWTON_MAX_ITERATIONS
) -> double {
  // Keep a candidate inside [start_jde, end_jde). Backing off `end_jde` by a small constant
  // would be a no-op — at JDE 2.4e6 anything under 4.7e-10 vanishes in the rounding.
  const auto pull_back = [&](const double jde) -> double {
    if (jde < start_jde) {
      return start_jde;
    }
    if (jde >= end_jde) {
      return std::nextafter(end_jde, start_jde);
    }
    return jde;
  };

  double step = NEWTON_INITIAL_STEP_DAYS;

  // `std::midpoint` equals the rounded half-sum where a + b cannot overflow, including these JDEs.
  double jde = std::midpoint(start_jde, end_jde);

  // Newton can walk away from a root it has already reached, so keep the best iterate rather
  // than the last one — otherwise one bad late round overwrites a correct answer.
  double best_jde      = jde;
  double best_residual = std::numeric_limits<double>::infinity();

  for (std::size_t i = 0; i < max_iterations; ++i) {
    const double residual = f(jde);

    if (std::fabs(residual) < best_residual) {
      best_residual = std::fabs(residual);
      best_jde      = jde;
    }

    // Stop on the residual, not on the step size: `f` cannot be driven closer to zero than the
    // angle swept during one ulp of `jde` — 4.6e-10° for the Sun, 5.7e-9° for the Moon's
    // elongation — so a step threshold below that is unreachable by construction.
    if (std::fabs(residual) <= NEWTON_RESIDUAL_TOL_ULP * mean_rate_deg_per_day * ulp(jde)) {
      break;
    }

    // Shrink the step by the golden ratio, but keep it above ½ulp(jde): below that,
    // both samples round back to `jde` and f' collapses to zero.
    const double h = std::max(step, NEWTON_MIN_STEP_ULP * ulp(jde));
    const double f_prime = (f(jde + h) - f(jde - h)) / (2.0 * h);

    // A shared solver has to tolerate an `f` that may go non-finite somewhere in its bracket
    // and a collapsed f' has no direction left to offer.
    if (not std::isfinite(f_prime) or f_prime == 0.0) {
      break;
    }

    jde   = pull_back(jde - (residual / f_prime));
    step /= std::numbers::phi;
  }

  // The final iterate never had its residual measured — let it compete too.
  if (std::fabs(f(jde)) < best_residual) {
    return jde;
  }

  return best_jde;
}
// NOLINTEND(bugprone-easily-swappable-parameters)

#pragma endregion

#pragma region Golden Section Search

/** @brief (√5 − 1)/2, rounded directly to binary64. */
inline constexpr double INV_PHI = 0.61803398874989484820;

/** @brief (3 − √5)/2, rounded independently of INV_PHI. */
inline constexpr double INV_PHI_SQ = 0.38196601125010515180;

/** @brief Arithmetic, comparison and stopping rules supplied by an extremum-search caller. */
template <typename LowerProbe, typename MoveLower, typename Finished>
struct GoldenSectionRules {
  LowerProbe lower_probe;
  MoveLower move_lower;
  Finished finished;
};

/** @brief The final bracket; converged is false when the update budget runs out. */
struct GoldenSectionBracket {
  double lower;
  double upper;
  bool converged;
};

/**
 * @brief Contract a unimodal bracket using the caller's numerical rules.
 * @param f The objective, evaluated at two initial probes even when maximum_updates is zero.
 * @param lower The initial lower bound.
 * @param upper The initial upper bound.
 * @param tolerance The width threshold passed to rules.finished.
 * @param maximum_updates The contraction budget, excluding the two initial probes.
 * @param rules The lower-probe arithmetic, move-lower comparison and width stopping predicate.
 * @return The final bounds and whether rules.finished accepted them.
 * @note The stopping predicate is checked before each update, never after the final allowed update.
 *       Midpoint selection, witnesses and the meaning of budget exhaustion belong to the caller.
 */
// NOLINTBEGIN(bugprone-easily-swappable-parameters) -- Bounds and tolerance have distinct named roles.
template <typename Func, typename Rules>
requires std::invocable<const Func&, double>
     and std::convertible_to<std::invoke_result_t<const Func&, double>, double>
[[nodiscard]] inline auto golden_section_bracket(
  const Func& f,
  const double lower,
  const double upper,
  const double tolerance,
  const std::size_t maximum_updates,
  const Rules& rules
) -> GoldenSectionBracket {
  double lo = lower;
  double hi = upper;
  double lhs = rules.lower_probe(lo, hi);
  double rhs = lo + (INV_PHI * (hi - lo));
  double lhs_value = f(lhs);
  double rhs_value = f(rhs);

  for (std::size_t iteration = 0; iteration < maximum_updates; ++iteration) {
    if (rules.finished(hi - lo, tolerance)) {
      return { .lower = lo, .upper = hi, .converged = true };
    }

    if (rules.move_lower(lhs_value, rhs_value)) {
      lo = lhs;
      lhs = rhs;
      lhs_value = rhs_value;
      rhs = lo + (INV_PHI * (hi - lo));
      rhs_value = f(rhs);
    } else {
      hi = rhs;
      rhs = lhs;
      rhs_value = lhs_value;
      lhs = rules.lower_probe(lo, hi);
      lhs_value = f(lhs);
    }
  }
  return { .lower = lo, .upper = hi, .converged = false };
}
// NOLINTEND(bugprone-easily-swappable-parameters)

#pragma endregion

} // namespace astro::toolbox
