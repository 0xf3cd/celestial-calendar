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

#include <algorithm>
#include <chrono>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <format>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <vector>

#include "toolbox.hpp"
#include "datetime.hpp"
#include "julian_day.hpp"
#include "coord_transform.hpp"
#include "sidereal_time.hpp"
#include "sun.hpp"
#include "moon.hpp"
#include "earth.hpp"
#include "earth/refraction.hpp"


/**
 * @namespace astro::rise_set
 * @brief Body-agnostic rise/transit/set engine (#62).
 * @note The engine knows nothing about any particular body: a body is injected as a
 *       `BodyProvider` (jde_tt → apparent equatorial coordinates), and the event
 *       altitude h₀ is injected by the caller. Per-body conventions (standard
 *       altitudes, ephemeris wiring) live in the `sun` / `moon` sub-namespaces;
 *       the generic layer stays usable for planets once their ephemerides land.
 */
namespace astro::rise_set {

#pragma region Constants

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
 * @brief The half-width of the root bracket around a transit estimate, in days.
 * @note For the Sun the estimate is local mean noon; the true transit deviates from it by the
 *       equation of time only (≤ 20.4 min ≈ 0.0142 day across the supported span — a 7.1x margin).
 *       Held by `RiseSet.TransitBracketCoversTheEquationOfTime` (#126).
 */
inline constexpr double TRANSIT_BRACKET_HALF_WIDTH_DAYS = 0.1;

/**
 * @brief The half-width of the first-try root bracket around the rise/set estimate, in days.
 * @note The estimate extrapolates H₀ computed from the declination at transit; for the Sun out to
 *       |φ| = 65° the δ drift between transit and the event moves the true root by ≤ 3.5 min, so
 *       ±72 min is a comfortable margin. Near the polar boundary the extrapolation degrades
 *       without bound — which is why this bracket is only an accelerator: the decider is the
 *       sign check at the true altitude extremum (see `rise_set_jde`).
 *       Held out to |φ| = 65° by `RiseSet.RiseSetBracketRetainsMargin` (#126).
 */
inline constexpr double RISE_SET_BRACKET_HALF_WIDTH_DAYS = 0.05;

/**
 * @brief The nominal offset of the adjacent lower culmination from a transit, in days — the
 *        mean-time *estimate* used to center the altitude-minimum search window.
 * @note This is only a search-window center, not a solved quantity: for the Sun the true
 *       altitude minimum deviates from the nominal half-day mark by ≤ ~16 s (measured peak
 *       16.00 s over the supported span), and the window half-width below dwarfs that.
 */
inline constexpr double LOWER_CULMINATION_OFFSET_DAYS = 0.5;

/**
 * @brief The half-width of the altitude-minimum search window around the nominal lower
 *        culmination, in days.
 * @note For the Sun the true minimum sits within ~16 s of the nominal half-day mark — a 500x
 *       margin (held by `RiseSet.MinSearchWindowRetainsMargin`, #126). The minimum is found by
 *       golden-section search over this window, NOT by solving
 *       H = ±180°: with dδ/dt ≠ 0 (the Moon runs ±13°/day) the altitude extremum wanders off the
 *       lower culmination by degrees of hour angle, and anchoring the existence check to H = 180°
 *       leaves a blind band of order 0.01°–0.05° of altitude that silently mislabels grazing
 *       days as polar (#62 mechanism 2).
 */
inline constexpr double MIN_SEARCH_HALF_WIDTH_DAYS = 0.1;

/**
 * @brief The residual guard on a transit root, in degrees of hour angle.
 * @note Same philosophy as `RISE_SET_RESIDUAL_GUARD_DEG`: a converged Newton solve leaves
 *       |H| ~ 1e-7°, so this guard (4 orders looser) only fires when the bracket held no root
 *       and `newton_method` returned a best-effort iterate — a numerical failure that must
 *       never ship as a fake transit or a fake eventless window.
 */
inline constexpr double TRANSIT_RESIDUAL_GUARD_DEG = 1e-3;

/**
 * @brief The residual guard on a rise/set root, in degrees of altitude.
 * @note Robustness only: after a directed sign check the bracketed Newton solve converges to
 *       residuals ~1e-7°; this guard (4 orders looser) exists so that a degenerate solve —
 *       `newton_method` may return a best-effort iterate when f′ collapses — never ships a wrong
 *       instant: a straddle that proved a root exists throws instead of mislabeling the day.
 */
inline constexpr double RISE_SET_RESIDUAL_GUARD_DEG = 1e-3;

/**
 * @brief Convergence tolerance for the golden-section extremum searches, in days.
 * @note 1e-9 day ≈ 0.1 ms of time — far below the precision the rise/set residual guard
 *       (1e-3°) cares about. In the representable range's ordinary part this tolerance is
 *       what ends the search (≤ 40 iterations for any bracket here); past JDE 2²³
 *       (≈ year 18255) the double ulp exceeds it and only the iteration cap in
 *       `golden_section_argmin` ends the loop (see that function's note).
 */
inline constexpr double EXTREMUM_SEARCH_TOLERANCE_DAYS = 1e-9;

/**
 * @brief Coarser tolerance for the edge-cell probes of `find_extrema`, in days.
 * @note The probes only locate a cut point — crossings are re-solved by Newton afterwards,
 *       and the polar verdict's altitude values need ~1e-3° — so ~0.1 s of time precision
 *       is far more than enough, and the coarser target cuts the probe's evaluation count
 *       by ~40% (20 vs 34 iterations on a 15-min cell; R4 measured 144 → 88 provider
 *       evaluations per `find_extrema`).
 */
inline constexpr double EDGE_PROBE_TOLERANCE_DAYS = 1e-6;

#pragma endregion


#pragma region Types

/**
 * @brief An observer's location on the Earth.
 * @note `longitude` is **positive east** of Greenwich (the modern/ISO 6709 convention),
 *       in [-180°, 180°]. This is the opposite of Meeus's west-positive convention used by
 *       `sidereal::local_apparent`; the negation happens inside this namespace, so callers
 *       never deal with west-positive longitudes.
 * @note +180° and -180° name the same meridian but are NOT interchangeable for the
 *       date-anchored solar APIs (`sun::transit_jde`, `sun::calculate`): the date input is a
 *       UT1 date, so they select transit-centered windows one day apart (local mean noon at
 *       +180° is 0h UT of that date; at -180° it is 24h UT). Pick the sign matching the
 *       intended UT window — deliberate, same behavior as other UT-date APIs. For the UT-day
 *       APIs (`calculate_day`, `moon::calculate`) the window is determined by `ymd` alone,
 *       and the two signs give identical results.
 */
struct GeoLocation {
  astro::toolbox::AngleDeg latitude;  // North-positive, [-90°, 90°].
  astro::toolbox::AngleDeg longitude; // East-positive, [-180°, 180°].
};

/**
 * @brief The whole-day topology of a date: whether the body crosses h₀ at all, and if not,
 *        which side of h₀ it stays on.
 * @note First-class member of `Result` — the event topology (normal / polar day / polar
 *       night) is part of the result's type, not inferred from counting empty optionals
 *       (design constraint from issue #62, 2026-08-02).
 */
enum class Polar : uint8_t { NONE, DAY, NIGHT };

/**
 * @brief The result of a rise/transit/set calculation for one date.
 * @note All instants are JDE, on the **TT** scale, like every other moment produced by this
 *       library. Use `julian_day::jde_to_ut1` to read them as civil (UT1) datetimes.
 * @note `transit_jde` is `nullopt` only for window-based queries (`calculate_day`) on dates
 *       where the body does not transit inside the window — a regular occurrence for the Moon,
 *       whose transits are ~24.84 h apart. Transit-centered queries always produce a transit.
 * @note `polar` is `DAY`/`NIGHT` only when neither rise nor set exists in the queried window;
 *       on transition days one of the two events can exist alone, in which case it stays `NONE`.
 */
struct Result {
  std::optional<double> rise_jde;     // The rise instant, or nullopt if the body never crosses h₀ upward.
  std::optional<double> set_jde;      // The set instant, or nullopt if the body never crosses h₀ downward.
  std::optional<double> transit_jde;  // The upper-culmination instant, if it exists in the window.
  Polar polar;                        // The no-crossing topology; see the enum's note.
};

/**
 * @brief A body position provider: apparent equatorial coordinates at one instant.
 * @note The instant is a julian ephemeris day on the **TT** scale. This is the single seam
 *       through which the engine sees a body; per-body sub-namespaces supply the providers.
 */
template <typename P>
concept BodyProvider = std::invocable<const P&, double>
                   and std::convertible_to<std::invoke_result_t<const P&, double>, astro::coords::EquatorialCoord>;

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
      std::format("Argument `location.latitude` out of range [-90, 90], got {}", lat)
    };
  }

  if (not std::isfinite(lon) or lon < -180.0 or lon > 180.0) {
    throw std::invalid_argument {
      std::format("Argument `location.longitude` out of range [-180, 180], got {}", lon)
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

/** @brief A body's local hour angle and equatorial position at one instant. */
struct BodyLocal {
  double hour_angle_deg;                // The local hour angle H = θ(LAST) - α, unwrapped to [-180°, 180°).
  astro::coords::EquatorialCoord eq;    // The body's apparent equatorial coordinates (α, δ).
};

/**
 * @brief Compute a body's local hour angle and equatorial coordinates for an observer.
 * @param jde_tt The instant, as a julian ephemeris day on the **TT** scale.
 * @param location The observer's location.
 * @param provider The body's position provider.
 * @return The hour angle (unwrapped to [-180°, 180°)) and the equatorial coordinates.
 * @note The sidereal time is evaluated on the UT1 scale (pitfall: feeding TT would shift the
 *       result by ΔT ≈ 69 s); the body's position and the nutation terms are evaluated on TT.
 */
template <BodyProvider P>
[[nodiscard]] inline auto body_local(const double jde_tt, const GeoLocation& location, const P& provider) -> BodyLocal {
  const double jd_ut1 = jde_tt_to_jd_ut1(jde_tt);
  const auto eq = provider(jde_tt);

  // `local_apparent` wants Meeus's west-positive longitude; `GeoLocation` carries east-positive.
  const auto θ = astro::sidereal::local_apparent(jd_ut1, jde_tt, -location.longitude);

  return {
    .hour_angle_deg = astro::toolbox::normalize_pm180((θ - eq.α).deg()),
    .eq = eq,
  };
}

/**
 * @brief Compute a body's geometric altitude for an observer at one instant.
 * @param jde_tt The instant, as a julian ephemeris day on the **TT** scale.
 * @param location The observer's location.
 * @param provider The body's position provider.
 * @return The body's altitude, in [-90°, 90°]. Purely geometric — refraction enters only through
 *         the h₀ convention, not through this function.
 */
template <BodyProvider P>
[[nodiscard]] inline auto altitude(
  const double jde_tt,
  const GeoLocation& location,
  const P& provider
) -> astro::toolbox::AngleDeg {
  using astro::toolbox::AngleDeg;

  const auto local = body_local(jde_tt, location, provider);
  const auto horizontal = astro::coords::equatorial_to_horizontal(
    AngleDeg { local.hour_angle_deg }, local.eq.δ, location.latitude
  );
  return horizontal.h;
}

/**
 * @brief Find the argmin of a unimodal function on [a, b] by golden-section search.
 * @note Derivative-free and unconditionally convergent — the altitude curve's derivative is
 *       not written by hand anywhere in this engine, and this keeps it that way. Converges
 *       to a bracket endpoint when the function is monotonic on [a, b] (polar-boundary arcs).
 * @note In the ordinary part of the date range the loop ends on `tolerance` (≤ 40 iterations
 *       for any bracket used here); the ITERATION CAP is the guarantee, not the normal exit:
 *       once the JDE magnitude passes 2²³ (≈ year 18255) the double ulp exceeds
 *       `EXTREMUM_SEARCH_TOLERANCE_DAYS`, the bracket can never shrink to the tolerance, and
 *       without a cap the loop hangs forever (R4 实录, inside the declared domain). After
 *       the cap the midpoint of the final, ulp-wide bracket is returned — the best answer
 *       representable at that magnitude.
 */
template <typename Func>
requires std::invocable<const Func&, double>
     and std::convertible_to<std::invoke_result_t<const Func&, double>, double>
[[nodiscard]] inline auto golden_section_argmin(
  const Func& f,
  const double a,
  const double b,
  const double tolerance = EXTREMUM_SEARCH_TOLERANCE_DAYS
) -> double {
  constexpr double INV_PHI = 0.61803398874989484820; // (√5 − 1)/2
  constexpr double INV_PHI_SQ = 0.38196601125010515180; // (3 − √5)/2
  constexpr int MAX_ITERATIONS = 64; // ≥ the 40 any bracket/tolerance here needs (0.2-day
                                     // bracket at 1e-9); see the @note above.

  double lo = a;
  double hi = b;
  double x1 = lo + (INV_PHI_SQ * (hi - lo));
  double x2 = lo + (INV_PHI * (hi - lo));
  double f1 = f(x1);
  double f2 = f(x2);

  for (int i = 0; i < MAX_ITERATIONS and hi - lo > tolerance; ++i) {
    if (f1 < f2) {
      hi = x2;
      x2 = x1;
      f2 = f1;
      x1 = lo + (INV_PHI_SQ * (hi - lo));
      f1 = f(x1);
    } else {
      lo = x1;
      x1 = x2;
      f1 = f2;
      x2 = lo + (INV_PHI * (hi - lo));
      f2 = f(x2);
    }
  }
  return (lo + hi) / 2.0;
}

/** @brief A refined local extremum of the altitude curve: its instant, kind, and value. */
struct AltitudeExtremum {
  double jde;
  bool is_minimum;
  double altitude_deg;
};

/**
 * @brief Find every local extremum of the altitude curve on [a, b], in time order.
 * @note Grid scan for direction changes (15-min cells on a 1-day window) plus a direct probe
 *       of each edge cell, then golden-section refinement per candidate. The window must be
 *       partitioned at ALL of these, not just at the global minimum and maximum: the global
 *       extremum need not be a monotone cut (R2 实录:2026-06-18 Tromsø, the window's end
 *       dips below the interior minimum, and partitioning at the global minimum hid that
 *       day's only rise inside a non-monotone segment).
 */
template <typename Func>
requires std::invocable<const Func&, double>
     and std::convertible_to<std::invoke_result_t<const Func&, double>, double>
[[nodiscard]] inline auto find_extrema(
  const Func& f,
  const double a,
  const double b
) -> std::vector<AltitudeExtremum> {
  constexpr int CELLS = 96; // 15 min on a 1-day window.
  const double step = (b - a) / CELLS;

  std::vector<double> values(static_cast<std::size_t>(CELLS) + 1);
  for (int i = 0; i <= CELLS; ++i) {
    values[static_cast<std::size_t>(i)] = f(a + (i * step));
  }

  std::vector<AltitudeExtremum> extrema;
  for (int i = 1; i < CELLS; ++i) {
    const double prev = values[static_cast<std::size_t>(i - 1)];
    const double curr = values[static_cast<std::size_t>(i)];
    const double next = values[static_cast<std::size_t>(i + 1)];
    // Strict on both sides: a non-strict comparison fires on the edge of a flat plateau
    // (e.g. exp() tails underflowing to exact zeros), manufacturing a spurious extremum
    // out of numerical noise.
    const bool is_min = (curr < prev) and (curr < next);
    const bool is_max = (curr > prev) and (curr > next);
    if (not is_min and not is_max) {
      continue;
    }

    const double lo = a + ((i - 1) * step);
    const double hi = a + ((i + 1) * step);
    const double jde = is_min ? golden_section_argmin(f, lo, hi)
                              : golden_section_argmin([&f](const double t) { return -f(t); }, lo, hi);
    extrema.push_back({ .jde = jde, .is_minimum = is_min, .altitude_deg = f(jde) });
  }

  // Edge cells: an extremum inside the first or last cell has no grid neighbor on one side,
  // so the direction check above can miss it (R3 实录:1.5% of lunar days, worst 0.53° —
  // and it is exactly the band a grazing h₀ query asks about). Probe both edge cells
  // directly; a candidate counts only if it lands strictly inside the cell and beats both
  // cell ends. Skip duplicates of grid-found extrema (a bump near the cell boundary shows
  // up in both) — same kind and sub-second distance, nothing broader: two genuinely
  // distinct extrema can share one cell (R4: a whole-cell, kind-blind radius ate them).
  const auto probe_edge_cell = [&](const double lo, const double hi) {
    for (const bool want_min : { true, false }) {
      const double jde = want_min
        ? golden_section_argmin(f, lo, hi, EDGE_PROBE_TOLERANCE_DAYS)
        : golden_section_argmin([&f](const double t) { return -f(t); }, lo, hi, EDGE_PROBE_TOLERANCE_DAYS);
      // The loop exits with (hi − lo) ≤ tolerance, so the returned midpoint sits within
      // tolerance/2 of the true extremum — and of the cell end, when the extremum IS the
      // endpoint. "Strictly inside" therefore needs an epsilon larger than tolerance/2;
      // EDGE_EPSILON_DAYS and EDGE_PROBE_TOLERANCE_DAYS are tied by that constraint
      // (currently 2x), and loosening the tolerance without the epsilon breaks it.
      constexpr double EDGE_EPSILON_DAYS = 1e-6;
      if (jde - lo <= EDGE_EPSILON_DAYS or hi - jde <= EDGE_EPSILON_DAYS) {
        continue;
      }
      const double v = f(jde);
      const bool beats = want_min ? (v < f(lo) and v < f(hi)) : (v > f(lo) and v > f(hi));
      if (not beats) {
        continue;
      }
      const bool duplicate = std::ranges::any_of(extrema, [&](const AltitudeExtremum& e) {
        return e.is_minimum == want_min and std::fabs(e.jde - jde) < EDGE_EPSILON_DAYS;
      });
      if (duplicate) {
        continue;
      }
      extrema.push_back({ .jde = jde, .is_minimum = want_min, .altitude_deg = v });
    }
  };
  probe_edge_cell(a, a + step);
  probe_edge_cell(b - step, b);

  // The partition in `calculate_day` consumes these in time order. The collection order is
  // NOT time order: the grid loop emits in order, but the edge probes append afterwards —
  // a first-cell candidate lands behind every grid-found extremum even when it precedes
  // them all. Sorting is what makes the partition's monotone-segment premise hold.
  std::ranges::sort(extrema, {}, &AltitudeExtremum::jde);

  return extrema;
}

/**
 * @brief Find the instant of minimum altitude in the half-arc adjacent to a transit.
 * @param transit The upper-culmination instant (JDE, TT scale).
 * @param before True for the half-arc preceding the transit, false for the following one.
 * @param location The observer's location.
 * @param provider The body's position provider.
 * @return The argmin instant (JDE, TT scale).
 * @note Golden-section search around the nominal lower culmination — NOT a solve of H = ±180°.
 *       With dδ/dt ≠ 0 the true altitude minimum wanders off the lower culmination (for the
 *       Moon, degrees of hour angle); the existence check must anchor on the true minimum,
 *       or grazing days fall into a blind band and read as polar (#62 mechanism 2).
 */
template <BodyProvider P>
[[nodiscard]] inline auto min_altitude_jde(
  const double transit,
  const bool before,
  const GeoLocation& location,
  const P& provider
) -> double {
  const auto f = [&location, &provider](const double jde) -> double {
    return altitude(jde, location, provider).deg();
  };

  const double center = before ? transit - LOWER_CULMINATION_OFFSET_DAYS
                               : transit + LOWER_CULMINATION_OFFSET_DAYS;
  return golden_section_argmin(f, center - MIN_SEARCH_HALF_WIDTH_DAYS, center + MIN_SEARCH_HALF_WIDTH_DAYS);
}

/**
 * @brief Validate `rise_set_jde`'s inputs.
 * @throw std::invalid_argument If `transit` is not finite, `location` is out of range, or `h0`
 *        is not finite or outside [-90°, 90°].
 */
inline void validate_rise_set_inputs(
  const double transit,
  const GeoLocation& location,
  const astro::toolbox::AngleDeg& h0
) {
  validate(location);

  if (not std::isfinite(transit)) {
    throw std::invalid_argument {
      std::format("Argument `transit` is not finite, got {}", transit)
    };
  }
  if (not std::isfinite(h0.deg()) or h0.deg() < -90.0 or h0.deg() > 90.0) {
    throw std::invalid_argument {
      std::format("Argument `h0` out of range [-90, 90], got {}", h0.deg())
    };
  }
}

/** @brief A refined crossing and the |altitude − h₀| residual it left, in degrees. */
struct Solved {
  double root_jde;
  double residual_deg;
};

/**
 * @brief Try to solve one altitude crossing inside a bracket.
 * @param f The residual function (altitude − h₀), in degrees, over JDE (TT).
 * @param lo_jde The left bracket end, inclusive.
 * @param hi_jde The right bracket end, exclusive.
 * @param rising True for an upward crossing (rise side), false for a downward one.
 * @return `nullopt` when the bracket ends do not straddle a crossing in the required direction
 *         (a plain sign change is not enough — direction is what rejects event-free brackets);
 *         otherwise the refined root plus its residual. The residual verdict is the caller's:
 *         a straddle proves a root exists, so what a rejection *means* depends on the bracket.
 */
// The `_jde` suffixes carry the contract at the call site.
// NOLINTBEGIN(bugprone-easily-swappable-parameters)
template <typename Func>
requires std::invocable<const Func&, double>
     and std::convertible_to<std::invoke_result_t<const Func&, double>, double>
[[nodiscard]] inline auto crossing_in_bracket(
  const Func& f,
  const double lo_jde,
  const double hi_jde,
  const bool rising
) -> std::optional<Solved> {
  const double f_lo = f(lo_jde);
  const double f_hi = f(hi_jde);

  const bool straddles = rising ? (f_lo < 0.0 and f_hi > 0.0) : (f_lo > 0.0 and f_hi < 0.0);
  if (not straddles) {
    return std::nullopt;
  }

  const double root = astro::toolbox::newton_method(
    f, lo_jde, hi_jde, astro::toolbox::SIDEREAL_RATE_DEG_PER_DAY
  );
  return Solved { .root_jde = root, .residual_deg = std::fabs(f(root)) };
}
// NOLINTEND(bugprone-easily-swappable-parameters)

/**
 * @brief Polish a transit estimate into the instant of upper culmination (H = 0).
 * @param estimate_jde The estimated transit instant, as a julian ephemeris day on the **TT**
 *        scale. Must lie within `TRANSIT_BRACKET_HALF_WIDTH_DAYS` of the true transit.
 * @param location The observer's location.
 * @param provider The body's position provider.
 * @return The transit instant (JDE, TT scale).
 * @throw std::invalid_argument If `location` is out of range.
 * @note The root of H = 0 is found with `toolbox::newton_method` on the unwrapped hour angle.
 */
template <BodyProvider P>
[[nodiscard]] inline auto polish_transit(
  const double estimate_jde,
  const GeoLocation& location,
  const P& provider
) -> double {
  validate(location);

  const auto f = [&location, &provider](const double jde) -> double {
    return body_local(jde, location, provider).hour_angle_deg;
  };

  return astro::toolbox::newton_method(
    f,
    estimate_jde - TRANSIT_BRACKET_HALF_WIDTH_DAYS,
    estimate_jde + TRANSIT_BRACKET_HALF_WIDTH_DAYS,
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
 *         equation degenerates — |cos φ · cos δ| < `POLAR_DENOMINATOR_EPSILON`, i.e. the
 *         observer at a geographic pole or the body at/near a celestial pole.
 * @throw std::invalid_argument If any argument is not finite or lies outside [-90°, 90°]
 *        (outside that domain sin/cos alias and (15.1) returns a physically meaningless H₀).
 * @ref Jean Meeus, "Astronomical Algorithms", Second Edition, Chapter 15, Formula (15.1).
 */
[[nodiscard]] inline auto hour_angle_at_altitude(
  const astro::toolbox::AngleDeg& δ,
  const astro::toolbox::AngleDeg& φ,
  const astro::toolbox::AngleDeg& h0
) -> std::optional<astro::toolbox::AngleDeg> {
  using astro::toolbox::AngleDeg;
  using astro::toolbox::rad_to_deg;

  const auto reject_outside_pm90 = [](const char* name, const double deg) {
    if (not std::isfinite(deg) or deg < -90.0 or deg > 90.0) {
      throw std::invalid_argument {
        std::format("Argument `{}` out of range [-90, 90], got {}", name, deg)
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
  double cos_H0 = (std::sin(h0.rad()) - (std::sin(φ.rad()) * std::sin(δ.rad()))) / denominator;

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

  return AngleDeg { rad_to_deg(std::acos(cos_H0)) };
}


/**
 * @brief Compute the instant at which a body crosses altitude h₀, before (rise) or after
 *        (set) a given transit.
 * @param transit The transit instant, as a julian ephemeris day on the **TT** scale.
 * @param is_rise True for the upward crossing before transit, false for the downward
 *        crossing after it.
 * @param location The observer's location.
 * @param h0 The crossing altitude.
 * @param provider The body's position provider.
 * @return The crossing instant (JDE, TT scale), or `nullopt` when the body does not cross h₀
 *         between the transit and the adjacent altitude minimum (polar day/night).
 * @throw std::invalid_argument If `transit` is not finite, `location` is out of range, or `h0`
 *        is not finite or outside [-90°, 90°].
 * @throw std::runtime_error If a directed sign change proved a crossing exists but the solve
 *        failed the residual guard — a numerical failure must not read as a polar verdict.
 *        The UT1/JD conversions also propagate `std::runtime_error` when `transit` lies
 *        outside the representable years.
 * @note Solves (altitude − h₀) = 0 directly: no hand-written dh/dH derivative, no hour-angle
 *       sign convention — the bracket side selects the event. Existence is decided by the
 *       directed sign check at the *true* altitude minimum (`detail::min_altitude_jde`,
 *       golden-section — exact also under dδ/dt ≠ 0); the H₀-based bracket only accelerates.
 */
template <BodyProvider P>
[[nodiscard]] inline auto rise_set_jde(
  const double transit,
  const bool is_rise,
  const GeoLocation& location,
  const astro::toolbox::AngleDeg& h0,
  const P& provider
) -> std::optional<double> {
  detail::validate_rise_set_inputs(transit, location, h0);

  const auto f = [&location, &h0, &provider](const double jde) -> double {
    return detail::altitude(jde, location, provider).deg() - h0.deg();
  };

  // First try: a tight bracket around the mean-rate extrapolation of H₀ from transit's δ.
  // (Reusing the sidereal rate overstates the Sun's ~360.0°/day sweep by 0.27% — ~1 min at
  // H₀ ≈ 90°; the ±72 min bracket absorbs it, the sign check decides. For the Moon the
  // extrapolation is coarser and more often falls through to the authority path below.)
  const auto eq_transit = provider(transit);
  const auto H0 = hour_angle_at_altitude(eq_transit.δ, location.latitude, h0);

  if (H0.has_value()) {
    const double sign = is_rise ? -1.0 : 1.0;
    const double estimate = transit + (sign * (H0->deg() / astro::toolbox::SIDEREAL_RATE_DEG_PER_DAY));
    const auto solved = detail::crossing_in_bracket(
      f,
      estimate - RISE_SET_BRACKET_HALF_WIDTH_DAYS,
      estimate + RISE_SET_BRACKET_HALF_WIDTH_DAYS,
      is_rise
    );
    if (solved.has_value() and solved->residual_deg <= RISE_SET_RESIDUAL_GUARD_DEG) [[likely]] {
      return solved->root_jde;
    }
    // A guard rejection falls through — the tight bracket must never be the last word.
  }

  // The authority: from the transit to the *true* altitude minimum in the half-arc — the
  // directed sign check there is the exact existence criterion, for any dδ/dt.
  const double min_jde = detail::min_altitude_jde(transit, is_rise, location, provider);
  const double lo = is_rise ? min_jde : transit;
  const double hi = is_rise ? transit : min_jde;

  const auto solved = detail::crossing_in_bracket(f, lo, hi, is_rise);
  if (not solved.has_value()) {
    return std::nullopt; // No directed crossing in the half arc: polar day/night.
  }
  if (solved->residual_deg <= RISE_SET_RESIDUAL_GUARD_DEG) [[likely]] {
    return solved->root_jde;
  }

  // This straddle proved a crossing exists, so a guard rejection here is a numerical failure —
  // surface it loudly instead of letting `calculate` turn it into a wrong polar-day/night flag.
  throw std::runtime_error {
    std::format(
      "rise_set_jde: bracketed {} root failed to converge: |altitude - h0| = {} deg at jde {} "
      "(bracket [{}, {}], guard {} deg)",
      is_rise ? "rise" : "set",
      solved->residual_deg, solved->root_jde, lo, hi, RISE_SET_RESIDUAL_GUARD_DEG
    )
  };
}


/**
 * @brief Compute rise, transit, and set around a known transit (transit-centered window).
 * @param transit The transit instant anchoring the window, as a julian ephemeris day on the
 *        **TT** scale.
 * @param location The observer's location.
 * @param h0 The event altitude.
 * @param provider The body's position provider.
 * @return The three instants (JDE, TT scale) and the polar topology; see `Result`'s notes.
 * @throw std::invalid_argument / std::runtime_error See the generic `rise_set_jde`, whose
 *        validation and residual-guard failures propagate through this function.
 * @note The rise is searched between the preceding altitude minimum and the transit, the set
 *       between the transit and the following one — the window is a half-period on each side,
 *       NOT a calendar day. For the Moon this means consecutive dates' results tile the
 *       timeline without gaps or double-counts only through their own transits; callers wanting
 *       almanac-style "events on this UT date" semantics should use `calculate_day`.
 */
template <BodyProvider P>
[[nodiscard]] inline auto calculate_around_transit(
  const double transit,
  const GeoLocation& location,
  const astro::toolbox::AngleDeg& h0,
  const P& provider
) -> Result {
  Result result {
    .rise_jde = rise_set_jde(transit, true, location, h0, provider),
    .set_jde  = rise_set_jde(transit, false, location, h0, provider),
    .transit_jde = transit,
    .polar = Polar::NONE,
  };

  if (not result.rise_jde.has_value() and not result.set_jde.has_value()) {
    // `>`, not `>=`: an exact graze AT the transit means the day's *highest* point merely
    // touched h₀ — every other instant stayed below it, which is a polar NIGHT, not a day.
    // (The transit-centered twin of `calculate_day`'s `h_max <= h0 → NIGHT`.)
    const bool above = detail::altitude(transit, location, provider).deg() > h0.deg();
    result.polar = above ? Polar::DAY : Polar::NIGHT;
  }

  return result;
}


/**
 * @brief Find a body's first upper culmination inside a time window, if there is one (#62 mechanism 1).
 * @param t0_jde The window start (inclusive), as a julian ephemeris day on the **TT** scale.
 * @param t1_jde The window end (exclusive), same scale.
 * @param location The observer's location.
 * @param provider The body's position provider.
 * @return The first transit instant in the window (JDE, TT scale), or `nullopt` when no transit
 *         falls in the window — a regular event for slow bodies (the Moon transits ~24.84 h
 *         apart, so a 1-day window without a transit occurs roughly one day in 29). If the
 *         window holds more than one transit (possible when it is not shorter than the body's
 *         transit period — the solar day dips below 24 h around the equinoxes, so even a 1-day
 *         window can contain two solar transits), the FIRST one is returned.
 * @throw std::invalid_argument If `location` is out of range, or the window is not finite and
 *        positive.
 * @throw std::runtime_error If the polished root fails the hour-angle residual guard — the
 *        estimate's bracket held no root, which is a numerical failure and must not read as
 *        an eventless window. The date-range guard of the UT1/JD conversions also propagates
 *        as `std::runtime_error` (reachable near the representable-range top, where the
 *        1-day α sample overshoots).
 * @note The estimate is Meeus Ch.15's m₀ in secant form: the hour angle sweeps at
 *       (sidereal rate − dα/dt), and dα/dt is *measured* from the provider over a fixed
 *       1-day span from t0 — the rate is a property of the body, not of the window, so the
 *       estimate is window-length-independent. The span aliases only if |dα/dt| ever
 *       reached 180°/day; the library's own providers stay an order of magnitude below
 *       that, and a custom provider that does not is outside this premise. Near the top of
 *       the representable date range the 1-day sample can itself leave the range and throw
 *       (see @throw).
 */
template <BodyProvider P>
[[nodiscard]] inline auto transit_in_window(
  const double t0_jde,
  const double t1_jde,
  const GeoLocation& location,
  const P& provider
) -> std::optional<double> {
  detail::validate(location);

  if (not std::isfinite(t0_jde) or not std::isfinite(t1_jde) or t1_jde <= t0_jde) {
    throw std::invalid_argument {
      std::format("transit_in_window: invalid window [{}, {}]", t0_jde, t1_jde)
    };
  }

  // Signed α drift per day, measured over a fixed 1-day span from t0 (see the @note: a body
  // property, not a window property — measuring across the whole window aliases once the
  // window spans more than half a turn of α, which for the Moon happens past ~13.6 days).
  const auto local0 = detail::body_local(t0_jde, location, provider);
  const auto local1 = detail::body_local(t0_jde + 1.0, location, provider);
  const double dα = astro::toolbox::normalize_pm180(local1.eq.α.deg() - local0.eq.α.deg());
  const double sweep_rate = astro::toolbox::SIDEREAL_RATE_DEG_PER_DAY - dα;

  // Hour angle from here to the next transit, in degrees, in (0°, 360°].
  const double H0 = local0.hour_angle_deg; // ∈ [-180°, 180°)
  const double forward_deg = H0 <= 0.0 ? -H0 : 360.0 - H0;

  const double estimate = t0_jde + (forward_deg / sweep_rate);

  const auto f = [&location, &provider](const double jde) -> double {
    return detail::body_local(jde, location, provider).hour_angle_deg;
  };
  const double root = astro::toolbox::newton_method(
    f,
    estimate - RISE_SET_BRACKET_HALF_WIDTH_DAYS,
    estimate + RISE_SET_BRACKET_HALF_WIDTH_DAYS,
    sweep_rate
  );

  // Residual guard: `newton_method` returns a best-effort iterate when its bracket holds no
  // root, so a converged-looking answer can be degrees of hour angle away from a real transit.
  // That is a numerical failure — say so loudly instead of shipping a fake transit or a fake
  // eventless window (same philosophy as `rise_set_jde`'s residual guard).
  const double H_root = detail::body_local(root, location, provider).hour_angle_deg;
  if (std::fabs(H_root) > TRANSIT_RESIDUAL_GUARD_DEG) [[unlikely]] {
    throw std::runtime_error {
      std::format(
        "transit_in_window: bracketed root failed to converge: |H| = {} deg at jde {} "
        "(estimate {}, bracket ±{} day, guard {} deg)",
        H_root, root, estimate, RISE_SET_BRACKET_HALF_WIDTH_DAYS, TRANSIT_RESIDUAL_GUARD_DEG
      )
    };
  }

  if (root < t0_jde or root >= t1_jde) {
    return std::nullopt;
  }
  return root;
}


/**
 * @brief Compute rise, transit, and set for one UT1 calendar day (almanac semantics).
 * @param ymd The date, interpreted on the **UT1** scale (callers handle time zones). The
 *        window is [0h, 24h) UT1 of this date.
 * @param location The observer's location.
 * @param h0 The event altitude.
 * @param provider The body's position provider.
 * @return The events inside the UT day (JDE, TT scale) and the polar topology. Any of the
 *         three instants may be absent: slow bodies (the Moon) regularly have UT dates with
 *         no rise, no set, or no transit — that absence is calendar arithmetic, NOT a polar
 *         verdict. `polar` is `DAY`/`NIGHT` only when no crossing exists *and* the altitude
 *         stays entirely on one side of h₀ for the whole day.
 * @note **At most one event of each kind is reported.** Days with two crossings of the same
 *         kind exist — lunar high-latitude days near a standstill (the Moon's declination
 *         swings fast enough that it can rise twice in one UT day; USNO lists both in the
 *         same cell), and any body whose diurnal period dips below 24 h. On such days the
 *         LATER crossing is reported. This library reports one event per cell; almanacs that
 *         list both keep the earlier one visible too.
 * @note Mechanism: the day is partitioned at its detected altitude extrema
 *         (`detail::find_extrema`), each segment's crossing is decided by a directed sign
 *         check, and the polar verdict compares the day's global extremes (window ends
 *         included) against h₀.
 * @note Boundary attribution: the window is [0h, 24h), and the crossing checks are strict
 *         sign comparisons, so an event landing EXACTLY on a UT midnight is not owned by
 *         either adjacent day. That is a measure-zero edge (the TT/UT1 conversions make an
 *         exact landing practically unreachable for real ephemerides), accepted deliberately
 *         rather than given a fake-precision rounding rule.
 * @throw std::invalid_argument If `ymd` is invalid, `location` is out of range, or `h0` is
 *        not finite or outside [-90°, 90°].
 * @throw std::runtime_error For chronologically valid but unsupported dates (the UT1/JD
 *        conversions reject dates outside the representable years), if the altitude straddles h₀ inside the day
 *        but no crossing solve converged, if a bracketed crossing failed the residual
 *        guard, or if the transit solve failed its own residual guard (see
 *        `transit_in_window`) — a numerical failure must not read as "no event".
 */
template <BodyProvider P>
[[nodiscard]] inline auto calculate_day(
  const std::chrono::year_month_day& ymd,
  const GeoLocation& location,
  const astro::toolbox::AngleDeg& h0,
  const P& provider
) -> Result {
  detail::validate(location);

  if (not std::isfinite(h0.deg()) or h0.deg() < -90.0 or h0.deg() > 90.0) {
    throw std::invalid_argument {
      std::format("Argument `h0` out of range [-90, 90], got {}", h0.deg())
    };
  }

  const calendar::Datetime day_start { ymd, 0.0 };
  const double t0 = astro::julian_day::ut1_to_jde(day_start);
  const double t1 = t0 + 1.0;

  const auto h = [&location, &provider](const double jde) -> double {
    return detail::altitude(jde, location, provider).deg();
  };
  const auto f = [&h, &h0](const double jde) -> double {
    return h(jde) - h0.deg();
  };

  const auto extrema = detail::find_extrema(h, t0, t1);

  Result result { .rise_jde = std::nullopt, .set_jde = std::nullopt,
                  .transit_jde = transit_in_window(t0, t1, location, provider),
                  .polar = Polar::NONE };

  // Monotone segments between the window ends and every interior extremum. Last-wins
  // assignment is deliberate: it implements the one-event-per-cell contract (the LATER
  // crossing is reported on double-event days).
  const auto scan_segment = [&f, &result](const double a, const double b) {
    constexpr double MIN_SEGMENT_DAYS = 1e-6; // ~0.1 s — below this the "segment" is an endpoint.
    if (b - a < MIN_SEGMENT_DAYS) {
      return;
    }
    if (const auto up = detail::crossing_in_bracket(f, a, b, true); up.has_value()) {
      if (up->residual_deg > RISE_SET_RESIDUAL_GUARD_DEG) [[unlikely]] {
        throw std::runtime_error {
          std::format("calculate_day: bracketed rise root failed to converge: |altitude - h0| = {} deg "
                      "at jde {} (bracket [{}, {}], guard {} deg)",
                      up->residual_deg, up->root_jde, a, b, RISE_SET_RESIDUAL_GUARD_DEG)
        };
      }
      result.rise_jde = up->root_jde;
    }
    if (const auto down = detail::crossing_in_bracket(f, a, b, false); down.has_value()) {
      if (down->residual_deg > RISE_SET_RESIDUAL_GUARD_DEG) [[unlikely]] {
        throw std::runtime_error {
          std::format("calculate_day: bracketed set root failed to converge: |altitude - h0| = {} deg "
                      "at jde {} (bracket [{}, {}], guard {} deg)",
                      down->residual_deg, down->root_jde, a, b, RISE_SET_RESIDUAL_GUARD_DEG)
        };
      }
      result.set_jde = down->root_jde;
    }
  };

  double seg_lo = t0;
  for (const auto& extremum : extrema) {
    scan_segment(seg_lo, extremum.jde);
    seg_lo = extremum.jde;
  }
  scan_segment(seg_lo, t1);

  if (not result.rise_jde.has_value() and not result.set_jde.has_value()) {
    // No crossing found. The day's global extremes — over every interior extremum AND the
    // window ends — decide the topology: entirely above → DAY, entirely below → NIGHT.
    // A straddle without a found crossing is an engine failure: on monotone segments the
    // directed checks above are exact, so reaching that state means the monotone-partition
    // premise broke; say so loudly.
    double h_min = h(t0);
    double h_max = h(t0);
    for (const auto& extremum : extrema) {
      h_min = std::min(h_min, extremum.altitude_deg);
      h_max = std::max(h_max, extremum.altitude_deg);
    }
    h_min = std::min(h_min, h(t1));
    h_max = std::max(h_max, h(t1));

    if (h_min >= h0.deg()) {
      result.polar = Polar::DAY;
    } else if (h_max <= h0.deg()) {
      result.polar = Polar::NIGHT;
    } else {
      throw std::runtime_error {
        std::format("calculate_day: altitude straddles h0 (min {} < {} < max {}) but no crossing was "
                    "found in [{}, {}] — the monotone-partition premise no longer holds for this body",
                    h_min, h0.deg(), h_max, t0, t1)
      };
    }
  }

  return result;
}


/**
 * @namespace astro::rise_set::sun
 * @brief Solar conventions and entry points: standard altitudes, twilight constants, and the
 *        transit-centered API wired to the Sun's ephemeris.
 */
namespace sun {

/**
 * @brief The standard altitude of the Sun's center at rise/set: -0°50' = -0.8333…°.
 * @note Meeus Chapter 15: -34' of standard atmospheric refraction at the horizon,
 *       plus -16' so that the event refers to the Sun's upper limb, not its center.
 */
inline constexpr auto STANDARD_ALTITUDE = astro::toolbox::AngleDeg::from_arcmin(-50.0);

/**
 * @brief Convert atmospheric conditions to the sunrise/sunset altitude convention.
 * @param p The atmospheric refraction parameters. Defaults to 15°C/1013.25 hPa/Bennett.
 * @return The geometric altitude of the Sun's center that makes the Sun's upper limb appear at
 *         the horizon: `−(horizon refraction + 16′)`.
 * @throw std::invalid_argument If `p` contains non-finite, non-positive pressure, or temperature
 *        at or below −273°C. For `Model::SAEMUNDSSON`, may throw `std::runtime_error` if the
 *         horizon iteration does not converge.
 * @note The default parameters reproduce `STANDARD_ALTITUDE` to within 0.02′.
 */
[[nodiscard]] inline auto h0_from(const astro::earth::refraction::Params& p = {}) -> astro::toolbox::AngleDeg {
  const auto horizon_refraction = astro::earth::refraction::at_horizon(p);
  const auto upper_limb = astro::toolbox::AngleDeg::from_arcmin(16.0);
  return -(horizon_refraction + upper_limb);
}

/** @brief The Sun's altitude at civil twilight: -6°. */
inline constexpr astro::toolbox::AngleDeg CIVIL_TWILIGHT { -6.0 };

/** @brief The Sun's altitude at nautical twilight: -12°. */
inline constexpr astro::toolbox::AngleDeg NAUTICAL_TWILIGHT { -12.0 };

/** @brief The Sun's altitude at astronomical twilight: -18°. */
inline constexpr astro::toolbox::AngleDeg ASTRONOMICAL_TWILIGHT { -18.0 };

/** @brief The Sun's position provider: apparent equatorial coordinates at one instant (TT). */
inline constexpr auto provider = &astro::sun::equatorial_coord::apparent;

/**
 * @brief Compute the instant of the Sun's upper culmination (solar transit / solar noon) on a date.
 * @param ymd The date, interpreted on the **UT1** scale (not local civil time — callers handle
 *        time zones). The returned transit is the one nearest 12h local mean time on this date.
 * @param location The observer's location.
 * @return The transit instant, as a julian ephemeris day on the **TT** scale.
 * @throw std::invalid_argument If `ymd` is invalid or `location` is out of range.
 * @throw std::runtime_error For chronologically valid but unsupported dates (the UT1/JD
 *        conversions reject dates outside the representable years with `std::runtime_error`).
 * @note The estimate is local mean noon; the bracket around it is one the equation of time can
 *       never escape (see `TRANSIT_BRACKET_HALF_WIDTH_DAYS`).
 */
[[nodiscard]] inline auto transit_jde(
  const std::chrono::year_month_day& ymd,
  const GeoLocation& location
) -> double {
  // Local mean noon in UT1: 12h UT minus the east-positive longitude's worth of a day.
  // The offset is applied in JDE arithmetic (not in the Datetime fraction) so longitudes near
  // ±180° cannot push the fraction outside [0, 1).
  const calendar::Datetime noon_ut1 { ymd, 0.5 };
  const double estimate = astro::julian_day::ut1_to_jde(noon_ut1) - (location.longitude.deg() / 360.0);
  return detail::polish_transit(estimate, location, provider);
}

/**
 * @brief Compute the instant at which the Sun crosses altitude h₀, before (sunrise) or after
 *        (sunset) a given transit.
 * @param transit The transit instant, as a julian ephemeris day on the **TT** scale
 *        (from `transit_jde`).
 * @param is_rise True for the upward crossing before transit, false for the downward
 *        crossing after it.
 * @param location The observer's location.
 * @param h0 The crossing altitude. Defaults to `STANDARD_ALTITUDE`; pass a twilight constant
 *        for dawn/dusk instants.
 * @return The crossing instant (JDE, TT scale), or `nullopt` on polar day/night.
 * @throw std::invalid_argument / std::runtime_error See the generic `rise_set_jde`.
 */
[[nodiscard]] inline auto rise_set_jde(
  const double transit,
  const bool is_rise,
  const GeoLocation& location,
  const astro::toolbox::AngleDeg& h0 = STANDARD_ALTITUDE
) -> std::optional<double> {
  return astro::rise_set::rise_set_jde(transit, is_rise, location, h0, provider);
}

/**
 * @brief Compute sunrise, transit, and sunset for a date and location.
 * @param ymd The date, interpreted on the **UT1** scale (callers handle time zones).
 * @param location The observer's location.
 * @param h0 The event altitude. Defaults to `STANDARD_ALTITUDE`; pass a twilight constant to
 *        compute dawn/dusk instead.
 * @return The three instants (JDE, TT scale) and the polar topology; see `Result`'s notes
 *         for the exact semantics.
 * @throw std::invalid_argument If `ymd` is invalid, `location` is out of range, or `h0` is
 *        not finite or outside [-90°, 90°].
 * @throw std::runtime_error For dates outside the representable years (see `transit_jde`) or a
 *        residual-guard failure inside `rise_set_jde`.
 * @note Consumer trap (deliberate semantics): because `ymd` is a UT1 date, for eastern
 *       longitudes the returned sunrise can fall on the *previous* UT1 calendar day (e.g.
 *       Beijing's sunrise is ~21-22h UT of `ymd - 1`); callers building a local calendar day
 *       must convert with their time zone, not assume all three instants share `ymd`.
 */
[[nodiscard]] inline auto calculate(
  const std::chrono::year_month_day& ymd,
  const GeoLocation& location,
  const astro::toolbox::AngleDeg& h0 = STANDARD_ALTITUDE
) -> Result {
  const double transit = transit_jde(ymd, location);
  return calculate_around_transit(transit, location, h0, provider);
}

} // namespace astro::rise_set::sun


/**
 * @namespace astro::rise_set::moon
 * @brief Lunar conventions and entry points: the Ch.15 standard altitude and the
 *        UT-day (almanac-style) API wired to the ELP2000-82B ephemeris.
 */
namespace moon {

/**
 * @brief The Moon's position provider: apparent geocentric equatorial coordinates at one
 *        instant (TT) — an alias of `astro::moon::equatorial_coord::apparent`, the lunar
 *        counterpart of the solar provider.
 */
inline constexpr auto apparent_equatorial = &astro::moon::equatorial_coord::apparent;

/**
 * @brief The Moon's equatorial horizontal parallax Π at one instant.
 * @note Geocentric distance from the ELP2000-82B ephemeris; see
 *       `moon::geocentric_coord::equatorial_horizontal_parallax`.
 */
[[nodiscard]] inline auto horizontal_parallax(const double jde_tt) -> astro::toolbox::AngleRad {
  const auto ecl = astro::moon::geocentric_coord::apparent(jde_tt);
  return astro::moon::geocentric_coord::equatorial_horizontal_parallax(
    astro::toolbox::DistanceKm { ecl.r.km() }
  );
}

/**
 * @brief The standard altitude of the Moon's center at rise/set: `0.7275·Π − refraction`.
 * @param Π The equatorial horizontal parallax at (approximately) the event time. A per-day
 *        value is sufficient: Π's intra-day drift moves rise/set instants by seconds at most,
 *        invisible to the minute-level golden contract (enforced end-to-end by the golden
 *        tests — R1/R2 实录:精确上界数字住进注释会被逐轮更密的采样证伪,故此处只留定性句).
 * @param p The atmospheric refraction parameters. Defaults reproduce Meeus's 34′ term.
 * @return The geometric altitude of the Moon's center that makes its upper limb appear at the
 *         horizon. The 0.7275 factor folds the parallax (and the upper-limb semidiameter) into
 *         the horizon dip, so **geocentric** coordinates can be used directly — no topocentric
 *         reduction (Ch.40) is needed at minute-level accuracy.
 * @throw std::invalid_argument If `p` is invalid (see `refraction::at_horizon`).
 * @throw std::runtime_error For `Model::SAEMUNDSSON`, if the horizon iteration does not
 *        converge (see `refraction::at_horizon`).
 * @ref Jean Meeus, "Astronomical Algorithms", Second Edition, Chapter 15.
 */
[[nodiscard]] inline auto h0(
  const astro::toolbox::AngleRad& Π,
  const astro::earth::refraction::Params& p = {}
) -> astro::toolbox::AngleDeg {
  const auto parallax_term = astro::toolbox::AngleDeg { 0.7275 * Π.deg() };
  return parallax_term - astro::earth::refraction::at_horizon(p);
}

/**
 * @brief Compute moonrise, lunar transit, and moonset for one UT1 calendar day.
 * @param ymd The date, interpreted on the **UT1** scale (callers handle time zones).
 * @param location The observer's location.
 * @param p The atmospheric refraction parameters for the h₀ convention (defaults: standard 34′).
 * @return The events inside the UT day and the polar topology. Any instant may be absent —
 *         with lunar transits ~24.84 h apart, a UT date without a moonrise (or without a
 *         transit) is routine, and that absence must not be read as a polar verdict; check
 *         `polar` for the topology. On double-event days (the Moon can rise or set twice in
 *         one UT day at high latitudes near a standstill) the LATER event is reported; see
 *         `calculate_day`'s note.
 * @throw std::invalid_argument If `ymd` is invalid, `location` is out of range, or `p` is
 *        invalid (see `refraction::at_horizon`).
 * @throw std::runtime_error For chronologically valid but unsupported dates (the UT1/JD
 *        conversions reject dates outside the representable years), for a refraction
 *        failure at the h₀ stage (see `h0`'s @throw — e.g. SAEMUNDSSON non-convergence),
 *        or numerical failures inside `calculate_day` (see its @throw).
 * @note h₀ is evaluated with Π taken at mid-day (see `h0`'s note for the error budget).
 * @note The window is the UT1 calendar day, matching almanac (e.g. USNO rstt/oneday at tz=0)
 *       cell semantics — unlike the solar API, which is transit-centered.
 */
[[nodiscard]] inline auto calculate(
  const std::chrono::year_month_day& ymd,
  const GeoLocation& location,
  const astro::earth::refraction::Params& p = {}
) -> Result {
  const calendar::Datetime day_start { ymd, 0.0 };
  const double midday = astro::julian_day::ut1_to_jde(day_start) + 0.5;
  const auto h0_moon = h0(horizontal_parallax(midday), p);
  return calculate_day(ymd, location, h0_moon, apparent_equatorial);
}

} // namespace astro::rise_set::moon

} // namespace astro::rise_set
