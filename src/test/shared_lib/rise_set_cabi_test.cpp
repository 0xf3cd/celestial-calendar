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

#include <array>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

#include "celestial.h"
#include "lib_codes.hpp"
#include "planet_rise_set.hpp"
#include "rise_set.hpp"
#include "ymd.hpp"

extern "C" auto rise_set_example_main() -> int;

namespace astro::rise_set::test {
namespace {

/** @brief Require exact optional-value transport and canonical absent storage. */
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): Value/mask/bit mirrors the C optional-event fields.
auto expect_event(const double actual, const uint32_t mask, const uint32_t bit, const std::optional<double>& expected) -> void {
  EXPECT_EQ((mask & bit) != 0, expected.has_value());
  EXPECT_EQ(actual, expected.value_or(0.0));
}

/** @brief Compare common C fields without deriving C codes from the C++ enum ordinals. */
auto expect_result(const auto& actual, const Result& expected) -> void {
  ASSERT_TRUE(actual.valid) << celestial_last_error();
  EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_NONE);
  EXPECT_STREQ(celestial_last_error(), "");
  EXPECT_EQ(actual.delta_t_identity_code, CELESTIAL_DELTA_T_MODEL_ALGO5);
  EXPECT_EQ(actual.nutation_identity_code, CELESTIAL_NUTATION_MODEL_IAU_1980);
  EXPECT_EQ(actual.present_fields & ~(CELESTIAL_PRESENT_RISE | CELESTIAL_PRESENT_TRANSIT | CELESTIAL_PRESENT_SET), 0U);
  if (expected.polar == Polar::DAY) {
    EXPECT_EQ(actual.polar_code, CELESTIAL_RISE_SET_POLAR_DAY);
  } else if (expected.polar == Polar::NIGHT) {
    EXPECT_EQ(actual.polar_code, CELESTIAL_RISE_SET_POLAR_NIGHT);
  } else {
    EXPECT_EQ(actual.polar_code, CELESTIAL_RISE_SET_POLAR_NONE);
  }
  expect_event(actual.rise_jde_tt, actual.present_fields, CELESTIAL_PRESENT_RISE, expected.rise_jde_tt);
  expect_event(actual.transit_jde_tt, actual.present_fields, CELESTIAL_PRESENT_TRANSIT, expected.transit_jde_tt);
  expect_event(actual.set_jde_tt, actual.present_fields, CELESTIAL_PRESENT_SET, expected.set_jde_tt);
}

struct Day {
  int32_t year;
  uint32_t month;
  uint32_t day;
  double latitude;
  double longitude;
  double elevation;
};

// Native wrapper parity, not a new numerical reference or tolerance.
// NOLINTBEGIN(modernize-use-designated-initializers): Keep the fixed UT1 date/site parity columns aligned.
constexpr std::array DAYS {
  Day { 2000,  1,  1,  40.0,    0.0,    0.0 },
  Day { 2025,  3, 20,  51.5,    0.0,    0.0 },
  Day { 2025,  6, 21,  67.0,   20.0,    0.0 },
  Day { 2025, 12, 21, -67.0,  100.0,    0.0 },
  Day { 2026, 11, 16,  40.0,  116.0,    0.0 },
  Day { 2000,  1,  1,  40.0,  180.0, 1000.0 },
  Day { 2000,  1,  1,  40.0, -180.0, 1000.0 },
  Day { 2000,  1,  1,  90.0,    0.0,    0.0 },
  Day { 2000,  1,  1, -90.0,    0.0,    0.0 },
  Day { 2019,  8, 27,  75.0,    0.0,    0.0 },
  Day { 1885,  1,  2,  40.0,    0.0,    0.0 },
  Day { 1885,  1,  3,  40.0,    0.0,    0.0 },
  Day { 2099, 12, 30,  40.0,    0.0,    0.0 },
  Day { 2099, 12, 31,  40.0,    0.0,    0.0 },
};
// NOLINTEND(modernize-use-designated-initializers)

} // namespace

