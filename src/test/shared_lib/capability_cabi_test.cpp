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
#include <cctype>
#include <cmath>
#include <cstdint>
#include <format>
#include <limits>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include <gtest/gtest.h>

#include "celestial.h"
#include "delta_t.hpp"
#include "ephemeris.hpp"
#include "sun.hpp"

namespace test {
namespace {

constexpr std::array TARGET_OPERATIONS {
  CELESTIAL_OPERATION_POSITION, CELESTIAL_OPERATION_LONGITUDE_RATE,
  CELESTIAL_OPERATION_EVENTS, CELESTIAL_OPERATION_EQUATORIAL,
};
constexpr std::array SOURCES {
  CELESTIAL_POSITION_SOURCE_SUN_VSOP87D, CELESTIAL_POSITION_SOURCE_MOON_ELP2000_82B_TRUNC,
  CELESTIAL_POSITION_SOURCE_PLANET_VSOP87D, CELESTIAL_POSITION_SOURCE_PLANET_VSOP87D,
  CELESTIAL_POSITION_SOURCE_PLANET_VSOP87D, CELESTIAL_POSITION_SOURCE_PLANET_VSOP87D,
  CELESTIAL_POSITION_SOURCE_PLANET_VSOP87D, CELESTIAL_POSITION_SOURCE_PLANET_VSOP87D,
  CELESTIAL_POSITION_SOURCE_PLANET_VSOP87D, CELESTIAL_POSITION_SOURCE_PLUTO_PLUTO95_APPARENT,
  CELESTIAL_POSITION_SOURCE_NODE_MEAN, CELESTIAL_POSITION_SOURCE_NODE_MEAN,
  CELESTIAL_POSITION_SOURCE_NODE_TRUE_MEEUS_FIVE_TERM, CELESTIAL_POSITION_SOURCE_NODE_TRUE_MEEUS_FIVE_TERM,
};

// These named pairs are independent of registry order and core enum ordinals.
constexpr std::array IDENTITIES {
  std::tuple { CELESTIAL_MODEL_KIND_DELTA_T, CELESTIAL_DELTA_T_MODEL_ALGO1, "algo1", "algo1" },
  std::tuple { CELESTIAL_MODEL_KIND_DELTA_T, CELESTIAL_DELTA_T_MODEL_ALGO2, "algo2", "algo2" },
  std::tuple { CELESTIAL_MODEL_KIND_DELTA_T, CELESTIAL_DELTA_T_MODEL_ALGO3, "algo3", "algo3" },
  std::tuple { CELESTIAL_MODEL_KIND_DELTA_T, CELESTIAL_DELTA_T_MODEL_ALGO4, "algo4", "algo4" },
  std::tuple { CELESTIAL_MODEL_KIND_DELTA_T, CELESTIAL_DELTA_T_MODEL_ALGO5, "algo5", "algo5" },
  std::tuple { CELESTIAL_MODEL_KIND_NUTATION, CELESTIAL_NUTATION_MODEL_MEEUS, "meeus", "meeus" },
  std::tuple { CELESTIAL_MODEL_KIND_NUTATION, CELESTIAL_NUTATION_MODEL_IAU_1980, "iau", "1980" },
  std::tuple { CELESTIAL_MODEL_KIND_POSITION_SOURCE, CELESTIAL_POSITION_SOURCE_SUN_VSOP87D, "sun", "vsop87d" },
  std::tuple { CELESTIAL_MODEL_KIND_POSITION_SOURCE, CELESTIAL_POSITION_SOURCE_MOON_ELP2000_82B_TRUNC, "moon", "elp2000" },
  std::tuple { CELESTIAL_MODEL_KIND_POSITION_SOURCE, CELESTIAL_POSITION_SOURCE_PLANET_VSOP87D, "planet", "vsop87d" },
  std::tuple { CELESTIAL_MODEL_KIND_POSITION_SOURCE, CELESTIAL_POSITION_SOURCE_PLUTO_PLUTO95_APPARENT, "pluto", "95" },
  std::tuple { CELESTIAL_MODEL_KIND_POSITION_SOURCE, CELESTIAL_POSITION_SOURCE_NODE_MEAN, "mean", "node" },
  std::tuple { CELESTIAL_MODEL_KIND_POSITION_SOURCE, CELESTIAL_POSITION_SOURCE_NODE_TRUE_MEEUS_FIVE_TERM, "true", "five" },
};

[[nodiscard]] auto same_double(const double a, const double b) -> bool {
  return a == b or (std::isnan(a) and std::isnan(b));
}

/** @brief Check a sized-text export without assuming a fixed allocation size. */
auto expect_sized_text(const auto& query) -> std::string {
  const auto required = query(nullptr, 0);
  EXPECT_GT(required, 1U);
  EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_NONE);
  if (required <= 1) {
    return {};
  }

