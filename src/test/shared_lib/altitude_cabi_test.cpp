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

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <vector>

#include <gtest/gtest.h>

#include "altitude_events.hpp"
#include "celestial.h"

namespace lib::altitude::test {
namespace {

/** @brief Sun centre above a geometric threshold, with no unused atmospheric storage. */
auto condition(const double level = 0.0) -> CelestialAltitudeCondition {
  return {
    .target_code = CELESTIAL_TARGET_SUN, .sense_code = CELESTIAL_ALTITUDE_SENSE_ABOVE,
    .refraction_code = CELESTIAL_REFRACTION_NONE, .reserved = 0,
    .level_deg = level, .temperature_c = 0.0, .pressure_hpa = 0.0,
  };
}

/** @brief Recognizable caller-owned storage, including every unused field and tail byte. */
auto sentinels() -> std::array<CelestialAltitudeRecord, 16> {
  std::array<CelestialAltitudeRecord, 16> records {};
  std::memset(records.data(), 0xA5, sizeof(records));
  return records;
}

/** @brief Compare the exact stored bytes, including signed zero and untouched sentinel fields. */
auto same_bytes(
  const std::span<const CelestialAltitudeRecord> left,
  const std::span<const CelestialAltitudeRecord> right
) -> bool {
  return std::ranges::equal(std::as_bytes(left), std::as_bytes(right));
}

/** @brief Invoke the equatorial one-day Sun fixture using the published C parameter order. */
auto events(
  const CelestialAltitudeCondition* input,
  uint32_t* count,
  CelestialAltitudeRecord* slots,
  const uint32_t capacity
) -> uint32_t {
  return celestial_altitude_events(input, 0.0, -77.0, 0.0, 2451545.0, 2451546.0, count, slots, capacity);
}

/** @brief Check the fields that a tag does not own against an all-zero record. */
auto zero_unused(CelestialAltitudeRecord record) -> bool {
  const uint32_t tag = record.tag;
  record.tag = 0;
  if (tag == CELESTIAL_ALTITUDE_RECORD_CONDITION) {
    record.condition_index = record.target_code = record.position_source_code = record.delta_t_model_code = 0;
    record.refraction_code = record.sense_code = record.initially_satisfied = 0;
    record.requested_level_deg = record.geometric_level_deg = record.temperature_c = record.pressure_hpa = 0.0;
  } else if (tag == CELESTIAL_ALTITUDE_RECORD_EVENT) {
    record.condition_index = record.direction_code = 0;
    record.jde_tt = 0.0;
  } else if (tag == CELESTIAL_ALTITUDE_RECORD_WINDOW) {
    record.jde_tt = record.end_jde_tt = 0.0;
  } else if (tag == CELESTIAL_ALTITUDE_RECORD_UPPER_TRANSIT) {
    record.target_code = record.position_source_code = record.delta_t_model_code = 0;
    record.jde_tt = 0.0;
  } else {
    return false;
  }
  const CelestialAltitudeRecord zero {};
  return same_bytes(std::span { &record, 1 }, std::span { &zero, 1 });
}

} // namespace

TEST(AltitudeCAbi, CountEveryPrefixAndUntouchedTailMatchTheCompleteFill) {
  const auto input = condition();
  uint32_t total = 99;
  ASSERT_EQ(events(&input, &total, nullptr, 0), 0U);
  ASSERT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_NONE);
  ASSERT_EQ(total, 3U);
  auto full = sentinels();
  ASSERT_EQ(events(&input, &total, full.data(), 16), total);
  for (uint32_t capacity = 0; capacity <= total + 1; ++capacity) {
    auto slots = sentinels();
    const auto before = slots;
    uint32_t count = 99;
    const uint32_t written = events(&input, &count, slots.data(), capacity);
    ASSERT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_NONE);
    EXPECT_EQ(count, total);
    EXPECT_EQ(written, std::min(total, capacity));
    EXPECT_TRUE(same_bytes(std::span { slots }.first(written), std::span { full }.first(written)));
    EXPECT_TRUE(same_bytes(std::span { slots }.subspan(written), std::span { before }.subspan(written)));
  }
}

