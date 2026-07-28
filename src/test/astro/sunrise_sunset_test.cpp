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

#include <chrono>
#include <limits>
#include <optional>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "util.hpp"
#include "sunrise_sunset.hpp"

namespace astro::sunrise_sunset::test {

using namespace astro::sunrise_sunset;
using astro::toolbox::Angle;
using astro::toolbox::AngleUnit::DEG;

namespace {

constexpr auto loc(const double lat_deg, const double lon_deg) -> GeoLocation {
  return { .latitude = Angle<DEG> { lat_deg }, .longitude = Angle<DEG> { lon_deg } };
}

// East-positive longitudes, per `GeoLocation`'s convention.
const GeoLocation EQUATOR   = loc(  0.0,      0.0);
const GeoLocation LONDON    = loc( 51.5074,  -0.1278);
const GeoLocation NEW_YORK  = loc( 40.7128, -74.0060);
const GeoLocation BEIJING   = loc( 39.9042, 116.4074);
const GeoLocation SYDNEY    = loc(-33.8688, 151.2093);
const GeoLocation TROMSO    = loc( 69.65,    18.96);
const GeoLocation MIDLAT_0E = loc( 40.0,      0.0);

/** @brief The estimate `transit_jde` starts from: local mean noon of the UT1 date, as a JDE. */
auto local_mean_noon_jde(const std::chrono::year_month_day& ymd, const GeoLocation& location) -> double {
  const calendar::Datetime noon_ut1 { ymd, 0.5 };
  return astro::julian_day::ut1_to_jde(noon_ut1) - location.longitude.deg() / 360.0;
}

/**
 * @brief Checked unwrap for assertions. clang-tidy's bugprone-unchecked-optional-access cannot
 *        see through gtest's ASSERT_TRUE, so tests unwrap through this provably-guarded helper
 *        rather than NOLINT-ing every access.
 */
template <typename T>
auto req(const std::optional<T>& opt) -> const T& {
  if (not opt.has_value()) {
    throw std::logic_error { "expected optional to hold a value" };
  }
  return *opt;
}

} // anonymous namespace


TEST(SunriseSunset, HourAngleAtAltitudeMatchesHorizontalAltitude) {
  // Meeus (15.1) inverts (13.6); feeding H₀ back through `equatorial_to_horizontal` must
  // reproduce h₀ (and at -H₀ too, by the symmetry of the diurnal arc).
  const std::vector<double> declinations { -23.44, -10.0, 0.0, 10.0, 23.44 };
  const std::vector<double> latitudes    { -55.0, -30.0, 0.0, 30.0, 51.5, 65.0 };
  const std::vector<double> altitudes    { -18.0, -12.0, -6.0, -0.8333, 0.0 };

  for (const double δ : declinations) {
    for (const double φ : latitudes) {
      for (const double h0 : altitudes) {
        const auto H0 = hour_angle_at_altitude(Angle<DEG> { δ }, Angle<DEG> { φ }, Angle<DEG> { h0 });
        if (not H0.has_value()) {
          continue; // Polar cases are covered by their own test.
        }

        for (const double sign : { 1.0, -1.0 }) {
          const auto horizontal = astro::coords::equatorial_to_horizontal(
            req(H0) * sign, Angle<DEG> { δ }, Angle<DEG> { φ }
          );
          ASSERT_NEAR(horizontal.h.deg(), h0, 1e-9)
            << "δ=" << δ << " φ=" << φ << " h0=" << h0 << " sign=" << sign;
        }
      }
    }
  }
}

TEST(SunriseSunset, HourAngleAtAltitudePolarAndDegenerateCases) {
  const Angle<DEG> h0_standard { -0.8333 };

  // Midnight sun at 80°N in northern summer: the Sun's minimum altitude is ~+13.4°, far above h₀.
  ASSERT_FALSE(hour_angle_at_altitude(Angle<DEG> { 23.44 }, Angle<DEG> { 80.0 }, h0_standard).has_value());

  // Polar night at 80°N in northern winter: the maximum altitude is ~-13.4°, below h₀.
  ASSERT_FALSE(hour_angle_at_altitude(Angle<DEG> { -23.44 }, Angle<DEG> { 80.0 }, h0_standard).has_value());

  // The equation degenerates at the geographic pole (cos φ = 0).
  ASSERT_FALSE(hour_angle_at_altitude(Angle<DEG> { 10.0 }, Angle<DEG> { 90.0 }, h0_standard).has_value());

  // Equator, equinoctial Sun, geometric horizon: the diurnal arc is exactly a half turn.
  const auto H0 = hour_angle_at_altitude(Angle<DEG> { 0.0 }, Angle<DEG> { 0.0 }, Angle<DEG> { 0.0 });
  ASSERT_TRUE(H0.has_value());
  ASSERT_NEAR(req(H0).deg(), 90.0, 1e-9);
}

TEST(SunriseSunset, AltitudeConstantsProvenance) {
  // Pin the externally-meaningful constants to their literature values (Meeus Ch.15's
  // -34' refraction - 16' upper limb; the standard civil/nautical/astronomical twilight
  // definitions). The solver tests are parametric in h₀ and would stay green with a silently
  // wrong constant — e.g. even h₀ = 0° keeps EquatorDayLength inside its ±30 min (per review).
  ASSERT_NEAR(STANDARD_ALTITUDE.deg(), -50.0 / 60.0, 1e-12);
  ASSERT_NEAR(CIVIL_TWILIGHT.deg(), -6.0, 1e-12);
  ASSERT_NEAR(NAUTICAL_TWILIGHT.deg(), -12.0, 1e-12);
  ASSERT_NEAR(ASTRONOMICAL_TWILIGHT.deg(), -18.0, 1e-12);
}

TEST(SunriseSunset, TransitHourAngleIsZero) {
  const std::vector<std::pair<std::chrono::year_month_day, GeoLocation>> cases {
    { util::to_ymd(2024, 6, 21), NEW_YORK },
    { util::to_ymd(2024, 6, 21), BEIJING  },
    { util::to_ymd(2024, 3, 20), LONDON   },
    { util::to_ymd(2024, 12, 21), SYDNEY  },
  };

  for (const auto& [ymd, location] : cases) {
    const double transit = transit_jde(ymd, location);
    const double H = detail::sun_local(transit, location).hour_angle_deg;
    // 0.001° of hour angle ≈ 0.24 s of time. Self-consistency only: solver and checker share
    // `detail::sun_local`, so a systematic error (e.g. a UT1/TT mixup) cancels out here — the
    // Meeus 28.a anchor below is the absolute guard (mutation-verified).
    ASSERT_NEAR(H, 0.0, 0.001);
  }
}

TEST(SunriseSunset, TransitNearLocalMeanNoon) {
  // The transit deviates from local mean noon by the equation of time only (|EoT| ≤ 16.5 min
  // ≈ 0.0115 day). A sign flip in `sun_local`'s longitude negation moves the H = 0 root ~0.65
  // day off Beijing's bracket — 32x this tolerance — so this pins that negation. (It does NOT
  // pin the estimate formula itself: the `local_mean_noon_jde` helper mirrors it by construction.)
  const std::vector<std::pair<std::chrono::year_month_day, GeoLocation>> cases {
    { util::to_ymd(2024, 2, 11), BEIJING  }, // Near the EoT minimum (~-14.2 min).
    { util::to_ymd(2024, 11, 3), NEW_YORK }, // Near the EoT maximum (~+16.5 min).
    { util::to_ymd(2024, 6, 21), TROMSO   }, // Transit exists even on a polar day.
  };

  for (const auto& [ymd, location] : cases) {
    const double transit = transit_jde(ymd, location);
    ASSERT_NEAR(transit, local_mean_noon_jde(ymd, location), 0.02);
  }
}

TEST(SunriseSunset, TransitAnchoredToMeeusEquationOfTime) {
  // Meeus Example 28.a: 1992 October 13, 0h TD — E = +3°.427351 = +13.70940 min. E > 0 means
  // the true Sun crosses the meridian before the mean sun, so at Greenwich the transit falls
  // at ≈ 12h − E of mean time (= UT). This is the one assertion in this file tied to a value
  // from OUTSIDE the computation chain: the self-consistency tests (H(transit) ≈ 0 etc.) run
  // solver and checker through the same `detail::sun_local`, so a systematic error — e.g.
  // feeding TT to the sidereal-time step — cancels out of them, but not out of this one.
  // Tolerance ±20 s: E drifts only a few seconds between 0h TD and the ~11h46m transit, and
  // the model differences are ~1 s; a UT1/TT mixup would shift the transit by ΔT ≈ 59 s
  // (epoch 1992), i.e. ~3x this tolerance.
  const auto ymd = util::to_ymd(1992, 10, 13);
  const double transit = transit_jde(ymd, loc(51.4769, 0.0)); // Greenwich; latitude does not move the transit.

  const auto ut1 = astro::julian_day::jde_to_ut1(transit);
  ASSERT_EQ(ut1.ymd, ymd);

  const double expected_fraction = 0.5 - (13.70940 / (24.0 * 60.0));
  ASSERT_NEAR(ut1.fraction(), expected_fraction, 20.0 / 86400.0);
}

TEST(SunriseSunset, TransitIsUpperCulmination) {
  const auto ymd = util::to_ymd(2024, 9, 22);
  const double transit = transit_jde(ymd, LONDON);

  const double h_transit = detail::sun_altitude(transit, LONDON).deg();
  ASSERT_GT(h_transit, detail::sun_altitude(transit - 2.0 / 24.0, LONDON).deg());
  ASSERT_GT(h_transit, detail::sun_altitude(transit + 2.0 / 24.0, LONDON).deg());
}

TEST(SunriseSunset, RiseSetAltitudeIsH0) {
  const std::vector<std::tuple<std::chrono::year_month_day, GeoLocation, Angle<DEG>>> cases {
    { util::to_ymd(2024, 3, 20), LONDON,  STANDARD_ALTITUDE },
    { util::to_ymd(2024, 6, 21), BEIJING, STANDARD_ALTITUDE },
    { util::to_ymd(2024, 12, 21), SYDNEY, STANDARD_ALTITUDE },
    { util::to_ymd(2024, 3, 20), LONDON,  CIVIL_TWILIGHT    },
    { util::to_ymd(2024, 3, 20), LONDON,  ASTRONOMICAL_TWILIGHT },
  };

  for (const auto& [ymd, location, h0] : cases) {
    const auto result = calculate(ymd, location, h0);
    ASSERT_TRUE(result.sunrise_jde.has_value());
    ASSERT_TRUE(result.sunset_jde.has_value());

    // 0.001° ≈ 0.36". At typical crossing rates (~200°/day) that is ~0.4 s of time.
    ASSERT_NEAR(detail::sun_altitude(req(result.sunrise_jde), location).deg(), h0.deg(), 0.001);
    ASSERT_NEAR(detail::sun_altitude(req(result.sunset_jde), location).deg(), h0.deg(), 0.001);
  }
}

TEST(SunriseSunset, OrderIsCorrect) {
  const std::vector<std::pair<std::chrono::year_month_day, GeoLocation>> cases {
    { util::to_ymd(2024, 6, 21), BEIJING },
    { util::to_ymd(2024, 12, 21), BEIJING },
    { util::to_ymd(2024, 6, 21), SYDNEY },
    { util::to_ymd(2024, 9, 22), EQUATOR },
  };

  for (const auto& [ymd, location] : cases) {
    const auto result = calculate(ymd, location);
    ASSERT_TRUE(result.sunrise_jde.has_value());
    ASSERT_TRUE(result.sunset_jde.has_value());
    ASSERT_LT(req(result.sunrise_jde), result.transit_jde);
    ASSERT_LT(result.transit_jde, req(result.sunset_jde));
    ASSERT_FALSE(result.is_polar_day);
    ASSERT_FALSE(result.is_polar_night);
  }
}

TEST(SunriseSunset, TwilightOrder) {
  // At 40°N the midsummer Sun still dips to ~-26.6°, so all three twilights exist.
  const auto ymd = util::to_ymd(2024, 6, 21);

  const auto r_standard = calculate(ymd, MIDLAT_0E, STANDARD_ALTITUDE);
  const auto r_civil    = calculate(ymd, MIDLAT_0E, CIVIL_TWILIGHT);
  const auto r_nautical = calculate(ymd, MIDLAT_0E, NAUTICAL_TWILIGHT);
  const auto r_astro    = calculate(ymd, MIDLAT_0E, ASTRONOMICAL_TWILIGHT);

  for (const auto& r : { r_standard, r_civil, r_nautical, r_astro }) {
    ASSERT_TRUE(r.sunrise_jde.has_value());
    ASSERT_TRUE(r.sunset_jde.has_value());
  }

  // Morning: astronomical dawn < nautical dawn < civil dawn < sunrise.
  ASSERT_LT(req(r_astro.sunrise_jde), req(r_nautical.sunrise_jde));
  ASSERT_LT(req(r_nautical.sunrise_jde), req(r_civil.sunrise_jde));
  ASSERT_LT(req(r_civil.sunrise_jde), req(r_standard.sunrise_jde));

  // Evening: sunset < civil dusk < nautical dusk < astronomical dusk.
  ASSERT_LT(req(r_standard.sunset_jde), req(r_civil.sunset_jde));
  ASSERT_LT(req(r_civil.sunset_jde), req(r_nautical.sunset_jde));
  ASSERT_LT(req(r_nautical.sunset_jde), req(r_astro.sunset_jde));
}

TEST(SunriseSunset, EquatorDayLength) {
  // At the equator the day is close to 12h all year; h₀ = -50' stretches it by a few minutes.
  for (int month = 1; month <= 12; ++month) {
    const auto ymd = util::to_ymd(2024, month, 15);
    const auto result = calculate(ymd, EQUATOR);

    ASSERT_TRUE(result.sunrise_jde.has_value());
    ASSERT_TRUE(result.sunset_jde.has_value());

    const double day_length = req(result.sunset_jde) - req(result.sunrise_jde);
    ASSERT_NEAR(day_length, 0.5, 30.0 / (24.0 * 60.0)) << "month=" << month; // 12h ± 30 min.
  }
}

TEST(SunriseSunset, RiseSetSymmetryAroundTransit) {
  // δ drifts slowly, so morning and afternoon half-arcs agree to well under a minute — but
  // assert only a loose bound to keep the test about symmetry, not about δ's exact rate.
  const auto result = calculate(util::to_ymd(2024, 9, 22), EQUATOR);
  ASSERT_TRUE(result.sunrise_jde.has_value());
  ASSERT_TRUE(result.sunset_jde.has_value());

  const double morning = result.transit_jde - req(result.sunrise_jde);
  const double evening = req(result.sunset_jde) - result.transit_jde;
  ASSERT_NEAR(morning, evening, 0.01);
}

TEST(SunriseSunset, PolarDayAndPolarNight) {
  // Tromsø (69.65°N) is inside the Arctic Circle: midnight sun in June, polar night in December.
  const auto summer = calculate(util::to_ymd(2024, 6, 21), TROMSO);
  ASSERT_FALSE(summer.sunrise_jde.has_value());
  ASSERT_FALSE(summer.sunset_jde.has_value());
  ASSERT_TRUE(summer.is_polar_day);
  ASSERT_FALSE(summer.is_polar_night);

  const auto winter = calculate(util::to_ymd(2024, 12, 21), TROMSO);
  ASSERT_FALSE(winter.sunrise_jde.has_value());
  ASSERT_FALSE(winter.sunset_jde.has_value());
  ASSERT_FALSE(winter.is_polar_day);
  ASSERT_TRUE(winter.is_polar_night);

  // In the December polar night the Sun still culminates at ~-3.1°, above the civil-twilight
  // altitude — so civil dawn/dusk exist even though sunrise/sunset do not.
  const auto winter_civil = calculate(util::to_ymd(2024, 12, 21), TROMSO, CIVIL_TWILIGHT);
  ASSERT_TRUE(winter_civil.sunrise_jde.has_value());
  ASSERT_TRUE(winter_civil.sunset_jde.has_value());
  ASSERT_FALSE(winter_civil.is_polar_day);
  ASSERT_FALSE(winter_civil.is_polar_night);
}

TEST(SunriseSunset, PolarNightOnsetWeekIsCoherent) {
  // Tromsø's polar night (h₀ = -50') begins in the last days of November. Scan the onset
  // window: every day must be exactly one of {both events; one event + no flags; no events +
  // the night flag}, and once polar night starts it must persist through the window. This
  // exercises the fallback bracket (transit → solved lower culmination) in the regime the
  // review flagged: short nights centered near the lower culmination.
  bool seen_polar_night = false;

  for (int day = 20; day <= 30; ++day) {
    const auto result = calculate(util::to_ymd(2024, 11, day), TROMSO);
    const int events = static_cast<int>(result.sunrise_jde.has_value())
                     + static_cast<int>(result.sunset_jde.has_value());

    if (events == 0) {
      ASSERT_TRUE(result.is_polar_night) << "day=" << day; // November at 69.65°N: night, never day.
      ASSERT_FALSE(result.is_polar_day) << "day=" << day;
      seen_polar_night = true;
    } else {
      ASSERT_FALSE(result.is_polar_day) << "day=" << day;
      ASSERT_FALSE(result.is_polar_night) << "day=" << day;
      ASSERT_FALSE(seen_polar_night) << "day=" << day; // No coming back out of the night in this window.
      if (events == 2) {
        ASSERT_LT(req(result.sunrise_jde), result.transit_jde);
        ASSERT_LT(result.transit_jde, req(result.sunset_jde));
      }
    }
  }

  ASSERT_TRUE(seen_polar_night); // The onset falls inside the scanned window.
}

TEST(SunriseSunset, MidnightSunOnsetWeekIsCoherent) {
  // The mirror window: Tromsø's midnight sun begins in mid-May. This is the only regime where
  // one-sided days (a rise or set alone, both flags false) can occur — the nights collapse one
  // side at a time around the lower culminations — so the scan exercises that branch when the
  // year's alignment produces one, and the polar-day onset in any case.
  bool seen_polar_day = false;

  for (int day = 12; day <= 24; ++day) {
    const auto result = calculate(util::to_ymd(2024, 5, day), TROMSO);
    const int events = static_cast<int>(result.sunrise_jde.has_value())
                     + static_cast<int>(result.sunset_jde.has_value());

    if (events == 0) {
      ASSERT_TRUE(result.is_polar_day) << "day=" << day; // May at 69.65°N: day, never night.
      ASSERT_FALSE(result.is_polar_night) << "day=" << day;
      seen_polar_day = true;
    } else {
      ASSERT_FALSE(result.is_polar_day) << "day=" << day;
      ASSERT_FALSE(result.is_polar_night) << "day=" << day;
      if (events == 2) {
        ASSERT_LT(req(result.sunrise_jde), result.transit_jde);
        ASSERT_LT(result.transit_jde, req(result.sunset_jde));
      }
    }
  }

  ASSERT_TRUE(seen_polar_day); // The onset falls inside the scanned window.
}

TEST(SunriseSunset, InvalidInputsThrow) {
  const auto ymd = util::to_ymd(2024, 6, 21);
  constexpr double NAN_D = std::numeric_limits<double>::quiet_NaN();

  ASSERT_THROW((void) transit_jde(ymd, loc(90.5, 0.0)), std::invalid_argument);
  ASSERT_THROW((void) transit_jde(ymd, loc(0.0, 180.5)), std::invalid_argument);
  ASSERT_THROW((void) transit_jde(ymd, loc(NAN_D, 0.0)), std::invalid_argument);
  ASSERT_THROW((void) transit_jde(ymd, loc(0.0, NAN_D)), std::invalid_argument);

  // An invalid gregorian date is rejected by `Datetime`'s constructor.
  ASSERT_THROW((void) transit_jde(util::to_ymd(2024, 2, 30), EQUATOR), std::invalid_argument);

  const double transit = transit_jde(ymd, EQUATOR);
  ASSERT_THROW((void) rise_set_jde(NAN_D, true, EQUATOR), std::invalid_argument);
  ASSERT_THROW((void) rise_set_jde(transit, true, EQUATOR, Angle<DEG> { NAN_D }), std::invalid_argument);
  ASSERT_THROW((void) rise_set_jde(transit, true, EQUATOR, Angle<DEG> { 100.0 }), std::invalid_argument);
  ASSERT_THROW((void) rise_set_jde(transit, true, loc(-91.0, 0.0)), std::invalid_argument);

  // hour_angle_at_altitude is public API too: out-of-domain angles alias through sin/cos into
  // physically meaningless H₀ values, so they are rejected rather than returned (per review).
  ASSERT_THROW((void) hour_angle_at_altitude(Angle<DEG> { NAN_D }, Angle<DEG> { 0.0 }, Angle<DEG> { 0.0 }),
               std::invalid_argument);
  ASSERT_THROW((void) hour_angle_at_altitude(Angle<DEG> { 0.0 }, Angle<DEG> { 95.0 }, Angle<DEG> { 0.0 }),
               std::invalid_argument);
  ASSERT_THROW((void) hour_angle_at_altitude(Angle<DEG> { 0.0 }, Angle<DEG> { 0.0 }, Angle<DEG> { 100.0 }),
               std::invalid_argument);
}

} // namespace astro::sunrise_sunset::test