  std::string buffer(required + 1, '!');
  EXPECT_EQ(query(buffer.data(), required), required);
  EXPECT_EQ(buffer.find('\0'), required - 1);
  EXPECT_EQ(buffer.at(required), '!');
  const auto full = buffer.substr(0, required - 1);

  for (const uint32_t capacity : { 1U, 8U }) {
    std::string small(capacity + 1, '!');
    EXPECT_EQ(query(small.data(), capacity), required);
    const auto copied = std::min(capacity - 1, required - 1);
    EXPECT_EQ(small.at(copied), '\0');
    EXPECT_EQ(small.at(capacity), '!');
    EXPECT_EQ(small.substr(0, copied), full.substr(0, copied));
    EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_NONE);
  }
  EXPECT_EQ(query(nullptr, 1), 0U);
  EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
  return full;
}

} // namespace

TEST(CapabilityCAbi, DomainRecordsMirrorTheCore) {
  for (const uint32_t operation : TARGET_OPERATIONS) {
    uint32_t count = 0;
    std::array<CelestialCapabilityRecord, 14> slots {};
    ASSERT_EQ(celestial_capabilities(operation, CELESTIAL_TARGET_ALL, &count, slots.data(), slots.size()), slots.size());
    ASSERT_EQ(count, slots.size());
    for (std::size_t index = 0; index < slots.size(); ++index) {
      const auto& slot = slots.at(index);
      EXPECT_EQ(slot.operation_code, operation);
      EXPECT_EQ(slot.subject_code, index);
      EXPECT_EQ(slot.source_code, SOURCES.at(index));
      EXPECT_EQ(slot.maximum_span_tt_days, operation == CELESTIAL_OPERATION_EVENTS ? 3660U : 0U);
      if (operation == CELESTIAL_OPERATION_LONGITUDE_RATE or operation == CELESTIAL_OPERATION_EVENTS) {
        const auto domain = astro::ephemeris::longitude_rate_domain(static_cast<astro::ephemeris::Target>(index));
        EXPECT_EQ(slot.domain_start_jde_tt, domain.at(0));
        EXPECT_EQ(slot.domain_end_jde_tt, domain.at(1));
      } else if (slot.subject_code == CELESTIAL_TARGET_PLUTO) {
        EXPECT_EQ(slot.status_code, CELESTIAL_STATUS_SOURCE_WINDOW);
        EXPECT_EQ(slot.domain_start_jde_tt, astro::planet::geocentric_coord::PLUTO_START_JDE_TT);
        EXPECT_EQ(slot.domain_end_jde_tt, astro::planet::geocentric_coord::PLUTO_END_JDE_TT);
      } else if (slot.subject_code >= CELESTIAL_TARGET_MEAN_ASCENDING) {
        EXPECT_EQ(slot.domain_start_jde_tt, astro::lunar_node::START_JDE_TT);
        EXPECT_EQ(slot.domain_end_jde_tt, astro::lunar_node::END_JDE_TT);
      } else {
        EXPECT_EQ(slot.status_code, CELESTIAL_STATUS_FINITE_INPUT_EVALUATION);
        EXPECT_TRUE(std::isnan(slot.domain_start_jde_tt));
        EXPECT_TRUE(std::isnan(slot.domain_end_jde_tt));
      }
    }
  }
}