TEST(AltitudeCAbi, TagsMetadataDirectionsAndUnusedFieldsMatchTypedCpp) {
  const auto input = condition();
  auto slots = sentinels();
  uint32_t count = 99;
  ASSERT_EQ(events(&input, &count, slots.data(), 16), 3U);
  const astro::observer::Observer site {
    .location = { .latitude = astro::toolbox::AngleDeg { 0.0 }, .longitude = astro::toolbox::AngleDeg { -77.0 } },
    .elevation = astro::toolbox::DistanceKm { 0.0 },
  };
  const auto expected = astro::observer::altitude_events(
    site,
    { .target = astro::ephemeris::Target::SUN,
      .level = astro::observer::GeometricLevel { .altitude = astro::toolbox::AngleDeg { 0.0 } },
      .sense = astro::observer::AltitudeSense::ABOVE },
    2451545.0,
    2451546.0
  );
  EXPECT_EQ(slots.front().tag, CELESTIAL_ALTITUDE_RECORD_CONDITION);
  EXPECT_EQ(slots.front().target_code, CELESTIAL_TARGET_SUN);
  EXPECT_EQ(slots.front().position_source_code, CELESTIAL_POSITION_SOURCE_SUN_VSOP87D);
  EXPECT_EQ(slots.front().delta_t_model_code, CELESTIAL_DELTA_T_MODEL_ALGO5);
  EXPECT_EQ(slots.front().refraction_code, CELESTIAL_REFRACTION_NONE);
  EXPECT_EQ(slots.front().sense_code, CELESTIAL_ALTITUDE_SENSE_ABOVE);
  EXPECT_EQ(slots.front().initially_satisfied, static_cast<uint32_t>(expected.condition.initially_satisfied));
  for (std::size_t index = 0; index < expected.events.size(); ++index) {
    const auto& record = slots.at(index + 1);
    EXPECT_EQ(record.tag, CELESTIAL_ALTITUDE_RECORD_EVENT);
    EXPECT_EQ(record.condition_index, 0U);
    EXPECT_DOUBLE_EQ(record.jde_tt, expected.events.at(index).jde_tt);
    EXPECT_EQ(record.direction_code, static_cast<uint32_t>(expected.events.at(index).direction));
  }
  for (const auto& record : std::span { slots.data(), count }) {
    EXPECT_TRUE(zero_unused(record));
  }
}

TEST(AltitudeCAbi, PublicRefractionCodesAndWeatherSurviveBothModelMappings) {
  for (const uint32_t code : { CELESTIAL_REFRACTION_BENNETT, CELESTIAL_REFRACTION_SAEMUNDSSON }) {
    auto input = condition(10.0);
    input.refraction_code = code;
    input.temperature_c = 15.0;
    input.pressure_hpa = 1013.25;
    auto slots = sentinels();
    uint32_t count = 99;
    ASSERT_EQ(celestial_altitude_events(&input, 90.0, -77.0, 0.0, 2451545.0, 2451545.25, &count, slots.data(), 16), 1U);
    ASSERT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_NONE);
    EXPECT_EQ(slots.front().refraction_code, code);
    EXPECT_DOUBLE_EQ(slots.front().requested_level_deg, 10.0);
    EXPECT_GT(slots.front().geometric_level_deg, 9.0);
    EXPECT_LT(slots.front().geometric_level_deg, 10.0);
    EXPECT_DOUBLE_EQ(slots.front().temperature_c, 15.0);
    EXPECT_DOUBLE_EQ(slots.front().pressure_hpa, 1013.25);
    EXPECT_TRUE(zero_unused(slots.front()));
  }
}