TEST(RiseSetCabi, SolarAndLunarNativeParity) {
  for (const auto& day : DAYS) {
    const auto date = util::to_ymd(day.year, day.month, day.day);
    const GeoLocation location { .latitude = toolbox::AngleDeg { day.latitude }, .longitude = toolbox::AngleDeg { day.longitude } };
    for (const auto h0 : { sun::STANDARD_ALTITUDE, sun::ASTRONOMICAL_TWILIGHT }) {
      const auto result = celestial_solar_rise_set(day.year, day.month, day.day, day.latitude, day.longitude, h0.deg());
      expect_result(result, sun::calculate(date, location, h0));
      EXPECT_EQ(result.source_code, CELESTIAL_POSITION_SOURCE_SUN_VSOP87D);
      EXPECT_EQ(result.h0_deg, h0.deg());
    }
    for (const uint32_t model : { CELESTIAL_REFRACTION_BENNETT, CELESTIAL_REFRACTION_SAEMUNDSSON }) {
      const double temperature = model == CELESTIAL_REFRACTION_BENNETT ? 15.0 : -20.0;
      const double pressure = model == CELESTIAL_REFRACTION_BENNETT ? 1013.25 : 800.0;
      const auto params = lib::codes::refraction_params(model, temperature, pressure);
      if (not params.has_value()) {
        throw std::logic_error { "The lunar parity case requires horizon refraction" };
      }
      const auto result = celestial_lunar_rise_set(day.year, day.month, day.day, day.latitude, day.longitude, model, temperature, pressure);
      expect_result(result, moon::calculate(date, location, *params));
      EXPECT_EQ(result.source_code, CELESTIAL_POSITION_SOURCE_MOON_ELP2000_82B_TRUNC);
      EXPECT_EQ(result.refraction_code, model);
      EXPECT_EQ(result.temperature_c, temperature);
      EXPECT_EQ(result.pressure_hpa, pressure);
    }
  }
}

TEST(RiseSetCabi, PlanetaryNativeParityAndSampleDomains) {
  uint32_t rejected = 0;
  for (const auto& day : DAYS) {
    const observer::Observer site {
      .location = { .latitude = toolbox::AngleDeg { day.latitude }, .longitude = toolbox::AngleDeg { day.longitude } },
      .elevation = toolbox::DistanceKm { day.elevation / 1000.0 },
    };
    for (const auto& target : lib::codes::TARGETS) {
      if (not ephemeris::target_facts(target.value).planet.has_value()) {
        continue;
      }
      for (const auto h0 : { planet::STANDARD_CENTER_ALTITUDE, toolbox::AngleDeg { 0.0 } }) {
        std::optional<Result> expected;
        std::string error;
        try {
          expected = planet::calculate(util::to_ymd(day.year, day.month, day.day), site, target.value, h0);
        } catch (const std::invalid_argument& failure) {
          error = failure.what();
        }
        const auto actual = celestial_planet_rise_set(
          target.code, day.year, day.month, day.day, day.latitude, day.longitude, day.elevation, h0.deg()
        );
        if (not expected.has_value()) {
          EXPECT_FALSE(actual.valid);
          EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
          EXPECT_EQ(celestial_last_error(), error);
          ++rejected;
          continue;
        }
        expect_result(actual, *expected);
        EXPECT_EQ(actual.target_code, target.code);
        EXPECT_EQ(actual.source_code, lib::codes::code_of(ephemeris::source_of(target.value)));
        EXPECT_EQ(actual.h0_deg, h0.deg());
      }
    }
  }
  EXPECT_EQ(rejected, 4U);
}