TEST(CapabilityCAbi, SingleSubjectAndCountOnlyProtocol) {
  uint32_t count = 99;
  CelestialCapabilityRecord slot {};
  EXPECT_EQ(celestial_capabilities(CELESTIAL_OPERATION_POSITION, CELESTIAL_TARGET_PLUTO, &count, &slot, 1), 1U);
  EXPECT_EQ(count, 1U);
  EXPECT_EQ(slot.subject_code, CELESTIAL_TARGET_PLUTO);
  EXPECT_EQ(slot.source_code, CELESTIAL_POSITION_SOURCE_PLUTO_PLUTO95_APPARENT);
  EXPECT_EQ(celestial_capabilities(CELESTIAL_OPERATION_EVENTS, CELESTIAL_TARGET_ALL, &count, nullptr, 0), 0U);
  EXPECT_EQ(count, 14U);
  EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_NONE);
  EXPECT_EQ(celestial_capabilities(CELESTIAL_OPERATION_EVENTS, CELESTIAL_TARGET_ALL, &count, &slot, 1), 1U);
  EXPECT_EQ(count, 14U);
  EXPECT_EQ(slot.subject_code, CELESTIAL_TARGET_SUN);
}

TEST(CapabilityCAbi, IdentitiesMatchNamedPairs) {
  uint32_t count = 0;
  std::array<CelestialModelIdentityRecord, IDENTITIES.size()> slots {};
  ASSERT_EQ(celestial_model_identities(CELESTIAL_MODEL_KIND_ALL, &count, slots.data(), slots.size()), slots.size());
  ASSERT_EQ(count, slots.size());
  for (std::size_t index = 0; index < slots.size(); ++index) {
    const auto& slot = slots.at(index);
    const auto& [kind, identity, name, qualifier] = IDENTITIES.at(index);
    EXPECT_EQ(slot.kind_code, kind);
    EXPECT_EQ(slot.identity_code, identity);
    EXPECT_NE(slot.identity_code, 0U);
    EXPECT_EQ(slot.status_code, kind == CELESTIAL_MODEL_KIND_DELTA_T and identity != CELESTIAL_DELTA_T_MODEL_ALGO5
                ? CELESTIAL_MODEL_STATUS_FROZEN : CELESTIAL_MODEL_STATUS_CURRENT);
    auto text = expect_sized_text([kind, identity](char* buffer, const uint32_t capacity) {
      return celestial_model_identity_text(kind, identity, buffer, capacity);
    });
    std::ranges::transform(text, text.begin(), [](const unsigned char value) {
      return static_cast<char>(std::tolower(value));
    });
    EXPECT_NE(text.find(name), std::string::npos) << text;
    EXPECT_NE(text.find(qualifier), std::string::npos) << text;
  }
  EXPECT_EQ(slots.at(0).domain_start_jde_tt, astro::delta_t::ALGO1_START_JDE_TT);
  EXPECT_EQ(slots.at(2).domain_end_jde_tt, astro::delta_t::ALGO3_END_JDE_TT);
  EXPECT_EQ(slots.at(3).domain_end_jde_tt, astro::delta_t::ALGO4_END_JDE_TT);
}