TEST(AltitudeCAbi, WindowStreamsPreserveConditionOrderAndChronologicalCompanions) {
  const std::array inputs { condition(), condition(-6.0) };
  auto slots = sentinels();
  uint32_t count = 99;
  const uint32_t written = celestial_observing_windows(
    inputs.data(), 2, 0.0, -77.0, 0.0, 2451545.0, 2451546.0, &count, slots.data(), 16
  );
  ASSERT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_NONE);
  ASSERT_EQ(written, 8U);
  EXPECT_EQ(count, written);
  EXPECT_EQ(slots.at(0).tag, CELESTIAL_ALTITUDE_RECORD_CONDITION);
  EXPECT_EQ(slots.at(1).tag, CELESTIAL_ALTITUDE_RECORD_CONDITION);
  EXPECT_EQ(slots.at(0).condition_index, 0U);
  EXPECT_EQ(slots.at(1).condition_index, 1U);
  for (std::size_t index = 2; index < 6; ++index) {
    EXPECT_EQ(slots.at(index).tag, CELESTIAL_ALTITUDE_RECORD_EVENT);
    EXPECT_LE(slots.at(index).condition_index, 1U);
    if (index > 2) {
      EXPECT_LE(slots.at(index - 1).jde_tt, slots.at(index).jde_tt);
    }
  }
  for (std::size_t index = 6; index < 8; ++index) {
    EXPECT_EQ(slots.at(index).tag, CELESTIAL_ALTITUDE_RECORD_WINDOW);
    EXPECT_LT(slots.at(index).jde_tt, slots.at(index).end_jde_tt);
  }
  for (const auto& record : std::span { slots.data(), count }) {
    EXPECT_TRUE(zero_unused(record));
  }
}

TEST(AltitudeCAbi, EmptyConjunctionAndTransitUseTheirDistinctRecordShapes) {
  auto slots = sentinels();
  uint32_t count = 99;
  ASSERT_EQ(celestial_observing_windows(nullptr, 0, 90.0, 0.0, 0.0, 13689325.25, 13689325.5, &count, slots.data(), 16), 1U);
  EXPECT_EQ(count, 1U);
  EXPECT_EQ(slots.front().tag, CELESTIAL_ALTITUDE_RECORD_WINDOW);
  EXPECT_DOUBLE_EQ(slots.front().jde_tt, 13689325.25);
  EXPECT_DOUBLE_EQ(slots.front().end_jde_tt, 13689325.5);
  EXPECT_TRUE(zero_unused(slots.front()));
  ASSERT_EQ(celestial_upper_transits(CELESTIAL_TARGET_MARS, 0.0, -77.0, 0.0, 2451545.0, 2451546.0, &count, slots.data(), 16), 1U);
  EXPECT_EQ(count, 1U);
  EXPECT_EQ(slots.front().tag, CELESTIAL_ALTITUDE_RECORD_UPPER_TRANSIT);
  EXPECT_EQ(slots.front().target_code, CELESTIAL_TARGET_MARS);
  EXPECT_EQ(slots.front().position_source_code, CELESTIAL_POSITION_SOURCE_PLANET_VSOP87D);
  EXPECT_EQ(slots.front().delta_t_model_code, CELESTIAL_DELTA_T_MODEL_ALGO5);
  EXPECT_TRUE(zero_unused(slots.front()));
}

TEST(AltitudeCAbi, PointerFailuresZeroTheCountAndLeaveSlotsUntouched) {
  const auto input = condition();
  auto slots = sentinels();
  const auto before = slots;
  uint32_t count = 99;
  EXPECT_EQ(events(nullptr, &count, slots.data(), 16), 0U);
  EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
  EXPECT_EQ(count, 0U);
  EXPECT_EQ(events(&input, nullptr, slots.data(), 16), 0U);
  EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
  count = 99;
  EXPECT_EQ(events(&input, &count, nullptr, 1), 0U);
  EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
  EXPECT_EQ(count, 0U);
  EXPECT_TRUE(same_bytes(slots, before));
}

