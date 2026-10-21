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
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
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