TEST(CapabilityCAbi, EveryIdentityKindIsAStableSubsetOfAll) {
  uint32_t all_count = 0;
  std::array<CelestialModelIdentityRecord, 13> all {};
  ASSERT_EQ(celestial_model_identities(CELESTIAL_MODEL_KIND_ALL, &all_count, all.data(), all.size()), all.size());
  ASSERT_EQ(all_count, all.size());
  for (const uint32_t kind : { CELESTIAL_MODEL_KIND_DELTA_T, CELESTIAL_MODEL_KIND_NUTATION, CELESTIAL_MODEL_KIND_POSITION_SOURCE }) {
    std::vector<CelestialModelIdentityRecord> expected;
    for (const auto& identity : all) {
      if (identity.kind_code == kind) {
        expected.push_back(identity);
      }
    }
    ASSERT_FALSE(expected.empty());
    for (const uint32_t capacity : { 0U, 1U, static_cast<uint32_t>(expected.size()), static_cast<uint32_t>(all.size()) }) {
      for (const uint32_t repeat : { 0U, 1U }) {
        SCOPED_TRACE(::testing::Message() << "kind=" << kind << " capacity=" << capacity << " repeat=" << repeat);
        uint32_t count = 99;
        EXPECT_EQ(celestial_model_identities(99, &count, nullptr, 0), 0U);
        EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
        std::array<CelestialModelIdentityRecord, 13> slots {};
        const auto written = celestial_model_identities(kind, &count, capacity == 0 ? nullptr : slots.data(), capacity);
        EXPECT_EQ(count, expected.size());
        ASSERT_EQ(written, std::min(capacity, static_cast<uint32_t>(expected.size())));
        EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_NONE);
        for (uint32_t index = 0; index < written; ++index) {
          const auto& actual = slots.at(index);
          const auto& reference = expected.at(index);
          EXPECT_EQ(actual.kind_code, reference.kind_code);
          EXPECT_EQ(actual.identity_code, reference.identity_code);
          EXPECT_EQ(actual.status_code, reference.status_code);
          EXPECT_TRUE(same_double(actual.domain_start_jde_tt, reference.domain_start_jde_tt));
          EXPECT_TRUE(same_double(actual.domain_end_jde_tt, reference.domain_end_jde_tt));
        }
      }
    }
  }
}

TEST(CapabilityCAbi, SelectorsAndResolvedIdentitiesMatchTheHeader) {
  for (const uint32_t delta_t : { CELESTIAL_DELTA_T_MODEL_DEFAULT, CELESTIAL_DELTA_T_MODEL_ALGO1,
    CELESTIAL_DELTA_T_MODEL_ALGO2, CELESTIAL_DELTA_T_MODEL_ALGO3, CELESTIAL_DELTA_T_MODEL_ALGO4, CELESTIAL_DELTA_T_MODEL_ALGO5 }) {
    for (const uint32_t nutation : { CELESTIAL_NUTATION_MODEL_DEFAULT, CELESTIAL_NUTATION_MODEL_MEEUS, CELESTIAL_NUTATION_MODEL_IAU_1980 }) {
      const auto result = celestial_house_query(
        2024, 3, 20, 0.5, CELESTIAL_CIVIL_SCALE_UT1, delta_t, nutation,
        51.5, -0.1, CELESTIAL_HOUSE_SYSTEM_EQUAL
      );
      ASSERT_TRUE(result.valid) << celestial_last_error();
      EXPECT_EQ(result.delta_t_identity_code, delta_t == CELESTIAL_DELTA_T_MODEL_DEFAULT ? CELESTIAL_DELTA_T_MODEL_ALGO5 : delta_t);
      EXPECT_EQ(result.nutation_identity_code, nutation == CELESTIAL_NUTATION_MODEL_DEFAULT ? CELESTIAL_NUTATION_MODEL_IAU_1980 : nutation);
    }
  }
}

