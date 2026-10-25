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

#pragma once

#include <array>
#include <cstdint>
#include <format>
#include <optional>
#include <stdexcept>
#include <vector>

#include "ephemeris.hpp"
#include "ephemeris_event_facts.hpp"

namespace astro::capability {

/** @brief A target operation whose declared input domain is expressed in JDE(TT). */
enum class Operation : uint8_t { POSITION, LONGITUDE_RATE, EVENTS, EQUATORIAL };

/** @brief The origin of an admission rule, independent of numerical accuracy. */
enum class Status : uint8_t {
  SOURCE_WINDOW,
  IMPLEMENTATION_WINDOW,
  FINITE_INPUT_EVALUATION, ///< No declared hard interval; finite evaluation can still fail.
};

/** @brief Typed domain facts; an absent interval is not a promise of successful evaluation. */
struct Record {
  Operation operation;
  ephemeris::Target target;
  Status status;
  ephemeris::PositionSource source;
  std::optional<std::array<double, 2>> domain;
  std::optional<double> maximum_span_tt_days;
};

/** @brief Compose a target operation's facts without evaluating a position or event. */
[[nodiscard]] inline auto domain_record(const Operation operation, const ephemeris::Target target) -> Record {
  const auto source = ephemeris::source_of(target);
  using enum Operation;
  switch (operation) {
    case POSITION:
    case EQUATORIAL: {
      const auto domain = ephemeris::position_domain(target);
      const auto bounded_status = ephemeris::target_facts(target).position_domain_origin == ephemeris::DomainOrigin::SOURCE
        ? Status::SOURCE_WINDOW : Status::IMPLEMENTATION_WINDOW;
      return {
        .operation = operation,
        .target = target,
        .status = domain.has_value() ? bounded_status : Status::FINITE_INPUT_EVALUATION,
        .source = source,
        .domain = domain,
        .maximum_span_tt_days = std::nullopt,
      };
    }
    case LONGITUDE_RATE:
    case EVENTS:
      return {
        .operation = operation,
        .target = target,
        .status = Status::IMPLEMENTATION_WINDOW,
        .source = source,
        .domain = ephemeris::longitude_rate_domain(target),
        .maximum_span_tt_days = operation == EVENTS
          ? std::optional { ephemeris::EVENT_MAXIMUM_QUERY_SPAN_TT_DAYS } : std::nullopt,
      };
  }
  throw std::invalid_argument { std::format("Unknown TT capability operation {}", static_cast<uint32_t>(operation)) };
}

/** @brief Return one typed domain record for each target in the core roster. */
[[nodiscard]] inline auto domain_records(const Operation operation) -> std::vector<Record> {
  auto records = std::vector<Record>();
  records.reserve(ephemeris::TARGET_FACTS.size());
  for (const auto& facts : ephemeris::TARGET_FACTS) {
    records.push_back(domain_record(operation, facts.target));
  }
  return records;
}

} // namespace astro::capability