TEST(RiseSetCabi, FullWidthInputsAndErrorClearing) {
  for (const uint32_t month : { 0U, 13U, 257U, std::numeric_limits<uint32_t>::max() }) {
    EXPECT_FALSE(celestial_solar_rise_set(2025, month, 1, 0, 0, -50.0 / 60).valid);
    EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
    EXPECT_FALSE(celestial_lunar_rise_set(2025, month, 1, 0, 0, CELESTIAL_REFRACTION_BENNETT, 15, 1013.25).valid);
    EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
    EXPECT_FALSE(celestial_planet_rise_set(CELESTIAL_TARGET_MARS, 2025, month, 1, 0, 0, 0, -34.0 / 60).valid);
    EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
  }
  for (const uint32_t target : { CELESTIAL_TARGET_SUN, CELESTIAL_TARGET_MOON, CELESTIAL_TARGET_TRUE_ASCENDING, 258U, 0xffffffffU }) {
    EXPECT_FALSE(celestial_planet_rise_set(target, 2025, 1, 1, 0, 0, 0, 0).valid);
    EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
  }
  for (const uint32_t model : { CELESTIAL_REFRACTION_NONE, 257U, 0xffffffffU }) {
    EXPECT_FALSE(celestial_lunar_rise_set(2025, 1, 1, 0, 0, model, 15, 1013.25).valid);
    EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
  }
  EXPECT_TRUE(celestial_solar_rise_set(2025, 1, 1, 0, 0, -50.0 / 60).valid);
  EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_NONE);
  EXPECT_STREQ(celestial_last_error(), "");
}

TEST(RiseSetCabi, LunarHorizonWeatherIsNotObserverApplicationWeather) {
  const auto params = earth::refraction::Params { .temperature_c = -100.0, .pressure_hpa = 200.0 };
  const GeoLocation site { .latitude = toolbox::AngleDeg { 40.0 }, .longitude = toolbox::AngleDeg { 0.0 } };
  const auto result = celestial_lunar_rise_set(2025, 1, 1, 40, 0, CELESTIAL_REFRACTION_BENNETT, -100, 200);
  expect_result(result, moon::calculate(util::to_ymd(2025, 1, 1), site, params));
  EXPECT_FALSE(celestial_lunar_rise_set(2025, 1, 1, 40, 0, CELESTIAL_REFRACTION_BENNETT, -273, 200).valid);
  EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
}

TEST(RiseSetCabi, ExecutableC11ExampleAndCapabilityText) {
  EXPECT_EQ(rise_set_example_main(), 0);
  const uint32_t count = celestial_capability_text(CELESTIAL_OPERATION_RISE_SET, CELESTIAL_TARGET_ALL, nullptr, 0);
  ASSERT_GT(count, 1U);
  std::string text(count, '\0');
  EXPECT_EQ(celestial_capability_text(CELESTIAL_OPERATION_RISE_SET, CELESTIAL_TARGET_ALL, text.data(), count), count);
  EXPECT_NE(text.find("transit-centred"), std::string::npos);
  EXPECT_NE(text.find("input date's 0h UT1"), std::string::npos);
  EXPECT_NE(text.find("TT"), std::string::npos);
}

TEST(RiseSetCabi, ExactMidnightAltitudeAndYearRollover) {
  const observer::Observer site {
    .location = { .latitude = toolbox::AngleDeg { 40.0 }, .longitude = toolbox::AngleDeg { 0.0 } },
    .elevation = toolbox::DistanceKm { 0.0 },
  };
  const auto midnight = julian_day::ut1_to_jde(calendar::Datetime { util::to_ymd(2025, 1, 1), 0.0 });
  const auto provider = planet::detail::provider_for(site, ephemeris::Target::MARS);
  const auto h0 = detail::altitude(midnight, site.location, provider);
  for (const auto& date : { util::to_ymd(2024, 12, 31), util::to_ymd(2025, 1, 1) }) {
    const auto expected = planet::calculate(date, site, ephemeris::Target::MARS, h0);
    const auto actual = celestial_planet_rise_set(
      CELESTIAL_TARGET_MARS,
      static_cast<int32_t>(date.year()),
      static_cast<uint32_t>(date.month()),
      static_cast<uint32_t>(date.day()),
      40.0, 0.0, 0.0, h0.deg()
    );
    expect_result(actual, expected);
    EXPECT_NE(actual.rise_jde_tt, midnight);
    EXPECT_NE(actual.set_jde_tt, midnight);
  }
}

} // namespace astro::rise_set::test