TEST(CapabilityCAbi, TextExportSizersThenFills) {
  for (const uint32_t operation : TARGET_OPERATIONS) {
    const auto text = expect_sized_text([operation](char* buffer, const uint32_t capacity) {
      return celestial_capability_text(operation, CELESTIAL_TARGET_SUN, buffer, capacity);
    });
    EXPECT_NE(text.find("VSOP87D"), std::string::npos);
    if (operation == CELESTIAL_OPERATION_EQUATORIAL) {
      EXPECT_NE(text.find("RA/Dec of date"), std::string::npos);
      EXPECT_NE(text.find("fixed IAU1980"), std::string::npos);
    } else if (operation == CELESTIAL_OPERATION_POSITION) {
      EXPECT_NE(text.find("true ecliptic and equinox of date"), std::string::npos);
      EXPECT_NE(text.find("no rate stencil"), std::string::npos);
    } else if (operation == CELESTIAL_OPERATION_LONGITUDE_RATE) {
      EXPECT_NE(text.find("Seven-point centered longitude rate"), std::string::npos);
      EXPECT_NE(text.find("degrees per TT day"), std::string::npos);
    } else if (operation == CELESTIAL_OPERATION_EVENTS) {
      EXPECT_NE(text.find("Event query, maximum span 3660 TT days"), std::string::npos);
      EXPECT_NE(text.find("true-ecliptic longitude"), std::string::npos);
    }
  }
  for (const uint32_t operation : { CELESTIAL_OPERATION_HOUSES, CELESTIAL_OPERATION_CHART, CELESTIAL_OPERATION_DELTA_T }) {
    EXPECT_FALSE(expect_sized_text([operation](char* buffer, const uint32_t capacity) {
      return celestial_capability_text(operation, CELESTIAL_TARGET_ALL, buffer, capacity);
    }).empty());
  }
}

TEST(CapabilityCAbi, RejectsWrongOperationsSubjectsAndFullWidthCodes) {
  uint32_t count = 99;
  CelestialCapabilityRecord slot {};
  for (const uint32_t operation : { CELESTIAL_OPERATION_HOUSES, CELESTIAL_OPERATION_CHART,
    CELESTIAL_OPERATION_DELTA_T, 17U, 256U, 65536U, UINT32_C(0x10000000), UINT32_MAX }) {
    count = 99;
    EXPECT_EQ(celestial_capabilities(operation, CELESTIAL_TARGET_ALL, &count, &slot, 1), 0U);
    EXPECT_EQ(count, 0U);
    EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
  }
  for (const uint32_t operation : TARGET_OPERATIONS) {
    EXPECT_EQ(celestial_capability_text(operation, CELESTIAL_TARGET_ALL, nullptr, 0), 0U);
    EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
    for (const uint32_t subject : { 14U, 256U, 257U, 65536U, UINT32_C(0x10000001) }) {
      count = 99;
      EXPECT_EQ(celestial_capabilities(operation, subject, &count, &slot, 1), 0U);
      EXPECT_EQ(count, 0U);
      EXPECT_EQ(celestial_capability_text(operation, subject, nullptr, 0), 0U);
      EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
    }
  }
  for (const uint32_t operation : { CELESTIAL_OPERATION_HOUSES, CELESTIAL_OPERATION_CHART, CELESTIAL_OPERATION_DELTA_T }) {
    for (const uint32_t subject : { CELESTIAL_TARGET_SUN, CELESTIAL_TARGET_SATURN, 14U, 256U, UINT32_C(0x10000000) }) {
      EXPECT_EQ(celestial_capability_text(operation, subject, nullptr, 0), 0U);
      EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
    }
  }
  for (const uint32_t operation : { 17U, 256U, 65536U, UINT32_C(0x10000000), UINT32_MAX }) {
    EXPECT_EQ(celestial_capability_text(operation, CELESTIAL_TARGET_SUN, nullptr, 0), 0U);
    EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
  }
}

TEST(CapabilityCAbi, RejectsUnknownModelIdentityPairsWithoutNarrowing) {
  for (const uint32_t kind : { CELESTIAL_MODEL_KIND_DELTA_T, CELESTIAL_MODEL_KIND_NUTATION, CELESTIAL_MODEL_KIND_POSITION_SOURCE }) {
    for (const uint32_t identity : { 0U, 7U, 256U, 257U, 65537U, UINT32_C(0x10000001), UINT32_MAX }) {
      EXPECT_EQ(celestial_model_identity_text(kind, identity, nullptr, 0), 0U);
      EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
    }
  }
  EXPECT_EQ(celestial_model_identity_text(CELESTIAL_MODEL_KIND_NUTATION, CELESTIAL_DELTA_T_MODEL_ALGO5, nullptr, 0), 0U);
  EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
  for (const uint32_t kind : { 3U, 256U, 257U, 65536U, UINT32_C(0x10000000), CELESTIAL_MODEL_KIND_ALL }) {
    EXPECT_EQ(celestial_model_identity_text(kind, 1, nullptr, 0), 0U);
    EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
  }
}

