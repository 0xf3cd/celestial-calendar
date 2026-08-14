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

#include <algorithm>
#include <chrono>
#include <cmath>
#include <format>
#include <functional>
#include <limits>
#include <numbers>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "util.hpp"
#include "rise_set.hpp"
#include "rise_set_test_helper.hpp"

namespace astro::rise_set::test {

using namespace astro::rise_set;
using astro::toolbox::AngleDeg;

namespace {

// East-positive longitudes, per `GeoLocation`'s convention.
const GeoLocation EQUATOR   = loc(  0.0,      0.0);
const GeoLocation LONDON    = loc( 51.5074,  -0.1278);
const GeoLocation NEW_YORK  = loc( 40.7128, -74.0060);
const GeoLocation BEIJING   = loc( 39.9042, 116.4074);
const GeoLocation SYDNEY    = loc(-33.8688, 151.2093);
const GeoLocation TROMSO    = loc( 69.65,    18.96);
const GeoLocation MIDLAT_0E = loc( 40.0,      0.0);

/** @brief The estimate `sun::transit_jde` starts from: local mean noon of the UT1 date, as a JDE. */
auto local_mean_noon_jde(const std::chrono::year_month_day& ymd, const GeoLocation& location) -> double {
  const calendar::Datetime noon_ut1 { ymd, 0.5 };
  return astro::julian_day::ut1_to_jde(noon_ut1) - (location.longitude.deg() / 360.0);
}

} // anonymous namespace


TEST(RiseSet, HourAngleAtAltitudeMatchesHorizontalAltitude) {
  // Meeus (15.1) inverts (13.6); feeding H₀ back through `equatorial_to_horizontal` must
  // reproduce h₀ (and at -H₀ too, by the symmetry of the diurnal arc).
  const std::vector<double> declinations { -23.44, -10.0, 0.0, 10.0, 23.44 };
  const std::vector<double> latitudes    { -55.0, -30.0, 0.0, 30.0, 51.5, 65.0 };
  const std::vector<double> altitudes    { -18.0, -12.0, -6.0, -0.8333, 0.0 };

  for (const double δ : declinations) {
    for (const double φ : latitudes) {
      for (const double h0 : altitudes) {
        const auto H0 = hour_angle_at_altitude(AngleDeg { δ }, AngleDeg { φ }, AngleDeg { h0 });
        if (not H0.has_value()) {
          continue; // Polar cases are covered by their own test.
        }

        for (const double sign : { 1.0, -1.0 }) {
          const auto horizontal = astro::coords::equatorial_to_horizontal(
            req(H0) * sign, AngleDeg { δ }, AngleDeg { φ }
          );
          ASSERT_NEAR(horizontal.h.deg(), h0, 1e-9)
            << "δ=" << δ << " φ=" << φ << " h0=" << h0 << " sign=" << sign;
        }
      }
    }
  }
}

TEST(RiseSet, HourAngleAtAltitudePolarAndDegenerateCases) {
  const AngleDeg h0_standard { -0.8333 };

  // Midnight sun at 80°N in northern summer: the Sun's minimum altitude is ~+13.4°, far above h₀.
  ASSERT_FALSE(hour_angle_at_altitude(AngleDeg { 23.44 }, AngleDeg { 80.0 }, h0_standard).has_value());

  // Polar night at 80°N in northern winter: the maximum altitude is ~-13.4°, below h₀.
  ASSERT_FALSE(hour_angle_at_altitude(AngleDeg { -23.44 }, AngleDeg { 80.0 }, h0_standard).has_value());

  // The equation degenerates at the geographic pole (cos φ = 0).
  ASSERT_FALSE(hour_angle_at_altitude(AngleDeg { 10.0 }, AngleDeg { 90.0 }, h0_standard).has_value());

  // Equator, equinoctial Sun, geometric horizon: the diurnal arc is exactly a half turn.
  const auto H0 = hour_angle_at_altitude(AngleDeg { 0.0 }, AngleDeg { 0.0 }, AngleDeg { 0.0 });
  ASSERT_TRUE(H0.has_value());
  ASSERT_NEAR(req(H0).deg(), 90.0, 1e-9);
}

TEST(RiseSet, AltitudeConstantsProvenance) {
  // Pin the externally-meaningful constants to their literature values (Meeus Ch.15's
  // -34' refraction - 16' upper limb; the standard civil/nautical/astronomical twilight
  // definitions). The solver tests are parametric in h₀ and would stay green with a silently
  // wrong constant — e.g. even h₀ = 0° keeps EquatorDayLength inside its ±30 min (per review).
  ASSERT_NEAR(sun::STANDARD_ALTITUDE.deg(), -50.0 / 60.0, 1e-12);
  ASSERT_NEAR(sun::CIVIL_TWILIGHT.deg(), -6.0, 1e-12);
  ASSERT_NEAR(sun::NAUTICAL_TWILIGHT.deg(), -12.0, 1e-12);
  ASSERT_NEAR(sun::ASTRONOMICAL_TWILIGHT.deg(), -18.0, 1e-12);
}

TEST(RiseSet, TransitHourAngleIsZero) {
  const std::vector<std::pair<std::chrono::year_month_day, GeoLocation>> cases {
    { util::to_ymd(2024, 6, 21), NEW_YORK },
    { util::to_ymd(2024, 6, 21), BEIJING  },
    { util::to_ymd(2024, 3, 20), LONDON   },
    { util::to_ymd(2024, 12, 21), SYDNEY  },
  };

  for (const auto& [ymd, location] : cases) {
    const double transit = sun::transit_jde(ymd, location);
    const double H = detail::body_local(transit, location, sun::provider).hour_angle_deg;
    // 0.001° of hour angle ≈ 0.24 s of time. Self-consistency only: solver and checker share
    // `detail::body_local`, so a systematic error (e.g. a UT1/TT mixup) cancels out here — the
    // Meeus 28.a anchor below is the absolute guard (mutation-verified).
    ASSERT_NEAR(H, 0.0, 0.001);
  }
}

TEST(RiseSet, TransitNearLocalMeanNoon) {
  // The transit deviates from local mean noon by the equation of time only (|EoT| ≤ 16.5 min
  // ≈ 0.0115 day). A sign flip in `body_local`'s longitude negation moves the H = 0 root ~0.65
  // day off Beijing's bracket — 32x this tolerance — so this pins that negation. (It does NOT
  // pin the estimate formula itself: the `local_mean_noon_jde` helper mirrors it by construction.)
  const std::vector<std::pair<std::chrono::year_month_day, GeoLocation>> cases {
    { util::to_ymd(2024, 2, 11), BEIJING  }, // Near the EoT minimum (~-14.2 min).
    { util::to_ymd(2024, 11, 3), NEW_YORK }, // Near the EoT maximum (~+16.5 min).
    { util::to_ymd(2024, 6, 21), TROMSO   }, // Transit exists even on a polar day.
  };

  for (const auto& [ymd, location] : cases) {
    const double transit = sun::transit_jde(ymd, location);
    ASSERT_NEAR(transit, local_mean_noon_jde(ymd, location), 0.02);
  }
}

TEST(RiseSet, TransitAnchoredToMeeusEquationOfTime) {
  // Meeus Example 28.a: 1992 October 13, 0h TD — E = +3°.427351 = +13.70940 min. E > 0 means
  // the true Sun crosses the meridian before the mean sun, so at Greenwich the transit falls
  // at ≈ 12h − E of mean time (= UT). This is the one assertion in this file tied to a value
  // from OUTSIDE the computation chain: the self-consistency tests (H(transit) ≈ 0 etc.) run
  // solver and checker through the same `detail::body_local`, so a systematic error — e.g.
  // feeding TT to the sidereal-time step — cancels out of them, but not out of this one.
  // Tolerance ±20 s: E drifts only a few seconds between 0h TD and the ~11h46m transit, and
  // the model differences are ~1 s; a UT1/TT mixup would shift the transit by ΔT ≈ 59 s
  // (epoch 1992), i.e. ~3x this tolerance.
  const auto ymd = util::to_ymd(1992, 10, 13);
  const double transit = sun::transit_jde(ymd, loc(51.4769, 0.0)); // Greenwich; latitude does not move the transit.

  const auto ut1 = astro::julian_day::jde_to_ut1(transit);
  ASSERT_EQ(ut1.ymd, ymd);

  const double expected_fraction = 0.5 - (13.70940 / (24.0 * 60.0));
  ASSERT_NEAR(ut1.fraction(), expected_fraction, 20.0 / 86400.0);
}

TEST(RiseSet, TransitIsUpperCulmination) {
  const auto ymd = util::to_ymd(2024, 9, 22);
  const double transit = sun::transit_jde(ymd, LONDON);

  const double h_transit = detail::altitude(transit, LONDON, sun::provider).deg();
  ASSERT_GT(h_transit, detail::altitude(transit - (2.0 / 24.0), LONDON, sun::provider).deg());
  ASSERT_GT(h_transit, detail::altitude(transit + (2.0 / 24.0), LONDON, sun::provider).deg());
}

TEST(RiseSet, RiseSetAltitudeIsH0) {
  const std::vector<std::tuple<std::chrono::year_month_day, GeoLocation, AngleDeg>> cases {
    { util::to_ymd(2024, 3, 20), LONDON,  sun::STANDARD_ALTITUDE },
    { util::to_ymd(2024, 6, 21), BEIJING, sun::STANDARD_ALTITUDE },
    { util::to_ymd(2024, 12, 21), SYDNEY, sun::STANDARD_ALTITUDE },
    { util::to_ymd(2024, 3, 20), LONDON,  sun::CIVIL_TWILIGHT    },
    { util::to_ymd(2024, 3, 20), LONDON,  sun::ASTRONOMICAL_TWILIGHT },
  };

  for (const auto& [ymd, location, h0] : cases) {
    const auto result = sun::calculate(ymd, location, h0);
    ASSERT_TRUE(result.rise_jde.has_value());
    ASSERT_TRUE(result.set_jde.has_value());

    // 0.001° ≈ 0.36". At typical crossing rates (~200°/day) that is ~0.4 s of time.
    ASSERT_NEAR(detail::altitude(req(result.rise_jde), location, sun::provider).deg(), h0.deg(), 0.001);
    ASSERT_NEAR(detail::altitude(req(result.set_jde), location, sun::provider).deg(), h0.deg(), 0.001);
  }
}

TEST(RiseSet, OrderIsCorrect) {
  const std::vector<std::pair<std::chrono::year_month_day, GeoLocation>> cases {
    { util::to_ymd(2024, 6, 21), BEIJING },
    { util::to_ymd(2024, 12, 21), BEIJING },
    { util::to_ymd(2024, 6, 21), SYDNEY },
    { util::to_ymd(2024, 9, 22), EQUATOR },
  };

  for (const auto& [ymd, location] : cases) {
    const auto result = sun::calculate(ymd, location);
    ASSERT_TRUE(result.rise_jde.has_value());
    ASSERT_TRUE(result.set_jde.has_value());
    ASSERT_LT(req(result.rise_jde), req(result.transit_jde));
    ASSERT_LT(req(result.transit_jde), req(result.set_jde));
    ASSERT_EQ(result.polar, Polar::NONE);
  }
}

TEST(RiseSet, TwilightOrder) {
  // At 40°N the midsummer Sun still dips to ~-26.6°, so all three twilights exist.
  const auto ymd = util::to_ymd(2024, 6, 21);

  const auto r_standard = sun::calculate(ymd, MIDLAT_0E, sun::STANDARD_ALTITUDE);
  const auto r_civil    = sun::calculate(ymd, MIDLAT_0E, sun::CIVIL_TWILIGHT);
  const auto r_nautical = sun::calculate(ymd, MIDLAT_0E, sun::NAUTICAL_TWILIGHT);
  const auto r_astro    = sun::calculate(ymd, MIDLAT_0E, sun::ASTRONOMICAL_TWILIGHT);

  for (const auto& r : { r_standard, r_civil, r_nautical, r_astro }) {
    ASSERT_TRUE(r.rise_jde.has_value());
    ASSERT_TRUE(r.set_jde.has_value());
  }

  // Morning: astronomical dawn < nautical dawn < civil dawn < sunrise.
  ASSERT_LT(req(r_astro.rise_jde), req(r_nautical.rise_jde));
  ASSERT_LT(req(r_nautical.rise_jde), req(r_civil.rise_jde));
  ASSERT_LT(req(r_civil.rise_jde), req(r_standard.rise_jde));

  // Evening: sunset < civil dusk < nautical dusk < astronomical dusk.
  ASSERT_LT(req(r_standard.set_jde), req(r_civil.set_jde));
  ASSERT_LT(req(r_civil.set_jde), req(r_nautical.set_jde));
  ASSERT_LT(req(r_nautical.set_jde), req(r_astro.set_jde));
}

TEST(RiseSet, EquatorDayLength) {
  // At the equator the day is close to 12h all year; h₀ = -50' stretches it by a few minutes.
  for (int month = 1; month <= 12; ++month) {
    const auto ymd = util::to_ymd(2024, month, 15);
    const auto result = sun::calculate(ymd, EQUATOR);

    ASSERT_TRUE(result.rise_jde.has_value());
    ASSERT_TRUE(result.set_jde.has_value());

    const double day_length = req(result.set_jde) - req(result.rise_jde);
    ASSERT_NEAR(day_length, 0.5, 30.0 / (24.0 * 60.0)) << "month=" << month; // 12h ± 30 min.
  }
}

TEST(RiseSet, RiseSetSymmetryAroundTransit) {
  // δ drifts slowly, so morning and afternoon half-arcs agree to well under a minute — but
  // assert only a loose bound to keep the test about symmetry, not about δ's exact rate.
  const auto result = sun::calculate(util::to_ymd(2024, 9, 22), EQUATOR);
  ASSERT_TRUE(result.rise_jde.has_value());
  ASSERT_TRUE(result.set_jde.has_value());

  const double morning = req(result.transit_jde) - req(result.rise_jde);
  const double evening = req(result.set_jde) - req(result.transit_jde);
  ASSERT_NEAR(morning, evening, 0.01);
}

TEST(RiseSet, PolarDayAndPolarNight) {
  // Tromsø (69.65°N) is inside the Arctic Circle: midnight sun in June, polar night in December.
  const auto summer = sun::calculate(util::to_ymd(2024, 6, 21), TROMSO);
  ASSERT_FALSE(summer.rise_jde.has_value());
  ASSERT_FALSE(summer.set_jde.has_value());
  ASSERT_EQ(summer.polar, Polar::DAY);

  const auto winter = sun::calculate(util::to_ymd(2024, 12, 21), TROMSO);
  ASSERT_FALSE(winter.rise_jde.has_value());
  ASSERT_FALSE(winter.set_jde.has_value());
  ASSERT_EQ(winter.polar, Polar::NIGHT);

  // In the December polar night the Sun still culminates at ~-3.1°, above the civil-twilight
  // altitude — so civil dawn/dusk exist even though sunrise/sunset do not.
  const auto winter_civil = sun::calculate(util::to_ymd(2024, 12, 21), TROMSO, sun::CIVIL_TWILIGHT);
  ASSERT_TRUE(winter_civil.rise_jde.has_value());
  ASSERT_TRUE(winter_civil.set_jde.has_value());
  ASSERT_EQ(winter_civil.polar, Polar::NONE);
}

TEST(RiseSet, PolarNightOnsetWeekIsCoherent) {
  // Tromsø's polar night (h₀ = -50') begins in the last days of November. Scan the onset
  // window: every day must be exactly one of {both events; one event + no flags; no events +
  // the night flag}, and once polar night starts it must persist through the window. This
  // exercises the fallback bracket (transit → true altitude minimum) in the regime the
  // review flagged: short nights centered near the lower culmination.
  bool seen_polar_night = false;

  for (int day = 20; day <= 30; ++day) {
    const auto result = sun::calculate(util::to_ymd(2024, 11, day), TROMSO);
    const int events = static_cast<int>(result.rise_jde.has_value())
                     + static_cast<int>(result.set_jde.has_value());

    if (events == 0) {
      ASSERT_EQ(result.polar, Polar::NIGHT) << "day=" << day; // November at 69.65°N: night, never day.
      seen_polar_night = true;
    } else {
      ASSERT_EQ(result.polar, Polar::NONE) << "day=" << day;
      ASSERT_FALSE(seen_polar_night) << "day=" << day; // No coming back out of the night in this window.
      if (events == 2) {
        ASSERT_LT(req(result.rise_jde), req(result.transit_jde));
        ASSERT_LT(req(result.transit_jde), req(result.set_jde));
      }
    }
  }

  ASSERT_TRUE(seen_polar_night); // The onset falls inside the scanned window.
}

TEST(RiseSet, MidnightSunOnsetWeekIsCoherent) {
  // The mirror window: Tromsø's midnight sun begins in mid-May. This is the only regime where
  // one-sided days (a rise or set alone, both flags false) can occur — the nights collapse one
  // side at a time around the lower culminations — so the scan exercises that branch when the
  // year's alignment produces one, and the polar-day onset in any case.
  bool seen_polar_day = false;

  for (int day = 12; day <= 24; ++day) {
    const auto result = sun::calculate(util::to_ymd(2024, 5, day), TROMSO);
    const int events = static_cast<int>(result.rise_jde.has_value())
                     + static_cast<int>(result.set_jde.has_value());

    if (events == 0) {
      ASSERT_EQ(result.polar, Polar::DAY) << "day=" << day; // May at 69.65°N: day, never night.
      seen_polar_day = true;
    } else {
      ASSERT_EQ(result.polar, Polar::NONE) << "day=" << day;
      if (events == 2) {
        ASSERT_LT(req(result.rise_jde), req(result.transit_jde));
        ASSERT_LT(req(result.transit_jde), req(result.set_jde));
      }
    }
  }

  ASSERT_TRUE(seen_polar_day); // The onset falls inside the scanned window.
}

TEST(RiseSet, GrazeAtTransitIsPolarNightNotDay) {
  // Synthetic fixed body (α = 0°, δ = +10°): at 70°N its transit altitude is exactly
  // 90° − (70° − 10°) = 30°. With h0 set to that value the day merely *touches* h0 at its
  // highest point — every other instant stays below: a polar NIGHT, not a day. (R1: a `>=`
  // here once counted the graze as DAY, diverging from `calculate_day`'s h_max <= h0 → NIGHT;
  // real ephemerides never hit exact equality, so only a synthetic body can pin it.)
  const auto fixed_body = [](const double) -> astro::coords::EquatorialCoord {
    return { .α = AngleDeg { 0.0 }, .δ = AngleDeg { 10.0 } };
  };
  const GeoLocation site = loc(70.0, 0.0);
  const auto ymd = util::to_ymd(2024, 6, 21);
  // A fixed-α body's transit can fall anywhere in the day (local mean noon is a solar-only
  // estimate), so find it with the window mechanism instead.
  const double day_start = astro::julian_day::ut1_to_jde(calendar::Datetime { ymd, 0.0 });
  const double transit = req(transit_in_window(day_start, day_start + 1.0, site, fixed_body));
  const double h_transit = detail::altitude(transit, site, fixed_body).deg();
  ASSERT_NEAR(h_transit, 30.0, 1e-9);

  const auto result = calculate_around_transit(transit, site, AngleDeg { h_transit }, fixed_body);
  ASSERT_FALSE(result.rise_jde.has_value());
  ASSERT_FALSE(result.set_jde.has_value());
  ASSERT_EQ(result.polar, Polar::NIGHT);
}

TEST(RiseSet, FindExtremaFindsEveryInteriorExtremum) {
  // cos(2πt) + 0.5·cos(4πt) has exactly three interior extrema on [0, 1]: minima at t = 1/3
  // and 2/3 (value −0.75) and a maximum at t = 1/2 (value −0.5). Pins the grid-scan +
  // refinement machinery directly (the R2 测试缝隙席 noted it was only covered end-to-end).
  const auto f = [](const double t) {
    return std::cos(2.0 * std::numbers::pi * t) + (0.5 * std::cos(4.0 * std::numbers::pi * t));
  };

  const auto extrema = detail::find_extrema(f, 0.0, 1.0);
  ASSERT_EQ(extrema.size(), 3UZ);

  ASSERT_TRUE(extrema[0].is_minimum);
  ASSERT_NEAR(extrema[0].jde, 1.0 / 3.0, 1e-6);
  ASSERT_NEAR(extrema[0].altitude_deg, -0.75, 1e-9);

  ASSERT_FALSE(extrema[1].is_minimum);
  ASSERT_NEAR(extrema[1].jde, 0.5, 1e-6);
  ASSERT_NEAR(extrema[1].altitude_deg, -0.5, 1e-9);

  ASSERT_TRUE(extrema[2].is_minimum);
  ASSERT_NEAR(extrema[2].jde, 2.0 / 3.0, 1e-6);
  ASSERT_NEAR(extrema[2].altitude_deg, -0.75, 1e-9);
}

TEST(RiseSet, TransitResidualGuardThrowsForRootlessBracket) {
  // A body whose α sweeps forward at 761°/day — faster than sidereal time, so H runs
  // backwards and the forward-angle estimate points the wrong way; the polish bracket holds
  // no root. The residual guard must throw rather than ship a fake transit (shape first
  // proven by the R2 probe; fixed inputs make it deterministic).
  const double jde0 = astro::julian_day::ut1_to_jde(
    calendar::Datetime { util::to_ymd(2026, 8, 15), 0.0 });
  const auto retrograde = [jde0](const double jde) -> astro::coords::EquatorialCoord {
    return { .α = AngleDeg { 761.0 * (jde - jde0) }.normalize(), .δ = AngleDeg { 5.0 } };
  };

  ASSERT_THROW(std::ignore = transit_in_window(jde0, jde0 + 1.0, EQUATOR, retrograde),
               std::runtime_error);
}

TEST(RiseSet, RiseSetResidualGuardThrowsOnDiscontinuousProvider) {
  // δ cliff at 80°N: +80° (circumpolar, always above h0 = 0) until 16:48, then −80° (always
  // below). No smooth crossing exists anywhere — f straddles only across the discontinuity,
  // so the bracketed solve can only return a best-effort iterate, and the residual guard
  // must throw instead of letting the day be misread as eventless/polar.
  const GeoLocation POLE80 = loc(80.0, 0.0);
  const auto cliff = [](const double jde) -> astro::coords::EquatorialCoord {
    const double frac = jde - std::floor(jde);
    return { .α = AngleDeg { 0.0 }, .δ = AngleDeg { frac < 0.7 ? 80.0 : -80.0 } };
  };

  ASSERT_THROW(std::ignore = calculate_day(util::to_ymd(2026, 8, 15), POLE80, AngleDeg { 0.0 }, cliff),
               std::runtime_error);
}

TEST(RiseSet, InvalidInputsThrow) {
  const auto ymd = util::to_ymd(2024, 6, 21);
  constexpr double NAN_D = std::numeric_limits<double>::quiet_NaN();

  ASSERT_THROW(std::ignore = sun::transit_jde(ymd, loc(90.5, 0.0)), std::invalid_argument);
  ASSERT_THROW(std::ignore = sun::transit_jde(ymd, loc(0.0, 180.5)), std::invalid_argument);
  ASSERT_THROW(std::ignore = sun::transit_jde(ymd, loc(NAN_D, 0.0)), std::invalid_argument);
  ASSERT_THROW(std::ignore = sun::transit_jde(ymd, loc(0.0, NAN_D)), std::invalid_argument);

  // An invalid gregorian date is rejected by `Datetime`'s constructor.
  ASSERT_THROW(std::ignore = sun::transit_jde(util::to_ymd(2024, 2, 30), EQUATOR), std::invalid_argument);

  const double transit = sun::transit_jde(ymd, EQUATOR);
  ASSERT_THROW(std::ignore = sun::rise_set_jde(NAN_D, true, EQUATOR), std::invalid_argument);
  ASSERT_THROW(std::ignore = sun::rise_set_jde(transit, true, EQUATOR, AngleDeg { NAN_D }), std::invalid_argument);
  ASSERT_THROW(std::ignore = sun::rise_set_jde(transit, true, EQUATOR, AngleDeg { 100.0 }), std::invalid_argument);
  ASSERT_THROW(std::ignore = sun::rise_set_jde(transit, true, loc(-91.0, 0.0)), std::invalid_argument);

  // hour_angle_at_altitude is public API too: out-of-domain angles alias through sin/cos into
  // physically meaningless H₀ values, so they are rejected rather than returned (per review).
  ASSERT_THROW(std::ignore = hour_angle_at_altitude(AngleDeg { NAN_D }, AngleDeg { 0.0 }, AngleDeg { 0.0 }),
               std::invalid_argument);
  ASSERT_THROW(std::ignore = hour_angle_at_altitude(AngleDeg { 0.0 }, AngleDeg { 95.0 }, AngleDeg { 0.0 }),
               std::invalid_argument);
  ASSERT_THROW(std::ignore = hour_angle_at_altitude(AngleDeg { 0.0 }, AngleDeg { 0.0 }, AngleDeg { 100.0 }),
               std::invalid_argument);
}


// Each bracket constant in `rise_set.hpp` carries a note arguing why it is wide enough:
// "|EoT| ≤ 16.5 min — a 8.7x margin", and so on. Those arguments were maintained by attention —
// shrink a bracket, widen the supported span, or swap a model, and the number beside it does not
// object. The three tests below sweep the span, measure the deviation each bracket actually has
// to cover, and hold it to what its note claims (#126).

namespace {

// The span the solver chain supports. `jd_to_ut1` rejects below year 401, and 401-01-01's
// adjacent lower culmination reaches back past that floor, so 402 is the first fully usable
// year. The ceiling is the one the rest of the suite already works to (`moon_phase_test.cpp`,
// and `newton_method`'s iteration-count note).
constexpr int FIRST_YEAR = 402;
constexpr int LAST_YEAR  = 9050;

/** @brief What a bracket constant's note claims: the deviation it covers. The margin is the
 *         quotient, so there is no third number to drift out of step with the other two. */
struct BracketClaim {
  std::string_view constant;
  double bracket_days;
  double bound_days;
};

// A bracket may not be merely wide enough. Newton needs the root strictly inside, and the
// deviations below are measurements on a grid, not proofs of the true extreme. This floor is
// also the only half of the check a narrowed bracket cannot fool: shrink the constant and the
// solver's root gets pinned near the bracket edge, so the swept deviation shrinks with it and
// would otherwise stay obediently under its bound.
constexpr double MIN_BRACKET_MARGIN = 2.0;

/** @brief One measured deviation, and the case that produced it. */
struct Sample {
  double deviation_days;
  std::string at;
};

/** @brief The sample that came closest to escaping the bracket. Throws rather than dereferencing
 *         `end()` if a sweep ever silently produces nothing — an empty sweep would otherwise read
 *         as a passing gate. */
auto worst_of(const std::vector<Sample>& samples) -> const Sample& {
  if (samples.empty()) {
    throw std::logic_error { "sweep produced no samples" };
  }
  return *std::ranges::max_element(samples, {}, &Sample::deviation_days);
}

auto date_str(const std::chrono::year_month_day& ymd) -> std::string {
  return std::format("{:04}-{:02}-{:02}", static_cast<int>(ymd.year()),
                     static_cast<unsigned>(ymd.month()), static_cast<unsigned>(ymd.day()));
}

/** @brief Years to sweep: every `stride`th, plus the far end — appended because that is where
 *         |EoT| peaks, and dropping it costs the transit sweep 0.6% of its worst case. The drift
 *         is not monotone: |EoT| sags through the middle of the span before climbing again,
 *         while the lower culmination peaks early in it — neither end predicts the other's.
 *         What the stride buys is blind-spot width, not accuracy — across years these curves are
 *         flat enough that halving it moves the measured worst by ~0.01%. It is sized so that a
 *         peak displaced by a model change still lands on the grid. */
auto sampled_years(const int stride) -> std::vector<int> {
  std::vector<int> years;
  years.reserve(static_cast<std::size_t>((LAST_YEAR - FIRST_YEAR) / stride) + 2);
  for (int year = FIRST_YEAR; year <= LAST_YEAR; year += stride) {
    years.push_back(year);
  }
  if (years.back() != LAST_YEAR) {
    years.push_back(LAST_YEAR);
  }
  return years;
}

// All three deviations are smooth annual curves, so a fortnightly-ish net loses well under a
// percent of the peak — cheaper than a two-pass coarse-then-refine sweep, and the bounds below
// carry more headroom than that. Measured against a daily sweep: within 0.6% on all three.
constexpr std::chrono::days SWEEP_STRIDE { 5 };

/** @brief The days of `year` the sweeps visit. */
auto sampled_days(const int year) -> std::vector<std::chrono::year_month_day> {
  const auto first = std::chrono::sys_days { util::to_ymd(year, 1, 1) };
  const auto past_last = std::chrono::sys_days { util::to_ymd(year + 1, 1, 1) };

  std::vector<std::chrono::year_month_day> days;
  days.reserve(static_cast<std::size_t>((past_last - first) / SWEEP_STRIDE) + 1);
  for (auto day = first; day < past_last; day += SWEEP_STRIDE) {
    days.emplace_back(day);
  }
  return days;
}

/** @brief A day count in whichever of minutes or seconds reads naturally — these three
 *         deviations span three orders of magnitude. */
auto human(const double days) -> std::string {
  const double minutes = days * 24 * 60;
  return minutes >= 1.0 ? std::format("{:.2f} min", minutes) : std::format("{:.2f} s", minutes * 60);
}

/** @brief What to say when a bracket has been narrowed past what its note says it covers. */
auto narrowed_report(const BracketClaim& claim) -> std::string {
  return std::format(
    "\n  {} = {:g} day no longer clears the {:g} day ({}) it must cover by {:g}x."
    "\n  Fix: widen the constant back. If the deviation genuinely shrank instead, re-measure and"
    "\n       lower the bound in this test together with the @note on the constant.",
    claim.constant, claim.bracket_days, claim.bound_days, human(claim.bound_days), MIN_BRACKET_MARGIN
  );
}

/** @brief What to say when the deviation a bracket must cover has outgrown its note. */
auto margin_report(const BracketClaim& claim, const Sample& measured) -> std::string {
  return std::format(
    "\n  {} = {:g} day"
    "\n    claimed to cover : deviation ≤ {:g} day ({}) — a {:.1f}x margin"
    "\n    measured worst   : {:.6e} day ({}) at {}"
    "\n    margin left      : {:.1f}x"
    "\n  Fix: if the model or the swept span changed, re-measure and update BOTH the bound in"
    "\n       this test and the @note on {} in src/astro/rise_set.hpp.",
    claim.constant, claim.bracket_days,
    claim.bound_days, human(claim.bound_days), claim.bracket_days / claim.bound_days,
    measured.deviation_days, human(measured.deviation_days), measured.at,
    claim.bracket_days / measured.deviation_days,
    claim.constant
  );
}

// The edge of the domain where the rise/set first-try bracket is meant to be sufficient. Past
// the polar circle the H₀ extrapolation degrades without bound and the fallback takes over, so
// 65° is where a margin claim still means something.
const GeoLocation SUBPOLAR_N = loc( 65.0, 0.0);
const GeoLocation SUBPOLAR_S = loc(-65.0, 0.0);

} // anonymous namespace


// The three gates below fail through `FAIL() <<` rather than `ASSERT_LE(...) <<`. Streaming a
// std::string into gtest's comparison helpers makes gcc 14 emit a false `-Wnull-dereference`
// inside `CmpHelperLE` once it inlines at -O2, and the build takes warnings as errors. Do not
// "simplify" these back: the reports already carry both sides of the comparison.


TEST(RiseSet, TransitBracketCoversTheEquationOfTime) {
  constexpr BracketClaim CLAIM {
    .constant     = "TRANSIT_BRACKET_HALF_WIDTH_DAYS",
    .bracket_days = TRANSIT_BRACKET_HALF_WIDTH_DAYS,
    .bound_days   = 0.0145, // 20.9 min; this sweep peaks at 0.014157 day (20.39 min).
  };
  if (CLAIM.bracket_days < CLAIM.bound_days * MIN_BRACKET_MARGIN) {
    FAIL() << narrowed_report(CLAIM);
  }

  // Longitude is left out of the sweep on purpose: it shifts only the instant at which the
  // equation of time is evaluated, worth ≤ 0.3% here, and `TransitNearLocalMeanNoon` already
  // pins the sign of the longitude term. Latitude does not move a transit at all.
  std::vector<Sample> samples;
  for (const int year : sampled_years(250)) {
    for (const auto& ymd : sampled_days(year)) {
      const double transit = sun::transit_jde(ymd, EQUATOR);
      samples.push_back({
        .deviation_days = std::fabs(transit - local_mean_noon_jde(ymd, EQUATOR)),
        .at = date_str(ymd),
      });
    }
  }

  const auto& measured = worst_of(samples);
  if (measured.deviation_days > CLAIM.bound_days) {
    FAIL() << margin_report(CLAIM, measured);
  }
}

TEST(RiseSet, MinSearchWindowRetainsMargin) {
  constexpr BracketClaim CLAIM {
    .constant     = "MIN_SEARCH_HALF_WIDTH_DAYS",
    .bracket_days = MIN_SEARCH_HALF_WIDTH_DAYS,
    .bound_days   = 1.86e-4, // 16.1 s; this sweep peaks at 1.8518e-4 day (16.00 s). The true
                             // minimum deviates slightly more than the old H = ±180° solve did
                             // (15.23 s) — it also absorbs the dδ/dt shift, which is the point.
  };
  if (CLAIM.bracket_days < CLAIM.bound_days * MIN_BRACKET_MARGIN) {
    FAIL() << narrowed_report(CLAIM);
  }

  // A minimum outside the search window reads as the argmin pinning to the window's edge —
  // deviation ≈ the full half-width — so this gate keeps teeth even though the search is
  // bounded by construction.
  std::vector<Sample> samples;
  for (const int year : sampled_years(250)) {
    for (const auto& ymd : sampled_days(year)) {
      const double transit = sun::transit_jde(ymd, EQUATOR);
      for (const bool before : { true, false }) {
        const double argmin = detail::min_altitude_jde(transit, before, EQUATOR, sun::provider);
        const double estimate = before ? transit - LOWER_CULMINATION_OFFSET_DAYS
                                       : transit + LOWER_CULMINATION_OFFSET_DAYS;
        samples.push_back({
          .deviation_days = std::fabs(argmin - estimate),
          .at = std::format("{} ({})", date_str(ymd), before ? "before transit" : "after transit"),
        });
      }
    }
  }

  const auto& measured = worst_of(samples);
  if (measured.deviation_days > CLAIM.bound_days) {
    FAIL() << margin_report(CLAIM, measured);
  }
}

TEST(RiseSet, RiseSetBracketRetainsMargin) {
  constexpr BracketClaim CLAIM {
    .constant     = "RISE_SET_BRACKET_HALF_WIDTH_DAYS",
    .bracket_days = RISE_SET_BRACKET_HALF_WIDTH_DAYS,
    .bound_days   = 2.6e-3, // 3.74 min; this sweep peaks at 2.4058e-3 day (3.46 min).
  };
  if (CLAIM.bracket_days < CLAIM.bound_days * MIN_BRACKET_MARGIN) {
    FAIL() << narrowed_report(CLAIM);
  }

  // The deviation grows with |φ| (1.1 min at the equator against 3.5 min here), so the domain
  // edge is the binding case; sweeping it densely beats sweeping every latitude thinly.
  std::vector<Sample> samples;
  for (const int year : { FIRST_YEAR, 2026, 5000, LAST_YEAR }) {
    for (const auto& ymd : sampled_days(year)) {
      for (const auto& location : { SUBPOLAR_N, SUBPOLAR_S }) {
        const double transit = sun::transit_jde(ymd, location);
        const auto eq = sun::provider(transit);
        const auto H0 = hour_angle_at_altitude(eq.δ, location.latitude, sun::STANDARD_ALTITUDE);
        if (not H0.has_value()) {
          continue; // Polar day or night: no crossing to bracket.
        }

        for (const bool is_rise : { true, false }) {
          const auto root = sun::rise_set_jde(transit, is_rise, location, sun::STANDARD_ALTITUDE);
          if (not root.has_value()) {
            continue;
          }
          const double sign = is_rise ? -1.0 : 1.0;
          const double estimate =
            transit + (sign * (req(H0).deg() / astro::toolbox::SIDEREAL_RATE_DEG_PER_DAY));
          samples.push_back({
            .deviation_days = std::fabs(req(root) - estimate),
            .at = std::format("{} at latitude {:+.0f}° ({})", date_str(ymd),
                              location.latitude.deg(), is_rise ? "rise" : "set"),
          });
        }
      }
    }
  }

  const auto& measured = worst_of(samples);
  if (measured.deviation_days > CLAIM.bound_days) {
    FAIL() << margin_report(CLAIM, measured);
  }
}

} // namespace astro::rise_set::test
