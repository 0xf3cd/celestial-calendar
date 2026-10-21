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
#include <cstdint>
#include <format>
#include <ranges>
#include <string>

#include "lib.hpp"
#include "celestial.h"

#include "capability.hpp"

namespace {

using astro::capability::Operation;

auto check_record_count_ptr(uint32_t * const record_count) -> void {
  if (record_count == nullptr) {
    throw std::invalid_argument { "Argument `record_count` is null." };
  }
  // Deterministic out-parameter state: every failure path below leaves *record_count == 0.
  *record_count = 0;
}

auto check_slots(const auto * const slots, const uint32_t slot_count) -> void {
  if (slots == nullptr and slot_count > 0) {
    throw std::invalid_argument { "Argument `slots` is null, but `slot_count` is greater than 0." };
  }
}

auto operation_of(const uint32_t code) -> Operation {
  switch (code) {
    case CAPABILITY_OPERATION_POSITION: return Operation::POSITION;
    case CAPABILITY_OPERATION_LONGITUDE_RATE: return Operation::LONGITUDE_RATE;
    case CAPABILITY_OPERATION_EVENTS: return Operation::EVENTS;
    case CAPABILITY_OPERATION_HOUSES: return Operation::HOUSES;
    default:
      throw std::invalid_argument { std::format("Argument `operation_code` must be a CAPABILITY_OPERATION_* code in [0, {}], got {}", CAPABILITY_OPERATION_HOUSES, code) };
  }
}

auto status_code_of(const astro::capability::Status status) -> uint32_t {
  switch (status) {
    case astro::capability::Status::SOURCE_WINDOW: return CAPABILITY_STATUS_SOURCE_WINDOW;
    case astro::capability::Status::IMPLEMENTATION_WINDOW: return CAPABILITY_STATUS_IMPLEMENTATION_WINDOW;
    case astro::capability::Status::UNBOUNDED_FINITE: return CAPABILITY_STATUS_UNBOUNDED_FINITE;
  }
  throw std::runtime_error { "Unsupported capability status" };
}

auto source_code_of(const astro::capability::PositionSource source) -> uint32_t {
  return static_cast<uint32_t>(source); // The C CAPABILITY_SOURCE_* codes mirror the C++ enum order.
}

auto to_c_record(const astro::capability::Record& record) -> CapabilityRecordV1 {
  return { .operation_code = static_cast<uint32_t>(record.operation),
           .subject_code = record.subject_code,
           .status_code = status_code_of(record.status),
           .source_code = source_code_of(record.source),
           .domain_start_jde_tt = record.domain_start_jde_tt,
           .domain_end_jde_tt = record.domain_end_jde_tt,
           .maximum_span_tt_days = record.maximum_span_tt_days,
           .flags = 0 };
}

auto check_subject_code(const uint32_t subject_code) -> void {
  if (subject_code != CAPABILITY_TARGET_ALL and subject_code >= astro::capability::detail::target_count()) {
    throw std::invalid_argument {
      std::format("Argument `subject_code` must be a CHART_TARGET_* code in [0, 13] or CAPABILITY_TARGET_ALL, got {}", subject_code)
    };
  }
}

} // namespace


