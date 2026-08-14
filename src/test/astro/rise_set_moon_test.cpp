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
#include <cmath>
#include <limits>
#include <optional>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "util.hpp"
#include "rise_set.hpp"
#include "rise_set_test_helper.hpp"

// Unit-level pins for the lunar entry points (#62) — semantics and coherence properties that
// do not need external data. External accuracy is pinned by rise_set_moon_golden_test.cpp
// (USNO, ±2 min); the solar behavior of the shared engine by rise_set_test.cpp.

namespace astro::rise_set::test {

using namespace astro::rise_set;
using astro::toolbox::AngleDeg;

namespace {

const GeoLocation BEIJING = loc( 39.9042, 116.4074);
const GeoLocation EQUATOR = loc(  0.0,      0.0);

} // anonymous namespace


TEST(RiseSetMoon, H0FormulaProvenance) {
  // Meeus Ch.15: the Moon's standard altitude is h₀ = 0.7275·Π − 34′ (standard refraction).
  // At the mean parallax Π ≈ 57′ that is 0.7275 × 57′ − 34′ = 7.47′ — positive, unlike the
  // Sun's −50′, because the parallax term outweighs refraction at lunar distances.
  const auto Π = astro::toolbox::AngleDeg { 57.0 / 60.0 };
  const auto h0 = moon::h0(astro::toolbox::AngleRad { Π.rad() });
  // The refraction term is `at_horizon(Params{})` = 33.988′ (Bennett@standard), not exactly
  // 34′ — the same 0.02′ residual the solar convention carries (#61). Tolerance absorbs it.
  ASSERT_NEAR(h0.deg(), (0.7275 * 57.0 - 34.0) / 60.0, AngleDeg::from_arcmin(0.02).deg());

  // A non-standard atmosphere must flow through the same convention: colder, denser air
  // refracts more, so h₀ sinks.
  astro::earth::refraction::Params cold;
  cold.temperature_c = -20.0;
  ASSERT_LT(moon::h0(astro::toolbox::AngleRad { Π.rad() }, cold).deg(), h0.deg());

  // The Saemundsson model flows through the same wiring (R2 测试缝隙席:这条链之前没有
  // 端到端覆盖):h0 与手工合成的 0.7275·Π − at_horizon 逐项一致。
  astro::earth::refraction::Params saemundsson;
  saemundsson.model = astro::earth::refraction::Model::SAEMUNDSSON;
  const auto expected = astro::toolbox::AngleDeg { 0.7275 * Π.deg() }
                      - astro::earth::refraction::at_horizon(saemundsson);
  ASSERT_NEAR(moon::h0(astro::toolbox::AngleRad { Π.rad() }, saemundsson).deg(),
              expected.deg(), 1e-12);
}

TEST(RiseSetMoon, RiseSetAltitudeIsH0) {
  // The events must actually sit on the h₀ curve, with h₀ from the same day's Π.
  const auto ymd = util::to_ymd(2026, 8, 15); // Beijing: rise, transit, and set all exist.
  const auto result = moon::calculate(ymd, BEIJING);
  ASSERT_TRUE(result.rise_jde.has_value());
  ASSERT_TRUE(result.set_jde.has_value());

  // h₀ must be reconstructed with the engine's own convention: Π at mid-day, fixed for the
  // whole UT day (see `moon::h0`'s error-budget note). Comparing against the transit-time Π
  // instead would measure the Π day-drift (~0.002°), not the solver residual (~1e-7°).
  const calendar::Datetime day_start { ymd, 0.0 };
  const double midday = astro::julian_day::ut1_to_jde(day_start) + 0.5;
  const auto h0 = moon::h0(moon::horizontal_parallax(midday));
  // 0.002° of altitude at the Moon's ~10-14°/h horizon-crossing rate is ~1 s of time.
  ASSERT_NEAR(detail::altitude(req(result.rise_jde), BEIJING, moon::apparent_equatorial).deg(),
              h0.deg(), 0.002);
  ASSERT_NEAR(detail::altitude(req(result.set_jde), BEIJING, moon::apparent_equatorial).deg(),
              h0.deg(), 0.002);
}

TEST(RiseSetMoon, OrderIsCorrectWhenAllThreeExist) {
  const auto ymd = util::to_ymd(2026, 8, 15); // Beijing: rise 00:04, transit 06:19, set 12:23 UT.
  const auto result = moon::calculate(ymd, BEIJING);
  ASSERT_TRUE(result.rise_jde.has_value());
  ASSERT_TRUE(result.transit_jde.has_value());
  ASSERT_TRUE(result.set_jde.has_value());
  ASSERT_LT(req(result.rise_jde), req(result.transit_jde));
  ASSERT_LT(req(result.transit_jde), req(result.set_jde));
  ASSERT_EQ(result.polar, Polar::NONE);
}

TEST(RiseSetMoon, ThirtyDayScanIsCoherent) {
  // A 30-day Beijing scan exercises every branch of the day-window engine: normal days,
  // the skipped-moonrise day, and transit-less days. The properties pinned here are the
  // UT-day window semantics at MID-LATITUDE: at most one event of each kind per UT date
  // (high-latitude standstill days can have two — see moon::calculate's note and the
  // 2026-05-14 golden row), instants strictly inside the date, consecutive same-kind
  // events 24–26 h apart, no duplicates.
  int rise_less = 0;
  int transit_less = 0;
  std::optional<double> prev_rise;
  std::vector<double> all_events;

  for (unsigned day = 1; day <= 30; ++day) {
    const auto ymd = util::to_ymd(2026, 8, day);
    const auto result = moon::calculate(ymd, BEIJING);
    ASSERT_EQ(result.polar, Polar::NONE) << "day=" << day; // 39.9°N: never polar for the Moon.

    rise_less += result.rise_jde.has_value() ? 0 : 1;
    transit_less += result.transit_jde.has_value() ? 0 : 1;

    for (const auto& event : { result.rise_jde, result.transit_jde, result.set_jde }) {
      if (event.has_value()) {
        ASSERT_EQ(astro::julian_day::jde_to_ut1(*event).ymd, ymd) << "day=" << day;
        all_events.push_back(*event);
      }
    }

    if (result.rise_jde.has_value() and prev_rise.has_value()) {
      const double gap_hours = (req(result.rise_jde) - req(prev_rise)) * 24.0;
      ASSERT_GT(gap_hours, 24.0) << "day=" << day;
      ASSERT_LT(gap_hours, 26.0) << "day=" << day;
    }
    if (result.rise_jde.has_value()) {
      prev_rise = result.rise_jde;
    }
  }

  // 30 days × (24 / 24.84 h) ≈ 29 rises and ≈ 29 transits: exactly one skipped day of each,
  // give or take window-edge alignment.
  ASSERT_GE(rise_less, 1);
  ASSERT_LE(rise_less, 2);
  ASSERT_GE(transit_less, 1);
  ASSERT_LE(transit_less, 2);

  // No instant may be attributed to two dates — the UT-day windows partition the timeline.
  std::ranges::sort(all_events);
  ASSERT_EQ(std::ranges::adjacent_find(all_events), all_events.end());
}

TEST(RiseSetMoon, TransitInWindowHandlesNonUnitWindows) {
  // The estimate must not scale with the window length (R1: it once multiplied by
  // `window_days`, silently correct only for the 1-day windows `calculate_day` passes).
  // 2026-08-15 Beijing: the true lunar transit is at ~06:19 UT (fraction ≈ 0.2635).
  const calendar::Datetime day_start { util::to_ymd(2026, 8, 15), 0.0 };
  const double t0 = astro::julian_day::ut1_to_jde(day_start);

  // A 0.3-day window containing the transit finds it, on the root (residual guard).
  const auto found = transit_in_window(t0, t0 + 0.3, BEIJING, moon::apparent_equatorial);
  ASSERT_TRUE(found.has_value());
  ASSERT_NEAR(req(found), t0 + 0.2635, 2.0 / 1440.0);
  ASSERT_NEAR(detail::body_local(req(found), BEIJING, moon::apparent_equatorial).hour_angle_deg,
              0.0, TRANSIT_RESIDUAL_GUARD_DEG);

  // A later window that day has no transit.
  ASSERT_FALSE(transit_in_window(t0 + 0.3, t0 + 0.9, BEIJING, moon::apparent_equatorial).has_value());

  // A two-day window returns the FIRST transit inside it.
  const auto first = transit_in_window(t0, t0 + 2.0, BEIJING, moon::apparent_equatorial);
  ASSERT_TRUE(first.has_value());
  ASSERT_NEAR(req(first), req(found), 1e-9);
}

TEST(RiseSetMoon, TransitInWindowMeasuresAlphaRate) {
  // Synthetic fast body: α sweeps 30°/day — far faster than the Moon ever gets. If
  // `transit_in_window` fell back to the raw sidereal rate instead of measuring dα/dt
  // (the R1 gate mutation that survived the suite), the estimate would miss the ±0.05-day
  // polish bracket whenever the forward hour angle is large (error up to ~0.09 day) and
  // the residual guard would throw. Sweeping the window start covers the full circle of
  // forward angles.
  const double jde0 = astro::julian_day::ut1_to_jde(
    calendar::Datetime { util::to_ymd(2026, 8, 15), 0.0 });
  const auto fast_body = [jde0](const double jde) -> astro::coords::EquatorialCoord {
    return {
      .α = AngleDeg { 30.0 * (jde - jde0) }.normalize(),
      .δ = AngleDeg { 10.0 },
    };
  };

  int found_count = 0;
  for (int k = 0; k < 8; ++k) {
    const double start = jde0 + (0.15 * k);
    const auto transit = transit_in_window(start, start + 1.0, EQUATOR, fast_body);
    if (not transit.has_value()) {
      continue; // Period ~1.088 d > 1 d window: some windows legitimately hold no transit.
    }
    ASSERT_NEAR(detail::body_local(req(transit), EQUATOR, fast_body).hour_angle_deg,
                0.0, TRANSIT_RESIDUAL_GUARD_DEG) << "start offset=" << 0.15 * k;
    ++found_count;
  }
  // ~92% of 1-day windows hold a transit (1.0/1.088); assert a loose floor against an
  // engine that silently finds none.
  ASSERT_GE(found_count, 5);
}

TEST(RiseSetMoon, InvalidInputsThrow) {
  const auto ymd = util::to_ymd(2026, 8, 15);
  constexpr double NAN_D = std::numeric_limits<double>::quiet_NaN();

  ASSERT_THROW(std::ignore = moon::calculate(ymd, loc(90.5, 0.0)), std::invalid_argument);
  ASSERT_THROW(std::ignore = moon::calculate(ymd, loc(0.0, -180.5)), std::invalid_argument);
  ASSERT_THROW(std::ignore = moon::calculate(ymd, loc(NAN_D, 0.0)), std::invalid_argument);
  ASSERT_THROW(std::ignore = moon::calculate(util::to_ymd(2026, 2, 29), EQUATOR), std::invalid_argument);

  // transit_in_window is public: a non-positive or non-finite window is rejected, not misread.
  ASSERT_THROW(std::ignore = transit_in_window(100.0, 100.0, EQUATOR, moon::apparent_equatorial),
               std::invalid_argument);
  ASSERT_THROW(std::ignore = transit_in_window(NAN_D, 101.0, EQUATOR, moon::apparent_equatorial),
               std::invalid_argument);
}

} // namespace astro::rise_set::test