TEST(AltitudeCAbi, InvalidSelectorsReservedFieldsAndUnusedWeatherNeverWriteOutput) {
  const auto input = condition();
  auto slots = sentinels();
  const auto before = slots;
  std::array invalids { input, input, input, input, input, input, input };
  invalids.at(0).target_code = 10;
  invalids.at(1).sense_code = 2;
  invalids.at(2).refraction_code = 3;
  invalids.at(3).reserved = 1;
  invalids.at(4).temperature_c = 1.0;
  invalids.at(5).pressure_hpa = std::numeric_limits<double>::quiet_NaN();
  invalids.at(6).level_deg = 90.0;
  for (const auto& invalid : invalids) {
    uint32_t count = 99;
    EXPECT_EQ(events(&invalid, &count, slots.data(), 16), 0U);
    EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
    EXPECT_EQ(count, 0U);
    EXPECT_TRUE(same_bytes(slots, before));
  }
  uint32_t count = 99;
  EXPECT_EQ(events(&input, &count, nullptr, 0), 0U);
  EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_NONE);
}

TEST(AltitudeCAbi, LateUnresolvedConditionFailsTheWholeWindowQuery) {
  const std::array inputs {
    condition(-89.0),
    CelestialAltitudeCondition {
      .target_code = CELESTIAL_TARGET_PLUTO, .sense_code = 1, .refraction_code = CELESTIAL_REFRACTION_NONE,
      .reserved = 0, .level_deg = 48.605252016244656, .temperature_c = 0.0, .pressure_hpa = 0.0,
    },
  };
  auto slots = sentinels();
  const auto before = slots;
  uint32_t count = 99;
  EXPECT_EQ(celestial_observing_windows(inputs.data(), 2, 30.0, -77.0, 0.0, 2451545.0, 2451545.25, &count, slots.data(), 16), 0U);
  EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_COMPUTATION);
  EXPECT_EQ(count, 0U);
  EXPECT_TRUE(same_bytes(slots, before));
}

TEST(AltitudeCAbi, ExclusiveCalendarEndHasNumericalRatherThanInputFailure) {
  auto slots = sentinels();
  const auto before = slots;
  uint32_t count = 99;
  EXPECT_EQ(celestial_upper_transits(CELESTIAL_TARGET_SUN, 0.0, 0.0, 0.0, 13689325.25, 13689325.5, &count, slots.data(), 16), 0U);
  EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_COMPUTATION);
  EXPECT_EQ(count, 0U);
  EXPECT_TRUE(same_bytes(slots, before));
}

TEST(AltitudeCAbi, OnlySelectedPhysicalProvidersRestrictAdmission) {
  auto input = condition(10.0);
  auto slots = sentinels();
  uint32_t count = 99;
  for (const uint32_t target : { CELESTIAL_TARGET_SUN, CELESTIAL_TARGET_MOON, CELESTIAL_TARGET_MARS,
                               CELESTIAL_TARGET_JUPITER, CELESTIAL_TARGET_SATURN }) {
    input.target_code = target;
    ASSERT_EQ(celestial_altitude_events(&input, 90.0, -77.0, 0.0, 2488070.125, 2488070.25, &count, slots.data(), 16), 1U);
    ASSERT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_NONE);
    EXPECT_EQ(slots.front().target_code, target);
  }
  input.target_code = CELESTIAL_TARGET_PLUTO;
  const auto before = slots;
  EXPECT_EQ(celestial_altitude_events(&input, 90.0, -77.0, 0.0, 2488070.125, 2488070.25, &count, slots.data(), 16), 0U);
  EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
  EXPECT_EQ(count, 0U);
  EXPECT_TRUE(same_bytes(slots, before));
}

TEST(AltitudeCAbi, WorkExhaustionIsNumericalAndCannotPublishPartialMetadata) {
  const std::vector<CelestialAltitudeCondition> inputs(2048, condition(10.0));
  auto slots = sentinels();
  const auto before = slots;
  uint32_t count = 99;
  EXPECT_EQ(celestial_observing_windows(inputs.data(), 2048, 90.0, -77.0, 0.0, 2451545.0, 2451545.25, &count, slots.data(), 16), 0U);
  EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_COMPUTATION);
  EXPECT_EQ(count, 0U);
  EXPECT_TRUE(same_bytes(slots, before));
}

} // namespace lib::altitude::test
