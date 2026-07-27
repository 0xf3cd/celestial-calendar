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

#include <chrono>
#include <cmath>
#include <format>
#include <optional>
#include <stdexcept>

#include "toolbox.hpp"
#include "datetime.hpp"
#include "julian_day.hpp"
#include "coord_transform.hpp"
#include "sidereal_time.hpp"
#include "sun.hpp"


namespace astro::sunrise_sunset {

#pragma region Constants

/**
 * @brief The standard altitude of the Sun's center at rise/set: -0°50' = -0.8333…°.
 * @note Meeus Chapter 15: -34' of standard atmospheric refraction at the horizon,
 *       plus -16' so that the event refers to the Sun's upper limb, not its center.
 */
inline constexpr auto STANDARD_ALTITUDE = astro::toolbox::Angle<astro::toolbox::AngleUnit::DEG>::from_arcmin(-50.0);

/** @brief The Sun's altitude at civil twilight: -6°. */
inline constexpr astro::toolbox::Angle<astro::toolbox::AngleUnit::DEG> CIVIL_TWILIGHT { -6.0 };

/** @brief The Sun's altitude at nautical twilight: -12°. */
inline constexpr astro::toolbox::Angle<astro::toolbox::AngleUnit::DEG> NAUTICAL_TWILIGHT { -12.0 };

/** @brief The Sun's altitude at astronomical twilight: -18°. */
inline constexpr astro::toolbox::Angle<astro::toolbox::AngleUnit::DEG> ASTRONOMICAL_TWILIGHT { -18.0 };

/**
 * @brief How far past ±1 cos(H₀) may land and still count as a grazing event rather than
 *        a polar day/night verdict.
 * @note Roundoff in `(sin h₀ - sin φ sin δ) / (cos φ cos δ)` is a few ulps (~1e-16); 1e-9 keeps
 *       a wide margin over that while staying far below any physically meaningful excess.
 */
inline constexpr double COS_H0_CLAMP_TOLERANCE = 1e-9;

/**
 * @brief Below this |cos φ · cos δ| the hour-angle equation is treated as degenerate (observer at
 *        a geographic pole, or the body at a celestial pole).
 */
inline constexpr double POLAR_DENOMINATOR_EPSILON = 1e-10;

/**
 * @brief The half-width of the root bracket around the transit estimate, in days.
 * @note The estimate is local mean noon; the true transit deviates from it by the equation of
 *       time only, |EoT| ≤ 16.5 min ≈ 0.0115 day — a 8.7x margin. The hour angle sweeps
 *       ±36° across this bracket, well clear of the ±180° wrap.
 */
inline constexpr double TRANSIT_BRACKET_HALF_WIDTH_DAYS = 0.1;

/**
 * @brief The half-width of the first-try root bracket around the rise/set estimate, in days.
 * @note The estimate extrapolates H₀ computed from the declination at transit; away from the
 *       polar boundary the δ drift between transit and the event (≤ ~0.2°) moves the true root
 *       by a few minutes at most, so ±72 min is a comfortable margin. Near the polar boundary
 *       the extrapolation degrades without bound — which is why this bracket is only an
 *       accelerator: the decider is the sign check at the bracket ends, with the
 *       transit-to-lower-culmination bracket as fallback (see `rise_set_jde`).
 */
inline constexpr double RISE_SET_BRACKET_HALF_WIDTH_DAYS = 0.05;

/**
 * @brief The nominal half solar day, in days — the mean-time estimate of how far the lower
 *        culmination sits from the transit. The apparent value deviates from 0.5 by up to
 *        ~±15 s (the apparent solar day runs 24h ± ~30 s), which is why the fallback bracket
 *        ends at the *solved* lower culmination, not at transit ± 0.5 (per review: a short
 *        polar-boundary night centered on the lower culmination can lie entirely outside
 *        an exact half-day window).
 */
inline constexpr double HALF_SOLAR_DAY_DAYS = 0.5;

/**
 * @brief The half-width of the root bracket around the lower-culmination estimate, in days.
 * @note The estimate is transit ± `HALF_SOLAR_DAY_DAYS`; the true lower culmination deviates
 *       by ≤ ~15 s ≈ 1.8e-4 day — a 500x margin. The unwrapped H−180° sweeps ±36° across
 *       this bracket, well clear of its wrap (which sits at the upper culminations).
 */
inline constexpr double LOWER_CULMINATION_BRACKET_HALF_WIDTH_DAYS = 0.1;

/**
 * @brief The residual guard on a rise/set root, in degrees of altitude.
 * @note Robustness only: after a directed sign check the bracketed Newton solve converges to
 *       residuals ~1e-7° for the Sun; this guard (4 orders looser) exists so that a degenerate
 *       solve — `newton_method` may return a best-effort iterate when f′ collapses — surfaces
 *       as "no event" instead of a wrong instant, and to keep the contract honest if this
 *       solver is ever reused for the Moon.
 */
inline constexpr double RISE_SET_RESIDUAL_GUARD_DEG = 1e-3;

#pragma endregion


#pragma region Types

/**
 * @brief An observer's location on the Earth.
 * @note `longitude` is **positive east** of Greenwich (the modern/ISO 6709 convention),
 *       in [-180°, 180°]. This is the opposite of Meeus's west-positive convention used by
 *       `sidereal::local_apparent`; the negation happens inside this namespace, so callers
 *       never deal with west-positive longitudes.
 * @note +180° and -180° name the same meridian but are NOT interchangeable here: the date
 *       input is a UT1 date, so they select transit-centered windows one day apart (local
 *       mean noon at +180° is 0h UT of that date; at -180° it is 24h UT). Pick the sign
 *       matching the intended UT window — deliberate, same behavior as other UT-date APIs.
 */
struct GeoLocation {
  astro::toolbox::Angle<astro::toolbox::AngleUnit::DEG> latitude;  // North-positive, [-90°, 90°].
  astro::toolbox::Angle<astro::toolbox::AngleUnit::DEG> longitude; // East-positive, [-180°, 180°].
};

/**
 * @brief The result of a rise/transit/set calculation for one date.
 * @note All instants are JDE, on the **TT** scale, like every other moment produced by this
 *       library. Use `julian_day::jde_to_ut1` to read them as civil (UT1) datetimes.
 * @note `is_polar_day`/`is_polar_night` are set only when neither rise nor set exists in the
 *       transit-centered window; on the transition days around the polar seasons one of the two
 *       events can exist alone, in which case both flags stay false.
 */
struct Result {
  std::optional<double> sunrise_jde;  // The sunrise instant, or nullopt if the Sun never crosses h₀ upward.
  std::optional<double> sunset_jde;   // The sunset instant, or nullopt if the Sun never crosses h₀ downward.
  double transit_jde;                 // The upper-culmination instant. Exists even on polar days/nights.
  bool is_polar_day;                  // The Sun stays above h₀ all day.
  bool is_polar_night;                // The Sun stays below h₀ all day.
};

#pragma endregion


namespace detail {

/**
 * @brief Validate an observer location, throwing on out-of-range or non-finite coordinates.
 * @param location The location to validate.
 * @throw std::invalid_argument If latitude ∉ [-90°, 90°] or longitude ∉ [-180°, 180°],
 *        or either is not finite.
 */
inline void validate(const GeoLocation& location) {
  const double lat = location.latitude.deg();
  const double lon = location.longitude.deg();

  if (not std::isfinite(lat) or lat < -90.0 or lat > 90.0) {
    throw std::invalid_argument {
      std::format("Argument `location.latitude` out of range [-90, 90], whose value is {}", lat)
    };
  }

  if (not std::isfinite(lon) or lon < -180.0 or lon > 180.0) {
    throw std::invalid_argument {
      std::format("Argument `location.longitude` out of range [-180, 180], whose value is {}", lon)
    };
  }
}

/**
 * @brief Convert a JDE (TT) to the JD (UT1) of the same instant.
 * @param jde_tt The julian ephemeris day, on the **TT** scale.
 * @return The julian day number, on the **UT1** scale.
 */
[[nodiscard]] inline auto jde_tt_to_jd_ut1(const double jde_tt) -> double {
  return astro::julian_day::ut1_to_jd(astro::julian_day::jde_to_ut1(jde_tt));
}

/** @brief The Sun's local hour angle and equatorial position at one instant. */
struct SunLocal {
  double hour_angle_deg;                // The local hour angle H = θ(LAST) - α, unwrapped to [-180°, 180°).
  astro::coords::EquatorialCoord eq;    // The Sun's apparent equatorial coordinates (α, δ).
};

/**
 * @brief Compute the Sun's local hour angle and equatorial coordinates for an observer.
 * @param jde_tt The instant, as a julian ephemeris day on the **TT** scale.
 * @param location The observer's location.
 * @return The hour angle (unwrapped to [-180°, 180°)) and the equatorial coordinates.
 * @note The sidereal time is evaluated on the UT1 scale (pitfall: feeding TT would shift the
 *       result by ΔT ≈ 69 s); the Sun's position and the nutation terms are evaluated on TT.
 */
[[nodiscard]] inline auto sun_local(const double jde_tt, const GeoLocation& location) -> SunLocal {
  const double jd_ut1 = jde_tt_to_jd_ut1(jde_tt);
  const auto eq = astro::sun::equatorial_coord::apparent(jde_tt);

  // `local_apparent` wants Meeus's west-positive longitude; `GeoLocation` carries east-positive.
  const auto θ = astro::sidereal::local_apparent(jd_ut1, jde_tt, -location.longitude);

  return {
    .hour_angle_deg = astro::toolbox::normalize_pm180((θ - eq.α).deg()),
    .eq = eq,
  };
}

/**
 * @brief Compute the Sun's geometric altitude for an observer at one instant.
 * @param jde_tt The instant, as a julian ephemeris day on the **TT** scale.
 * @param location The observer's location.
 * @return The Sun's altitude, in [-90°, 90°]. Purely geometric — refraction enters only through
 *         the h₀ convention (e.g. `STANDARD_ALTITUDE`), not through this function.
 */
[[nodiscard]] inline auto sun_altitude(
  const double jde_tt,
  const GeoLocation& location
) -> astro::toolbox::Angle<astro::toolbox::AngleUnit::DEG> {
  using astro::toolbox::Angle;
  using astro::toolbox::AngleUnit::DEG;

  const auto local = sun_local(jde_tt, location);
  const auto horizontal = astro::coords::equatorial_to_horizontal(
    Angle<DEG> { local.hour_angle_deg }, local.eq.δ, location.latitude
  );
  return horizontal.h;
}

/**
 * @brief Compute the Sun's lower culmination adjacent to a transit, before or after it.
 * @param transit The upper-culmination instant (JDE, TT scale).
 * @param before True for the lower culmination preceding the transit, false for the following one.
 * @param location The observer's location.
 * @return The lower-culmination instant (JDE, TT scale).
 * @note Solves H = ±180° with the wrap moved onto the upper culminations: the residual
 *       `normalize_pm180(H - 180°)` is smooth around the lower culmination, exactly as
 *       the plain unwrapped H is smooth around the transit.
 */
[[nodiscard]] inline auto lower_culmination_jde(
  const double transit,
  const bool before,
  const GeoLocation& location
) -> double {
  const auto f = [&location](const double jde) -> double {
    return astro::toolbox::normalize_pm180(sun_local(jde, location).hour_angle_deg - 180.0);
  };

  const double estimate = before ? transit - HALF_SOLAR_DAY_DAYS : transit + HALF_SOLAR_DAY_DAYS;
  return astro::toolbox::newton_method(
    f,
    estimate - LOWER_CULMINATION_BRACKET_HALF_WIDTH_DAYS,
    estimate + LOWER_CULMINATION_BRACKET_HALF_WIDTH_DAYS,
    astro::toolbox::SIDEREAL_RATE_DEG_PER_DAY
  );
}

} // namespace detail


/**
 * @brief Compute the hour angle at which a body of declination δ reaches altitude h₀, as seen
 *        from latitude φ.
 * @param δ The body's declination.
 * @param φ The observer's geographic latitude.
 * @param h0 The target altitude.
 * @return The (positive) hour angle H₀ ∈ [0°, 180°]; the body is at h₀ at hour angles ±H₀.
 *         Returns `nullopt` when the altitude is never reached (polar day/night) or when the
 *         equation degenerates (observer at a pole).
 * @throw std::invalid_argument If any argument is not finite or lies outside [-90°, 90°]
 *        (outside that domain sin/cos alias and (15.1) returns a physically meaningless H₀).
 * @ref Jean Meeus, "Astronomical Algorithms", Second Edition, Chapter 15, Formula (15.1).
 */
[[nodiscard]] inline auto hour_angle_at_altitude(
  const astro::toolbox::Angle<astro::toolbox::AngleUnit::DEG>& δ,
  const astro::toolbox::Angle<astro::toolbox::AngleUnit::DEG>& φ,
  const astro::toolbox::Angle<astro::toolbox::AngleUnit::DEG>& h0
) -> std::optional<astro::toolbox::Angle<astro::toolbox::AngleUnit::DEG>> {
  using astro::toolbox::Angle;
  using astro::toolbox::AngleUnit::DEG;
  using astro::toolbox::rad_to_deg;

  const auto reject_outside_pm90 = [](const char* name, const double deg) {
    if (not std::isfinite(deg) or deg < -90.0 or deg > 90.0) {
      throw std::invalid_argument {
        std::format("Argument `{}` out of range [-90, 90], whose value is {}", name, deg)
      };
    }
  };
  reject_outside_pm90("δ", δ.deg());
  reject_outside_pm90("φ", φ.deg());
  reject_outside_pm90("h0", h0.deg());

  const double denominator = std::cos(φ.rad()) * std::cos(δ.rad());
  if (std::fabs(denominator) < POLAR_DENOMINATOR_EPSILON) [[unlikely]] {
    return std::nullopt;
  }

  // Meeus (15.1): cos H₀ = (sin h₀ - sin φ sin δ) / (cos φ cos δ).
  double cos_H0 = (std::sin(h0.rad()) - std::sin(φ.rad()) * std::sin(δ.rad())) / denominator;

  // Roundoff can push a grazing event just past ±1, where acos would return NaN — clamp within
  // the tolerance, and treat anything beyond it as a real polar day (< -1) or night (> +1).
  if (cos_H0 < -1.0) {
    if (cos_H0 < -1.0 - COS_H0_CLAMP_TOLERANCE) {
      return std::nullopt;
    }
    cos_H0 = -1.0;
  }
  if (cos_H0 > 1.0) {
    if (cos_H0 > 1.0 + COS_H0_CLAMP_TOLERANCE) {
      return std::nullopt;
    }
    cos_H0 = 1.0;
  }

  return Angle<DEG> { rad_to_deg(std::acos(cos_H0)) };
}


/**
 * @brief Compute the instant of the Sun's upper culmination (solar transit / solar noon) on a date.
 * @param ymd The date, interpreted on the **UT1** scale (not local civil time — callers handle
 *        time zones). The returned transit is the one nearest 12h local mean time on this date.
 * @param location The observer's location.
 * @return The transit instant, as a julian ephemeris day on the **TT** scale.
 * @throw std::invalid_argument If `ymd` is invalid or `location` is out of range.
 * @note The root of H = 0 is found with `toolbox::newton_method` on the unwrapped hour angle;
 *       the initial bracket is local mean noon ± `TRANSIT_BRACKET_HALF_WIDTH_DAYS`, which the
 *       equation of time can never escape (see the constant's note).
 */
[[nodiscard]] inline auto transit_jde(
  const std::chrono::year_month_day& ymd,
  const GeoLocation& location
) -> double {
  detail::validate(location);

  // Local mean noon in UT1: 12h UT minus the east-positive longitude's worth of a day.
  // The offset is applied in JDE arithmetic (not in the Datetime fraction) so longitudes near
  // ±180° cannot push the fraction outside [0, 1).
  const calendar::Datetime noon_ut1 { ymd, 0.5 };
  const double estimate = astro::julian_day::ut1_to_jde(noon_ut1) - location.longitude.deg() / 360.0;

  const auto f = [&location](const double jde) -> double {
    return detail::sun_local(jde, location).hour_angle_deg;
  };

  return astro::toolbox::newton_method(
    f,
    estimate - TRANSIT_BRACKET_HALF_WIDTH_DAYS,
    estimate + TRANSIT_BRACKET_HALF_WIDTH_DAYS,
    astro::toolbox::SIDEREAL_RATE_DEG_PER_DAY
  );
}


/**
 * @brief Compute the instant at which the Sun crosses altitude h₀, before (sunrise) or after
 *        (sunset) a given transit.
 * @param transit The transit instant, as a julian ephemeris day on the **TT** scale
 *        (from `transit_jde`).
 * @param is_sunrise True for the upward crossing before transit, false for the downward
 *        crossing after it.
 * @param location The observer's location.
 * @param h0 The crossing altitude. Defaults to `STANDARD_ALTITUDE`; pass a twilight constant
 *        for dawn/dusk instants.
 * @return The crossing instant (JDE, TT scale), or `nullopt` when the Sun does not cross h₀
 *         between the transit and the adjacent lower culmination (polar day/night).
 * @throw std::invalid_argument If `transit` is not finite, `location` is out of range, or `h0`
 *        is not finite or outside [-90°, 90°].
 * @note The root of (altitude - h₀) = 0 is found with `toolbox::newton_method`. Working on the
 *       altitude directly sidesteps two of the classic pitfalls: there is no hand-written
 *       dh/dH derivative to get wrong (the solver differentiates numerically), and no hour-angle
 *       sign convention to enforce (the bracket side selects the event).
 * @note Whether a crossing exists is decided by a directed sign check at the bracket ends — the
 *       H₀-based estimate only accelerates the search, it never decides (#63's lesson: measure
 *       the quantity that actually drives the behavior). When the tight bracket misses (polar
 *       boundary, where extrapolating transit's δ degrades), the fallback bracket runs from the
 *       transit to the *solved* adjacent lower culmination — not to transit ± 0.5, whose ~±15 s
 *       error could swallow a short polar-boundary night whole (per review).
 */
[[nodiscard]] inline auto rise_set_jde(
  const double transit,
  const bool is_sunrise,
  const GeoLocation& location,
  const astro::toolbox::Angle<astro::toolbox::AngleUnit::DEG>& h0 = STANDARD_ALTITUDE
) -> std::optional<double> {
  detail::validate(location);

  if (not std::isfinite(transit)) {
    throw std::invalid_argument {
      std::format("Argument `transit` is not finite, whose value is {}", transit)
    };
  }
  if (not std::isfinite(h0.deg()) or h0.deg() < -90.0 or h0.deg() > 90.0) {
    throw std::invalid_argument {
      std::format("Argument `h0` out of range [-90, 90], whose value is {}", h0.deg())
    };
  }

  const auto f = [&location, &h0](const double jde) -> double {
    return detail::sun_altitude(jde, location).deg() - h0.deg();
  };

  // Sunrise is an upward crossing (f goes - to +), sunset a downward one; requiring the right
  // direction — not just a sign change — is what rejects brackets that contain no event.
  const auto straddles = [&f, is_sunrise](const double lo, const double hi) -> bool {
    const double f_lo = f(lo);
    const double f_hi = f(hi);
    return is_sunrise ? (f_lo < 0.0 and f_hi > 0.0) : (f_lo > 0.0 and f_hi < 0.0);
  };

  // The residual guard turns a degenerate solve into "no event" instead of a wrong instant;
  // see `RISE_SET_RESIDUAL_GUARD_DEG` (robustness only, per review).
  const auto solve = [&f](const double lo, const double hi) -> std::optional<double> {
    const double root = astro::toolbox::newton_method(f, lo, hi, astro::toolbox::SIDEREAL_RATE_DEG_PER_DAY);
    if (std::fabs(f(root)) > RISE_SET_RESIDUAL_GUARD_DEG) [[unlikely]] {
      return std::nullopt;
    }
    return root;
  };

  // First try: a tight bracket around the mean-rate extrapolation of H₀ from transit's δ.
  const auto eq_transit = astro::sun::equatorial_coord::apparent(transit);
  const auto H0 = hour_angle_at_altitude(eq_transit.δ, location.latitude, h0);
  const double sign = is_sunrise ? -1.0 : 1.0;

  if (H0.has_value()) {
    const double estimate = transit + sign * (H0->deg() / astro::toolbox::SIDEREAL_RATE_DEG_PER_DAY);
    const double lo = estimate - RISE_SET_BRACKET_HALF_WIDTH_DAYS;
    const double hi = estimate + RISE_SET_BRACKET_HALF_WIDTH_DAYS;
    if (straddles(lo, hi)) {
      // On a guard rejection fall THROUGH to the fallback rather than reporting "no event":
      // the straddle proved a root exists, so the tight bracket must never be the last word.
      if (const auto root = solve(lo, hi); root.has_value()) {
        return root;
      }
    }
  }

  // Fallback: from the transit to the solved adjacent lower culmination, whose endpoint IS the
  // Sun's altitude minimum — so the directed sign check there is the exact existence criterion.
  const double culmination = detail::lower_culmination_jde(transit, is_sunrise, location);
  const double lo = is_sunrise ? culmination : transit;
  const double hi = is_sunrise ? transit : culmination;
  if (straddles(lo, hi)) {
    return solve(lo, hi);
  }

  return std::nullopt;
}


/**
 * @brief Compute sunrise, transit, and sunset for a date and location.
 * @param ymd The date, interpreted on the **UT1** scale (callers handle time zones).
 * @param location The observer's location.
 * @param h0 The event altitude. Defaults to `STANDARD_ALTITUDE`; pass a twilight constant to
 *        compute dawn/dusk instead.
 * @return The three instants (JDE, TT scale) and the polar-day/night flags; see `Result`'s notes
 *         for the exact semantics.
 * @throw std::invalid_argument If `ymd` is invalid, `location` is out of range, or `h0` is not
 *        finite or outside [-90°, 90°].
 * @note Consumer trap (deliberate semantics): because `ymd` is a UT1 date, for eastern
 *       longitudes the returned sunrise can fall on the *previous* UT1 calendar day (e.g.
 *       Beijing's sunrise is ~21-22h UT of `ymd - 1`); callers building a local calendar day
 *       must convert with their time zone, not assume all three instants share `ymd`.
 */
[[nodiscard]] inline auto calculate(
  const std::chrono::year_month_day& ymd,
  const GeoLocation& location,
  const astro::toolbox::Angle<astro::toolbox::AngleUnit::DEG>& h0 = STANDARD_ALTITUDE
) -> Result {
  const double transit = transit_jde(ymd, location);

  Result result {
    .sunrise_jde = rise_set_jde(transit, true, location, h0),
    .sunset_jde  = rise_set_jde(transit, false, location, h0),
    .transit_jde = transit,
    .is_polar_day = false,
    .is_polar_night = false,
  };

  if (not result.sunrise_jde.has_value() and not result.sunset_jde.has_value()) {
    // `>=`: an exact graze (the Sun's center touching h₀ at transit without crossing) counts as
    // a polar day — the midnight-sun convention "never goes below" includes the touch.
    const bool above = detail::sun_altitude(transit, location).deg() >= h0.deg();
    result.is_polar_day = above;
    result.is_polar_night = not above;
  }

  return result;
}

} // namespace astro::sunrise_sunset
