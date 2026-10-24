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
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "celestial.h"

#include "capability.hpp"
#include "ephemeris.hpp"
#include "sun.hpp"

namespace test {

// #357: the capability exports must accept and reject exactly what the computations they
// describe accept and reject; the record streams mirror the C++ layer field for field.

[[nodiscard]] auto same_double(const double a, const double b) -> bool {
  return a == b or (std::isnan(a) and std::isnan(b));
}

TEST(CapabilityCAbi, DomainRecordsMirrorTheCore) {
  for (const uint32_t operation : { CAPABILITY_OPERATION_POSITION, CAPABILITY_OPERATION_LONGITUDE_RATE,
                                    CAPABILITY_OPERATION_EVENTS }) {
    uint32_t count = 0;
    std::array<CapabilityRecordV1, 14> slots {};
    const auto written = celestial_capabilities(operation, CAPABILITY_TARGET_ALL, &count, slots.data(), slots.size());
    EXPECT_EQ(written, 14);
    EXPECT_EQ(count, 14);

    const auto core_records = astro::capability::domain_records(static_cast<astro::capability::Operation>(operation));
    for (uint32_t index = 0; index < 14; ++index) {
      const auto& slot = slots.at(index);
      const auto& core = core_records.at(index);
      EXPECT_EQ(slot.operation_code, operation);
      EXPECT_EQ(slot.subject_code, index);
      EXPECT_TRUE(same_double(slot.domain_start_jde_tt, core.domain_start_jde_tt));
      EXPECT_TRUE(same_double(slot.domain_end_jde_tt, core.domain_end_jde_tt));
      EXPECT_EQ(slot.maximum_span_tt_days, core.maximum_span_tt_days);
    }
  }
}

TEST(CapabilityCAbi, SingleSubjectAndHouses) {
  uint32_t count = 0;
  CapabilityRecordV1 slot {};
  EXPECT_EQ(celestial_capabilities(CAPABILITY_OPERATION_POSITION, CHART_TARGET_PLUTO, &count, &slot, 1), 1);
  EXPECT_EQ(count, 1);
  EXPECT_EQ(slot.status_code, CAPABILITY_STATUS_SOURCE_WINDOW);
  EXPECT_DOUBLE_EQ(slot.domain_start_jde_tt, astro::planet::geocentric_coord::PLUTO_START_JDE_TT);

  // HOUSES: one record, subject filter ignored, no JDE window.
  std::array<CapabilityRecordV1, 4> house_slots {};
  EXPECT_EQ(celestial_capabilities(CAPABILITY_OPERATION_HOUSES, CAPABILITY_TARGET_ALL, &count, house_slots.data(), 4), 1);
  EXPECT_EQ(count, 1);
  EXPECT_EQ(house_slots[0].status_code, CAPABILITY_STATUS_UNBOUNDED_FINITE);
  EXPECT_TRUE(std::isnan(house_slots[0].domain_start_jde_tt));
  EXPECT_EQ(celestial_capabilities(CAPABILITY_OPERATION_HOUSES, CHART_TARGET_SATURN, &count, &slot, 1), 1);

  // slot_count 0 is a count-only pass.
  EXPECT_EQ(celestial_capabilities(CAPABILITY_OPERATION_EVENTS, CAPABILITY_TARGET_ALL, &count, nullptr, 0), 0);
  EXPECT_EQ(count, 14);
}

TEST(CapabilityCAbi, IdentitiesMirrorTheCore) {
  uint32_t count = 0;
  std::array<ModelIdentityRecordV1, 13> slots {};
  EXPECT_EQ(celestial_model_identities(MODEL_KIND_ALL, &count, slots.data(), slots.size()), 13);
  EXPECT_EQ(count, 13);

  const auto core = astro::capability::model_identities();
  for (uint32_t index = 0; index < 13; ++index) {
    const auto& slot = slots.at(index);
    const auto& identity = core.at(index);
    EXPECT_EQ(slot.kind_code, static_cast<uint32_t>(identity.kind));
    EXPECT_EQ(slot.identity_code, identity.identity_code);
    EXPECT_EQ(slot.status_code,
              identity.status == astro::capability::ModelStatus::CURRENT ? MODEL_STATUS_CURRENT : MODEL_STATUS_FROZEN);
    EXPECT_EQ(slot.default_alias_code, identity.default_alias_code);
  }

  // Kind filtering keeps the registry order.
  EXPECT_EQ(celestial_model_identities(MODEL_KIND_NUTATION, &count, slots.data(), slots.size()), 2);
  EXPECT_EQ(count, 2);
  EXPECT_EQ(slots[0].kind_code, MODEL_KIND_NUTATION);
  EXPECT_EQ(slots[1].kind_code, MODEL_KIND_NUTATION);
}

TEST(CapabilityCAbi, EveryIdentityKindIsAStableSubsetOfAll) {
  uint32_t all_count = 0;
  std::array<ModelIdentityRecordV1, 13> all {};
  ASSERT_EQ(celestial_model_identities(MODEL_KIND_ALL, &all_count, all.data(), all.size()), all.size());
  ASSERT_EQ(all_count, all.size());
  for (const uint32_t kind : { MODEL_KIND_DELTA_T, MODEL_KIND_NUTATION, MODEL_KIND_POSITION_SOURCE }) {
    std::vector<ModelIdentityRecordV1> expected;
    for (const auto& identity : all) {
      if (identity.kind_code == kind) {
        expected.push_back(identity);
      }
    }
    ASSERT_FALSE(expected.empty());
    for (const uint32_t capacity : { uint32_t { 0 }, uint32_t { 1 }, static_cast<uint32_t>(expected.size()),
                                      static_cast<uint32_t>(all.size()) }) {
      for (const uint32_t repeat : { uint32_t { 0 }, uint32_t { 1 } }) {
        SCOPED_TRACE(::testing::Message() << "kind=" << kind << " capacity=" << capacity << " repeat=" << repeat);
        uint32_t count = 99;
        EXPECT_EQ(celestial_model_identities(99, &count, nullptr, 0), 0);
        EXPECT_STRNE(last_error(), "");
        std::array<ModelIdentityRecordV1, 13> slots {};
        const auto written = celestial_model_identities(kind, &count, capacity == 0 ? nullptr : slots.data(), capacity);
        EXPECT_EQ(count, expected.size());
        ASSERT_EQ(written, std::min(capacity, static_cast<uint32_t>(expected.size())));
        EXPECT_STREQ(last_error(), "");
        for (uint32_t index = 0; index < written; ++index) {
          const auto& actual = slots.at(index);
          const auto& reference = expected.at(index);
          EXPECT_EQ(actual.kind_code, reference.kind_code);
          EXPECT_EQ(actual.identity_code, reference.identity_code);
          EXPECT_EQ(actual.status_code, reference.status_code);
          EXPECT_EQ(actual.default_alias_code, reference.default_alias_code);
          EXPECT_TRUE(same_double(actual.domain_start_jde_tt, reference.domain_start_jde_tt));
          EXPECT_TRUE(same_double(actual.domain_end_jde_tt, reference.domain_end_jde_tt));
        }
      }
    }
  }
}

TEST(CapabilityCAbi, SelectorsAndResolvedIdentitiesMatchTheHeader) {
  using astro::capability::DeltaTSelector;
  using astro::capability::NutationSelector;
  using DeltaT = astro::delta_t::Model;
  using Nutation = astro::earth::nutation::Model;

  const std::array<std::tuple<uint32_t, DeltaTSelector, DeltaT, uint32_t>, 6> delta_t_cases {{
    { CHART_DELTA_T_MODEL_DEFAULT, DeltaTSelector::DEFAULT, DeltaT::ALGO5, MODEL_IDENTITY_DELTA_T_ALGO5 },
    { CHART_DELTA_T_MODEL_ALGO1,   DeltaTSelector::ALGO1,   DeltaT::ALGO1, MODEL_IDENTITY_DELTA_T_ALGO1 },
    { CHART_DELTA_T_MODEL_ALGO2,   DeltaTSelector::ALGO2,   DeltaT::ALGO2, MODEL_IDENTITY_DELTA_T_ALGO2 },
    { CHART_DELTA_T_MODEL_ALGO3,   DeltaTSelector::ALGO3,   DeltaT::ALGO3, MODEL_IDENTITY_DELTA_T_ALGO3 },
    { CHART_DELTA_T_MODEL_ALGO4,   DeltaTSelector::ALGO4,   DeltaT::ALGO4, MODEL_IDENTITY_DELTA_T_ALGO4 },
    { CHART_DELTA_T_MODEL_ALGO5,   DeltaTSelector::ALGO5,   DeltaT::ALGO5, MODEL_IDENTITY_DELTA_T_ALGO5 },
  }};
  const std::array<std::tuple<uint32_t, NutationSelector, Nutation, uint32_t>, 3> nutation_cases {{
    { HOUSE_NUTATION_MODEL_DEFAULT,  NutationSelector::DEFAULT,  Nutation::IAU_1980, MODEL_IDENTITY_NUTATION_IAU1980 },
    { HOUSE_NUTATION_MODEL_MEEUS,    NutationSelector::MEEUS,    Nutation::MEEUS,    MODEL_IDENTITY_NUTATION_MEEUS   },
    { HOUSE_NUTATION_MODEL_IAU_1980, NutationSelector::IAU_1980, Nutation::IAU_1980, MODEL_IDENTITY_NUTATION_IAU1980 },
  }};

  for (const auto& [delta_t_code, delta_t_selector, delta_t_model, delta_t_identity] : delta_t_cases) {
    EXPECT_EQ(std::to_underlying(delta_t_selector), delta_t_code);
    EXPECT_EQ(astro::capability::resolve_delta_t_code(delta_t_code), delta_t_model);
    EXPECT_EQ(std::to_underlying(delta_t_model), delta_t_identity);
    for (const auto& [nutation_code, nutation_selector, nutation_model, nutation_identity] : nutation_cases) {
      EXPECT_EQ(std::to_underlying(nutation_selector), nutation_code);
      EXPECT_EQ(astro::capability::resolve_nutation_code(nutation_code), nutation_model);
      EXPECT_EQ(std::to_underlying(nutation_model), nutation_identity);
      const auto result = house_query_v1(
        2024, 3, 20, 0.5, CHART_CIVIL_SCALE_UT1, delta_t_code, nutation_code,
        51.5, -0.1, CHART_HOUSE_SYSTEM_EQUAL
      );
      ASSERT_TRUE(result.valid) << last_error();
      EXPECT_EQ(result.delta_t_identity_code, delta_t_identity);
      EXPECT_EQ(result.nutation_identity_code, nutation_identity);
    }
  }
}

TEST(CapabilityCAbi, CapabilityEnumValuesMatchTheHeader) {
  using namespace astro::capability;
  EXPECT_EQ(std::to_underlying(Operation::POSITION), CAPABILITY_OPERATION_POSITION);
  EXPECT_EQ(std::to_underlying(Operation::LONGITUDE_RATE), CAPABILITY_OPERATION_LONGITUDE_RATE);
  EXPECT_EQ(std::to_underlying(Operation::EVENTS), CAPABILITY_OPERATION_EVENTS);
  EXPECT_EQ(std::to_underlying(Operation::HOUSES), CAPABILITY_OPERATION_HOUSES);
  EXPECT_EQ(std::to_underlying(Status::SOURCE_WINDOW), CAPABILITY_STATUS_SOURCE_WINDOW);
  EXPECT_EQ(std::to_underlying(Status::IMPLEMENTATION_WINDOW), CAPABILITY_STATUS_IMPLEMENTATION_WINDOW);
  EXPECT_EQ(std::to_underlying(Status::UNBOUNDED_FINITE), CAPABILITY_STATUS_UNBOUNDED_FINITE);
  EXPECT_EQ(std::to_underlying(ModelKind::DELTA_T), MODEL_KIND_DELTA_T);
  EXPECT_EQ(std::to_underlying(ModelKind::NUTATION), MODEL_KIND_NUTATION);
  EXPECT_EQ(std::to_underlying(ModelKind::POSITION_SOURCE), MODEL_KIND_POSITION_SOURCE);
  EXPECT_EQ(std::to_underlying(ModelStatus::CURRENT), MODEL_STATUS_CURRENT);
  EXPECT_EQ(std::to_underlying(ModelStatus::FROZEN), MODEL_STATUS_FROZEN);
  EXPECT_EQ(std::to_underlying(PositionSource::SUN_VSOP87D), CAPABILITY_SOURCE_SUN_VSOP87D);
  EXPECT_EQ(std::to_underlying(PositionSource::MOON_ELP2000_82B_TRUNC), CAPABILITY_SOURCE_MOON_ELP2000_82B_TRUNC);
  EXPECT_EQ(std::to_underlying(PositionSource::PLANET_VSOP87D), CAPABILITY_SOURCE_PLANET_VSOP87D);
  EXPECT_EQ(std::to_underlying(PositionSource::PLUTO_PLUTO95_APPARENT), CAPABILITY_SOURCE_PLUTO_PLUTO95_APPARENT);
  EXPECT_EQ(std::to_underlying(PositionSource::NODE_MEAN), CAPABILITY_SOURCE_NODE_MEAN);
  EXPECT_EQ(std::to_underlying(PositionSource::NODE_TRUE_MEEUS_FIVE_TERM), CAPABILITY_SOURCE_NODE_TRUE_MEEUS_FIVE_TERM);
}

TEST(CapabilityCAbi, TextExportSizersThenFills) {
  const auto required = celestial_capability_text(CAPABILITY_OPERATION_POSITION, CHART_TARGET_SUN, nullptr, 0);
  EXPECT_GT(required, 0);

  std::string buffer(required + 1, '\0');
  EXPECT_EQ(celestial_capability_text(CAPABILITY_OPERATION_POSITION, CHART_TARGET_SUN, buffer.data(), buffer.size()),
            required);
  EXPECT_EQ(buffer.size(), required + 1);
  EXPECT_EQ(buffer.find('\0'), required);
  EXPECT_NE(buffer.find("VSOP87D"), std::string::npos);
  EXPECT_NE(buffer.find("Role:"), std::string::npos);

  // Truncation still null-terminates within capacity.
  std::string small(8, '\0');
  EXPECT_EQ(celestial_capability_text(CAPABILITY_OPERATION_POSITION, CHART_TARGET_SUN, small.data(), small.size()),
            required);
  EXPECT_EQ(small.back(), '\0');

  const auto identity_required = celestial_capability_text(CAPABILITY_OPERATION_MODEL_IDENTITY, 4, nullptr, 0);
  std::string identity(identity_required + 1, '\0');
  EXPECT_EQ(celestial_capability_text(CAPABILITY_OPERATION_MODEL_IDENTITY, 4, identity.data(), identity.size()),
            identity_required);
  EXPECT_NE(identity.find("algo5"), std::string::npos);
  EXPECT_NE(identity.find("DEFAULT"), std::string::npos);
}

TEST(CapabilityCAbi, FailuresLeaveDeterministicState) {
  uint32_t count = 99;
  CapabilityRecordV1 slot {};

  EXPECT_EQ(celestial_capabilities(17, CAPABILITY_TARGET_ALL, &count, &slot, 1), 0);
  EXPECT_EQ(count, 0);

  count = 99;
  EXPECT_EQ(celestial_capabilities(CAPABILITY_OPERATION_POSITION, 14, &count, &slot, 1), 0);
  EXPECT_EQ(count, 0);

  count = 99;
  EXPECT_EQ(celestial_capabilities(CAPABILITY_OPERATION_POSITION, CAPABILITY_TARGET_ALL, &count, nullptr, 1), 0);
  EXPECT_EQ(count, 0);

  count = 99;
  EXPECT_EQ(celestial_model_identities(9, &count, nullptr, 0), 0);
  EXPECT_EQ(count, 0);

  count = 99;
  EXPECT_EQ(celestial_capabilities(CAPABILITY_OPERATION_POSITION, CAPABILITY_TARGET_ALL, nullptr, &slot, 1), 0);

  // The year-3000 probe (#357): the capability domain for the +/-10 Jm targets covers inputs far
  // outside the J2000 +/- 100-year error-bound band, and the event machinery accepts them. The
  // queried longitude is five degrees ahead of the Sun's longitude at the window start, so a
  // crossing is guaranteed inside the window.
  const double year_3000_start = 2816787.0;  // ~3000-01-01, inside the Sun's rate domain
  const double sun_longitude = astro::sun::geocentric_coord::apparent(year_3000_start).λ.deg();
  uint32_t event_count = 0;
  EXPECT_EQ(longitude_events(EVENT_TARGET_SUN, sun_longitude + 5.0, year_3000_start,
                             year_3000_start + 300.0, &event_count, nullptr, 0),
            0);  // count-only pass writes no slots
  EXPECT_EQ(event_count, 1);
  EXPECT_STREQ(last_error(), "");

  // The computation/capability agreement probe (#357): a query just inside the declared domain
  // — at both edges — must not be rejected as leaving the domain, and a query at or past the
  // declared end must be. Runtime evaluation failures are a separate failure taxonomy (legal
  // anywhere, including deep inside a domain), so the inside-edge assertions compare the error
  // message instead of requiring success; a too-wide declared domain would surface the domain
  // rejection there and turn them red.
  for (uint32_t target = 0; target < 14; ++target) {
    CapabilityRecordV1 record {};
    uint32_t record_count = 0;
    ASSERT_EQ(celestial_capabilities(CAPABILITY_OPERATION_EVENTS, target, &record_count, &record, 1), 1);

    constexpr auto DOMAIN_REJECTION = "Event query leaves its longitude-rate domain";
    uint32_t event_count = 0;
    EXPECT_EQ(longitude_events(target, 0.0, record.domain_start_jde_tt, record.domain_start_jde_tt + 30.0,
                               &event_count, nullptr, 0),
              0);
    EXPECT_STRNE(last_error(), DOMAIN_REJECTION) << "lower edge, target " << target;

    EXPECT_EQ(longitude_events(target, 0.0, record.domain_end_jde_tt - 30.0, record.domain_end_jde_tt,
                               &event_count, nullptr, 0),
              0);
    EXPECT_STRNE(last_error(), DOMAIN_REJECTION) << "upper edge, target " << target;

    EXPECT_EQ(longitude_events(target, 0.0, record.domain_end_jde_tt, record.domain_end_jde_tt + 30.0,
                               &event_count, nullptr, 0),
              0);
    EXPECT_STREQ(last_error(), DOMAIN_REJECTION) << "outside upper edge, target " << target;
  }

  // The text export validates its subject before narrowing: an out-of-range code must fail,
  // never return a wrapped subject's text. HOUSES ignores the subject but accepts only a valid
  // target code or CAPABILITY_TARGET_ALL, mirroring celestial_capabilities.
  EXPECT_EQ(celestial_capability_text(CAPABILITY_OPERATION_POSITION, 256, nullptr, 0), 0);
  EXPECT_EQ(celestial_capability_text(CAPABILITY_OPERATION_MODEL_IDENTITY, 256, nullptr, 0), 0);
  EXPECT_EQ(celestial_capability_text(CAPABILITY_OPERATION_POSITION, 14, nullptr, 0), 0);
  EXPECT_EQ(celestial_capability_text(CAPABILITY_OPERATION_HOUSES, 256, nullptr, 0), 0);
  EXPECT_GT(celestial_capability_text(CAPABILITY_OPERATION_HOUSES, CAPABILITY_TARGET_ALL, nullptr, 0), 0);
}

} // namespace test