extern "C" {

// #357: the capability exports. Every bound is computed from the defining constants in the
// operation's own header, so these queries accept and reject exactly what the computations do.

auto celestial_capabilities(
  const uint32_t operation_code,
  const uint32_t subject_code,
  uint32_t * const record_count,
  CapabilityRecordV1 * const slots,
  const uint32_t slot_count
) -> uint32_t {
  return lib::wrap_export(__func__, [=] {
    check_record_count_ptr(record_count);
    check_slots(slots, slot_count);
    const auto operation = operation_of(operation_code);
    check_subject_code(subject_code);

    auto records = astro::capability::domain_records(operation);
    // HOUSES carries a single record whose subject is documented as unused; the target filter
    // applies to the per-target operations only.
    if (operation != Operation::HOUSES and subject_code != CAPABILITY_TARGET_ALL) {
      const auto position = std::ranges::find(records, subject_code, &astro::capability::Record::subject_code);
      if (position == records.end()) {
        throw std::invalid_argument { std::format("Operation {} has no subject {}", operation_code, subject_code) };
      }
      records = { *position };
    }

    *record_count = static_cast<uint32_t>(records.size());
    const auto num_written = std::min(static_cast<uint32_t>(records.size()), slot_count);
    std::transform(
      records.cbegin(), records.cbegin() + num_written, slots,
      [](const auto& record) { return to_c_record(record); }
    );
    return num_written;
  });
}

auto celestial_model_identities(
  const uint32_t kind_code,
  uint32_t * const record_count,
  ModelIdentityRecordV1 * const slots,
  const uint32_t slot_count
) -> uint32_t {
  return lib::wrap_export(__func__, [=] {
    check_record_count_ptr(record_count);
    check_slots(slots, slot_count);
    if (kind_code != MODEL_KIND_ALL and kind_code > MODEL_KIND_POSITION_SOURCE) {
      throw std::invalid_argument {
        std::format("Argument `kind_code` must be a MODEL_KIND_* code in [0, {}] or MODEL_KIND_ALL, got {}", MODEL_KIND_POSITION_SOURCE, kind_code)
      };
    }

    auto identities = astro::capability::model_identities();
    if (kind_code != MODEL_KIND_ALL) {
      const auto kind = static_cast<astro::capability::ModelKind>(kind_code);
      auto kept = identities | std::views::filter([kind](const auto& identity) { return identity.kind == kind; });
      identities.assign(kept.begin(), kept.end());
    }

    *record_count = static_cast<uint32_t>(identities.size());
    const auto num_written = std::min(static_cast<uint32_t>(identities.size()), slot_count);
    std::transform(
      identities.cbegin(), identities.cbegin() + num_written, slots,
      [](const auto& identity) {
        return ModelIdentityRecordV1 {
          .kind_code = static_cast<uint32_t>(identity.kind),
          .identity_code = identity.identity_code,
          .status_code = identity.status == astro::capability::ModelStatus::CURRENT ? MODEL_STATUS_CURRENT : MODEL_STATUS_FROZEN,
          .default_alias_code = identity.default_alias_code,
          .domain_start_jde_tt = identity.domain_start_jde_tt,
          .domain_end_jde_tt = identity.domain_end_jde_tt,
        };
      }
    );
    return num_written;
  });
}

// NOLINTBEGIN(bugprone-easily-swappable-parameters): operation/subject is the published text-lookup pair.
auto celestial_capability_text(
  const uint32_t operation_code,
  const uint32_t subject_code,
  char * const buffer,
  const uint32_t capacity
) -> uint32_t {
  return lib::wrap_export(__func__, [=] {
    if (buffer == nullptr and capacity > 0) {
      throw std::invalid_argument { "Argument `buffer` is null, but `capacity` is greater than 0." };
    }

    std::string text;
    if (operation_code == CAPABILITY_OPERATION_MODEL_IDENTITY) {
      // Validate before narrowing: a truncated code must not silently return another subject's text.
      if (subject_code >= astro::capability::model_identities().size()) {
        throw std::invalid_argument { std::format("Unknown model identity {}", subject_code) };
      }
      text = astro::capability::describe_identity(static_cast<uint8_t>(subject_code));
    } else {
      const auto operation = operation_of(operation_code);
      // Validate before narrowing so an out-of-range code fails instead of returning a wrapped
      // subject's text. The predicate mirrors celestial_capabilities: HOUSES ignores the subject
      // but accepts only a valid target code or CAPABILITY_TARGET_ALL; every other domain
      // operation names a target and rejects >= 14 (CAPABILITY_TARGET_ALL included).
      const bool houses_ok = operation == Operation::HOUSES
                             and (subject_code == CAPABILITY_TARGET_ALL
                                  or subject_code < astro::capability::detail::target_count());
      if (not houses_ok
          and (operation != Operation::HOUSES or subject_code != CAPABILITY_TARGET_ALL)
          and subject_code >= astro::capability::detail::target_count()) {
        throw std::invalid_argument { std::format("Unknown ephemeris target {}", subject_code) };
      }
      text = astro::capability::describe(operation, static_cast<uint8_t>(subject_code));
    }

    const auto required = static_cast<uint32_t>(text.size());
    if (buffer != nullptr and capacity > 0) {
      const auto writable = std::min<std::uint64_t>(static_cast<std::uint64_t>(capacity) - 1, text.size());
      std::copy_n(text.c_str(), writable, buffer);
      buffer[writable] = '\0'; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic) -- caller-owned buffer, offset bounded above.
    }
    return required;
  });
}
// NOLINTEND(bugprone-easily-swappable-parameters)

} // extern "C"