TEST(CapabilityCAbi, FailuresLeaveDeterministicState) {
  uint32_t count = 99;
  CelestialCapabilityRecord slot {};
  EXPECT_EQ(celestial_capabilities(CELESTIAL_OPERATION_POSITION, CELESTIAL_TARGET_ALL, &count, nullptr, 1), 0U);
  EXPECT_EQ(count, 0U);
  EXPECT_EQ(celestial_capabilities(CELESTIAL_OPERATION_POSITION, CELESTIAL_TARGET_ALL, nullptr, &slot, 1), 0U);
  EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
  count = 99;
  EXPECT_EQ(celestial_model_identities(9, &count, nullptr, 0), 0U);
  EXPECT_EQ(count, 0U);
  EXPECT_EQ(celestial_model_identities(CELESTIAL_MODEL_KIND_ALL, &count, nullptr, 1), 0U);
  EXPECT_EQ(count, 0U);
  EXPECT_EQ(celestial_model_identities(CELESTIAL_MODEL_KIND_ALL, nullptr, nullptr, 0), 0U);
  EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);

  // Retain the year-3000 event probe independently of modern validation coverage.
  const double year_3000_start = 2816787.0; // ~3000-01-01, inside the Sun's rate domain
  const double sun_longitude = astro::sun::geocentric_coord::apparent(year_3000_start).λ.deg();
  uint32_t event_count = 0;
  EXPECT_EQ(celestial_longitude_events(CELESTIAL_TARGET_SUN, sun_longitude + 5.0, year_3000_start,
    year_3000_start + 300.0, &event_count, nullptr, 0), 0U);
  EXPECT_EQ(event_count, 1U);
  EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_NONE);
}

TEST(CapabilityCAbi, EventDomainEdgesAgreeWithAdmission) {
  for (uint32_t target = 0; target < 14; ++target) {
    CelestialCapabilityRecord record {};
    uint32_t record_count = 0;
    ASSERT_EQ(celestial_capabilities(CELESTIAL_OPERATION_EVENTS, target, &record_count, &record, 1), 1U);
    constexpr auto DOMAIN_REJECTION = "Event query leaves its longitude-rate domain";
    uint32_t event_count = 0;
    EXPECT_EQ(celestial_longitude_events(target, 0.0, record.domain_start_jde_tt, record.domain_start_jde_tt + 30.0,
      &event_count, nullptr, 0), 0U);
    EXPECT_FALSE(std::string_view { celestial_last_error() }.starts_with(DOMAIN_REJECTION)) << "lower edge, target " << target;
    EXPECT_EQ(celestial_longitude_events(target, 0.0, record.domain_end_jde_tt - 30.0, record.domain_end_jde_tt,
      &event_count, nullptr, 0), 0U);
    EXPECT_FALSE(std::string_view { celestial_last_error() }.starts_with(DOMAIN_REJECTION)) << "upper edge, target " << target;
    EXPECT_EQ(celestial_longitude_events(target, 0.0, record.domain_end_jde_tt, record.domain_end_jde_tt + 30.0,
      &event_count, nullptr, 0), 0U);
    EXPECT_EQ(
      std::string { celestial_last_error() },
      std::format(
        "Event query leaves its longitude-rate domain: query [{}, {}) JDE(TT), domain [{}, {}], target code {}",
        record.domain_end_jde_tt, record.domain_end_jde_tt + 30.0,
        record.domain_start_jde_tt, record.domain_end_jde_tt, target
      )
    ) << "outside upper edge, target " << target;
    EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
  }
}

} // namespace test
