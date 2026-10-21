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

#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>

#include <gtest/gtest.h>

#include "capability.hpp"

namespace astro::capability::test {

using ephemeris::Target;

// The capability layer computes every bound from the defining constants in the operation's
// own header; these tests pin the derivation, not a second table of numbers.

TEST(Capability, PositionDomainPerTarget) {
  const auto pluto = domain_record(Operation::POSITION, Target::PLUTO);
  EXPECT_EQ(pluto.status, Status::SOURCE_WINDOW);
  EXPECT_EQ(pluto.source, PositionSource::PLUTO_PLUTO95_APPARENT);
  EXPECT_DOUBLE_EQ(pluto.domain_start_jde_tt, astro::planet::geocentric_coord::PLUTO_START_JDE_TT);
  EXPECT_DOUBLE_EQ(pluto.domain_end_jde_tt, astro::planet::geocentric_coord::PLUTO_END_JDE_TT);

  for (const auto target : { Target::MEAN_ASCENDING, Target::MEAN_DESCENDING, Target::TRUE_ASCENDING, Target::TRUE_DESCENDING }) {
    const auto record = domain_record(Operation::POSITION, target);
    EXPECT_EQ(record.status, Status::IMPLEMENTATION_WINDOW);
    EXPECT_DOUBLE_EQ(record.domain_start_jde_tt, lunar_node::START_JDE_TT);
    EXPECT_DOUBLE_EQ(record.domain_end_jde_tt, lunar_node::END_JDE_TT);
  }
  EXPECT_EQ(domain_record(Operation::POSITION, Target::TRUE_ASCENDING).source,
            PositionSource::NODE_TRUE_MEEUS_FIVE_TERM);
  EXPECT_EQ(domain_record(Operation::POSITION, Target::MEAN_DESCENDING).source, PositionSource::NODE_MEAN);

  const auto sun = domain_record(Operation::POSITION, Target::SUN);
  EXPECT_EQ(sun.status, Status::UNBOUNDED_FINITE);
  EXPECT_TRUE(std::isnan(sun.domain_start_jde_tt));
  EXPECT_TRUE(std::isnan(sun.domain_end_jde_tt));
}

TEST(Capability, RateAndEventsDomainsTrackTheRateFunction) {
  for (const auto target : detail::all_targets()) {
    const auto rate = domain_record(Operation::LONGITUDE_RATE, target);
    // The center domain is always the root window shrunk by the stencil reach, so it is an
    // implementation window for every target including Pluto (the root window's own nature is
    // stated on the POSITION record).
    EXPECT_EQ(rate.status, Status::IMPLEMENTATION_WINDOW);
    const auto expected = ephemeris::longitude_rate_domain(target);
    EXPECT_DOUBLE_EQ(rate.domain_start_jde_tt, expected.front());
    EXPECT_DOUBLE_EQ(rate.domain_end_jde_tt, expected.back());
    EXPECT_EQ(rate.maximum_span_tt_days, 0);

    const auto events = domain_record(Operation::EVENTS, target);
    EXPECT_EQ(events.status, Status::IMPLEMENTATION_WINDOW);
    EXPECT_DOUBLE_EQ(events.domain_start_jde_tt, expected.front());
    EXPECT_DOUBLE_EQ(events.domain_end_jde_tt, expected.back());
    EXPECT_EQ(events.maximum_span_tt_days,
              static_cast<uint32_t>(ephemeris::detail::EVENT_MAXIMUM_QUERY_SPAN_TT_DAYS));
  }

  // The century band (J2000 +/- 100 years, about 36525 days) selects only the certified
  // error-bound table; it must not constrain the accepted domain of the +/-10 Jm targets,
  // or year-3000 event queries would be wrongly rejected (#357).
  const double century_low = 2451545.0 - 36525.0;
  const double century_high = 2451545.0 + 36525.0;
  for (const auto target : { Target::SUN, Target::MOON, Target::MERCURY, Target::VENUS, Target::MARS,
                             Target::JUPITER, Target::SATURN, Target::URANUS, Target::NEPTUNE }) {
    const auto events = domain_record(Operation::EVENTS, target);
    EXPECT_LT(events.domain_start_jde_tt, century_low);
    EXPECT_GT(events.domain_end_jde_tt, century_high);
  }
}

TEST(Capability, HousesRecordCarriesNoWindow) {
  const auto houses = domain_record(Operation::HOUSES, Target::SUN);
  EXPECT_EQ(houses.operation, Operation::HOUSES);
  EXPECT_EQ(houses.status, Status::UNBOUNDED_FINITE);
  EXPECT_TRUE(std::isnan(houses.domain_start_jde_tt));
  EXPECT_TRUE(std::isnan(houses.domain_end_jde_tt));
  EXPECT_EQ(houses.maximum_span_tt_days, 0);
}

TEST(Capability, DomainRecordsEnumerateTargets) {
  for (const auto operation : { Operation::POSITION, Operation::LONGITUDE_RATE, Operation::EVENTS }) {
    const auto records = domain_records(operation);
    ASSERT_EQ(records.size(), 14);
    for (uint32_t index = 0; index < records.size(); ++index) {
      EXPECT_EQ(records[index].subject_code, index);
    }
  }
  ASSERT_EQ(domain_records(Operation::HOUSES).size(), 1);
}

TEST(Capability, ModelIdentityRegistry) {
  const auto identities = model_identities();
  ASSERT_EQ(identities.size(), 13);

  // Delta T: five identities, Algo5 current and the default alias, Algo4 capped at 2035.
  for (uint32_t code = 0; code < 5; ++code) {
    EXPECT_EQ(identities[code].kind, ModelKind::DELTA_T);
    EXPECT_EQ(identities[code].identity_code, code);
    EXPECT_EQ(identities[code].status, code == 4 ? ModelStatus::CURRENT : ModelStatus::FROZEN);
  }
  EXPECT_EQ(identities[4].default_alias_code, DEFAULT_DELTA_T_CODE);
  EXPECT_EQ(identities[3].default_alias_code, std::numeric_limits<uint32_t>::max());
  EXPECT_TRUE(std::isnan(identities[4].domain_end_jde_tt));
  EXPECT_DOUBLE_EQ(identities[3].domain_end_jde_tt, astro::delta_t::ALGO4_END_JDE_TT);
  EXPECT_DOUBLE_EQ(identities[0].domain_start_jde_tt, astro::delta_t::ALGO1_START_JDE_TT);
  EXPECT_TRUE(std::isnan(identities[0].domain_end_jde_tt));
  EXPECT_TRUE(std::isnan(identities[2].domain_start_jde_tt));
  EXPECT_DOUBLE_EQ(identities[2].domain_end_jde_tt, astro::delta_t::ALGO3_END_JDE_TT);

  // Nutation: Meeus and IAU 1980, both current.
  EXPECT_EQ(identities[5].kind, ModelKind::NUTATION);
  EXPECT_EQ(identities[6].kind, ModelKind::NUTATION);

  // Position sources: six identities (ordinals 7-12); Pluto and the nodes carry their
  // provider windows, the others are unbounded.
  for (uint32_t ordinal = 7; ordinal < 13; ++ordinal) {
    EXPECT_EQ(identities[ordinal].kind, ModelKind::POSITION_SOURCE);
    EXPECT_EQ(identities[ordinal].identity_code, ordinal - 7);
  }
  EXPECT_TRUE(std::isnan(identities[7].domain_end_jde_tt));   // Sun: unbounded
  EXPECT_TRUE(std::isnan(identities[8].domain_end_jde_tt));   // Moon: unbounded
  EXPECT_TRUE(std::isnan(identities[9].domain_end_jde_tt));   // planets: unbounded
  EXPECT_DOUBLE_EQ(identities[10].domain_start_jde_tt, astro::planet::geocentric_coord::PLUTO_START_JDE_TT);
  EXPECT_DOUBLE_EQ(identities[10].domain_end_jde_tt, astro::planet::geocentric_coord::PLUTO_END_JDE_TT);
  EXPECT_DOUBLE_EQ(identities[11].domain_start_jde_tt, lunar_node::START_JDE_TT);
  EXPECT_DOUBLE_EQ(identities[12].domain_end_jde_tt, lunar_node::END_JDE_TT);
}

TEST(Capability, DefaultAliasResolvesLikeTheBoundarySwitch) {
  // The C ABI switch maps DEFAULT(0) and ALGO5(5) to Algo5; the canonical table must agree.
  EXPECT_EQ(resolve_delta_t_code(DEFAULT_DELTA_T_CODE), astro::delta_t::Model::ALGO5);
  EXPECT_EQ(resolve_delta_t_code(5), astro::delta_t::Model::ALGO5);
  for (uint32_t code = 1; code <= 4; ++code) {
    EXPECT_EQ(resolve_delta_t_code(code), static_cast<astro::delta_t::Model>(code - 1));
  }
  EXPECT_EQ(resolve_delta_t_code(6), std::nullopt);
  EXPECT_EQ(resolve_delta_t_code(std::numeric_limits<uint32_t>::max()), std::nullopt);
}

TEST(Capability, DescribeCoversEveryRecord) {
  for (const auto operation : { Operation::POSITION, Operation::LONGITUDE_RATE, Operation::EVENTS }) {
    for (const auto target : detail::all_targets()) {
      const auto text = describe(operation, static_cast<uint8_t>(target));
      EXPECT_FALSE(text.empty());
      EXPECT_NE(text.find("Role:"), std::string::npos);
      // POSITION names its measured coverage "Validation:"; rate and events hang the same
      // coverage prose on an explicit "Position coverage:" slot so residuals are not misread
      // as rate validation.
      EXPECT_TRUE(text.contains("Validation:") or text.contains("Position coverage:"));
    }
  }
  EXPECT_NE(describe(Operation::HOUSES, 0).find("No JDE window"), std::string::npos);

  for (uint8_t ordinal = 0; ordinal < 13; ++ordinal) {
    EXPECT_FALSE(describe_identity(ordinal).empty());
  }
  EXPECT_THROW((void)describe_identity(13), std::invalid_argument);
  EXPECT_THROW((void)describe(static_cast<Operation>(4), 0), std::invalid_argument);
}

} // namespace astro::capability::test
