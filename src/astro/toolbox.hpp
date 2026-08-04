/*
 * CelestialCalendar: 
 *   A C++23-style library that performs astronomical calculations and date conversions among various calendars,
 *   including Gregorian, Lunar, and Chinese Ganzhi calendars.
 * 
 * Copyright (C) 2024 Ningqi Wang (0xf3cd)
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
#include <format>
#include <limits>
#include <cstddef>
#include <numbers>
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
constexpr auto normalize_deg(const double deg) -> double {
  if (not std::isfinite(deg)) [[unlikely]] {
    throw std::invalid_argument {
      std::format("Argument `deg` is not finite, whose value is {}", deg)
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
constexpr auto normalize_rad(const double rad) -> double {
  if (not std::isfinite(rad)) [[unlikely]] {
    throw std::invalid_argument {
      std::format("Argument `rad` is not finite, whose value is {}", rad)
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
 */
constexpr auto normalize_pm180(const double deg) -> double {
  const double _deg = normalize_deg(deg);
  return _deg >= 180.0 ? _deg - 360.0 : _deg;
}

/** @brief The number of degrees in a radian. */
inline constexpr double DEG_PER_RAD = 180.0 / std::numbers::pi;

/** @brief Convert degree to radian. */
constexpr auto deg_to_rad(const double deg) -> double {
  return deg / DEG_PER_RAD;
}

/** @brief Convert radian to degree. */
constexpr auto rad_to_deg(const double rad) -> double {
  return rad * DEG_PER_RAD;
}

/**
 * @brief The mean angular rate of Earth's rotation relative to the stars, in degrees per day.
 *        Used to convert between time offsets and hour angle offsets.
 * @ref Jean Meeus, "Astronomical Algorithms", Ch.12.
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
constexpr auto arcmin_to_deg(const double arcmin) -> double {
  return arcmin / MIN_PER_DEG;
}

/** @brief Convert seconds to degrees. */
constexpr auto arcsec_to_deg(const double arcsec) -> double {
  return arcsec / SEC_PER_DEG;
}


/** @enum AngleUnit the angle's unit, either degree or radian. */
enum class AngleUnit : uint8_t { RAD, DEG };

/** 
 * @struct Represents an angle. 
 * @tparam Unit The angle's unit, either degree or radian.
 */
template <AngleUnit Unit>
struct Angle {
  constexpr explicit Angle(const double value) : _value { value } {}

  /**
   * @brief Build an angle from a count of arcminutes, carried in this angle's own unit.
   * @note Arcminutes subdivide the degree, so the count lands in DEG and converts from there.
   */
  constexpr static auto from_arcmin(const double value) -> Angle<Unit> {
    return Angle<Unit> { Angle<AngleUnit::DEG> { arcmin_to_deg(value) } };
  }

  /** @brief Build an angle from a count of arcseconds, carried in this angle's own unit. */
  constexpr static auto from_arcsec(const double value) -> Angle<Unit> {
    return Angle<Unit> { Angle<AngleUnit::DEG> { arcsec_to_deg(value) } };
  }

  /** @brief Convert to the other unit — explicit, so a unit change never happens silently. */
  template <AngleUnit As>
  constexpr explicit operator Angle<As>() const {
    return Angle<As> { as<As>() };
  }

  constexpr auto operator+(const Angle<Unit>& other) const -> Angle<Unit> {
    return Angle<Unit> { _value + other._value };
  }

  constexpr auto operator-(const Angle<Unit>& other) const -> Angle<Unit> {
    return Angle<Unit> { _value - other._value };
  }

  constexpr auto operator-() const -> Angle<Unit> {
    return Angle<Unit> { -_value };
  }

  constexpr auto operator*(const double other) const -> Angle<Unit> {
    return Angle<Unit> { _value * other };
  }

  /**
   * @brief Divide the angle by a bare (dimensionless) factor.
   * @throws std::runtime_error if the divisor is zero — an infinite angle is never the intent (#48).
   */
  constexpr auto operator/(const double other) const -> Angle<Unit> {
    if (other == 0.0) {
      throw std::runtime_error { "Division by zero." };
    }
    return Angle<Unit> { _value / other };
  }

  /**
   * @brief Convert the angle to another unit.
   * @param As The unit to convert to.
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
 * @brief The spelling used everywhere outside this header (#53).
 * @note `Angle<DEG>` needs both names in scope, which outside `astro::toolbox` only a
 *       namespace-scope `using` can arrange — and headers may not have one (#51). These
 *       aliases keep the unit in the type name without asking the reader's namespace for
 *       anything: `toolbox::AngleDeg`.
 */
using AngleDeg = Angle<AngleUnit::DEG>;
using AngleRad = Angle<AngleUnit::RAD>;

#pragma endregion


#pragma region Literals

namespace literals {

constexpr auto operator""_deg(const long double value) -> AngleDeg {
  return AngleDeg { static_cast<double>(value) };
}

constexpr auto operator""_arcmin(const long double value) -> AngleDeg {
  return AngleDeg::from_arcmin(static_cast<double>(value));
}

constexpr auto operator""_arcsec(const long double value) -> AngleDeg {
  return AngleDeg::from_arcsec(static_cast<double>(value));
}

constexpr auto operator""_rad(const long double value) -> AngleRad {
  return AngleRad { static_cast<double>(value) };
}

}  // namespace literals

#pragma endregion


#pragma region Coordinate Definitions

/** @enum The unit of distance, either AU or KM. */
enum class DistanceUnit : uint8_t { AU, KM };

/** @brief The scaling factor from AU to KM. */
inline constexpr double au_km_scale = 149597870.691; 

/** @brief Convert from AU to KM. */
constexpr auto au_to_km(const double au) -> double { 
  return au * au_km_scale; 
}

/** @brief Convert from KM to AU. */
constexpr auto km_to_au(const double km) -> double { 
  return km / au_km_scale; 
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

/** @brief The spelling used outside this header, for the same reason as `AngleDeg` (#53). */
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
inline auto ulp(const double x) -> double {
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

/** @brief The iteration budget. Convergence is measured at 5 iterations or fewer. */
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
inline auto newton_method(
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
  double jde  = (start_jde + end_jde) / 2.0;

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

    // Shrinking the step by the golden ratio is what keeps the run short: at most 5 iterations
    // over years 401-9050, against 7 for a fixed step. The floor is what keeps it honest — below
    // ½ulp(jde) both samples round back to `jde` and f' collapses to exactly zero.
    const double h = std::max(step, NEWTON_MIN_STEP_ULP * ulp(jde));
    const double f_prime = (f(jde + h) - f(jde - h)) / (2.0 * h);

    // A shared solver has to tolerate an `f` that may go non-finite somewhere in its bracket
    // (#43 originally planned such an f), and a collapsed f' has no direction left to offer.
    if (not std::isfinite(f_prime) or f_prime == 0.0) {
      break;
    }

    jde   = pull_back(jde - residual / f_prime);
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

} // namespace astro::toolbox
