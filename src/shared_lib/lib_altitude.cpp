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
#include <functional>
#include <limits>
#include <span>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <vector>

#include "altitude_event_query.hpp"
#include "celestial.h"
#include "lib.hpp"
#include "lib_codes.hpp"

namespace {

namespace query = astro::observer::detail::altitude_events;

static_assert(sizeof(CelestialAltitudeCondition) == 40);
static_assert(sizeof(CelestialAltitudeRecord) == 88);
static_assert(std::is_trivially_copy_assignable_v<CelestialAltitudeRecord>);

/** @brief Physical-target source identities, reconciled with both owners at compile time. */
constexpr auto SOURCE_CODES = std::invoke([] {
  std::array<uint32_t, 10> sources {};
  for (uint32_t target = 0; target < sources.size(); ++target) {
    sources.at(target) = lib::codes::code_of(astro::ephemeris::source_of(static_cast<astro::ephemeris::Target>(target)));
  }
  return sources;
});

/** @brief Decode the bounded complete list before invoking any provider. */
[[nodiscard]] auto requests(const CelestialAltitudeCondition* inputs, const uint32_t count) -> std::vector<query::Request> {
  if (inputs == nullptr and count > 0) {
    throw std::invalid_argument { "A nonempty condition list requires input storage" };
  }
  if (count > query::MAX_CONDITIONS) {
    throw std::runtime_error { "The condition-list work budget is exhausted" };
  }

  std::vector<query::Request> result(count);
  const auto conditions = std::span { inputs, count };
  for (uint32_t index = 0; index < count; ++index) {
    const auto& input = conditions[index];
    if (input.sense_code > 1 or input.refraction_code > 2 or input.reserved != 0) {
      throw std::invalid_argument { "Invalid condition selector or reserved storage" };
    }
    if (input.refraction_code == CELESTIAL_REFRACTION_NONE and (input.temperature_c != 0.0 or input.pressure_hpa != 0.0)) {
      throw std::invalid_argument { "Geometric input has nonzero unused weather storage" };
    }

    const auto atmosphere = lib::codes::refraction_params(input.refraction_code, input.temperature_c, input.pressure_hpa);
    query::Request request {
      .target = input.target_code, .level = input.level_deg, .above = input.sense_code != 0, .convention = 2,
      .params = { .temperature_c = 0.0, .pressure_hpa = 0.0, .model = astro::earth::refraction::Model::BENNETT },
    };
    if (atmosphere.has_value()) {
      request.convention = static_cast<uint32_t>(atmosphere->model);
      request.params = *atmosphere;
    }
    result[index] = request;
  }
  return result;
}

/** @brief Completed preparation; projecting a valid index cannot allocate, validate or throw. */
struct Prepared {
  query::Kind kind;
  const query::Computed& computed;
  std::span<const query::Request> requests;
  uint32_t target;
  uint32_t source;
  uint32_t clock;

  [[nodiscard]] auto size() const noexcept -> std::size_t {
    return kind == query::Kind::TRANSITS ? computed.transit_times.size()
      : requests.size() + computed.composed.reported_events.size() + (kind == query::Kind::WINDOWS ? computed.composed.windows.size() : 0);
  }

  auto write(std::size_t index, CelestialAltitudeRecord& record) const noexcept -> void {
    record = {};
    if (kind == query::Kind::TRANSITS) {
      record.tag = CELESTIAL_ALTITUDE_RECORD_UPPER_TRANSIT;
      record.target_code = target;
      record.position_source_code = source;
      record.delta_t_model_code = clock;
      record.jde_tt = computed.transit_times[index];
      return;
    }
    if (index < requests.size()) {
      const auto& request = requests[index];
      record.condition_index = static_cast<uint32_t>(index);
      record.target_code = request.target;
      record.position_source_code = std::span { SOURCE_CODES }[request.target];
      record.delta_t_model_code = clock;
      // Internal Bennett/Saemundsson/geometric codes 0/1/2 become C codes 1/2/0.
      record.refraction_code = request.convention == 2 ? CELESTIAL_REFRACTION_NONE : request.convention + 1;
      record.sense_code = static_cast<uint32_t>(request.above);
      record.initially_satisfied = computed.composed.states[index].initially_satisfied;
      record.requested_level_deg = request.level;
      record.geometric_level_deg = computed.conditions[index].geometric_degrees;
      if (request.convention != 2) {
        record.temperature_c = request.params.temperature_c;
        record.pressure_hpa = request.params.pressure_hpa;
      }
      return;
    }
    index -= requests.size();
    if (index < computed.composed.reported_events.size()) {
      const auto& event = computed.composed.reported_events[index];
      record.tag = CELESTIAL_ALTITUDE_RECORD_EVENT;
      record.condition_index = event.condition;
      record.direction_code = event.change > 0
        ? CELESTIAL_EVENT_DIRECTION_INCREASING : CELESTIAL_EVENT_DIRECTION_DECREASING;
      record.jde_tt = event.jde_tt;
      return;
    }
    index -= computed.composed.reported_events.size();
    const auto& window = computed.composed.windows[index];
    record.tag = CELESTIAL_ALTITUDE_RECORD_WINDOW;
    record.jde_tt = window.start_jde_tt;
    record.end_jde_tt = window.end_jde_tt;
  }
};

/** @brief Validate completed condition metadata before publishing any output. */
[[nodiscard]] auto records(
  const query::Kind kind,
  const std::span<const query::Request> requests,
  const uint32_t target,
  const query::Computed& computed
) -> Prepared {
  constexpr uint32_t clock = lib::codes::code_of(astro::delta_t::Model::ALGO5);
  const uint32_t source = kind == query::Kind::TRANSITS ? std::span { SOURCE_CODES }[target] : 0;
  Prepared result {
    .kind = kind, .computed = computed, .requests = requests,
    .target = target, .source = source, .clock = clock,
  };
  if (kind == query::Kind::TRANSITS) {
    return result;
  }
  if (computed.composed.states.size() != requests.size() or computed.conditions.size() != requests.size()) {
    throw std::runtime_error { "The completed condition metadata is inconsistent" };
  }
  if (computed.composed.reported_events.size() != computed.composed.companion_events) {
    throw std::runtime_error { "The typed merger lost a reported condition event" };
  }
  return result;
}

/** @brief Apply the shared C error/count boundary to one complete feature stream. */
// NOLINTBEGIN(bugprone-easily-swappable-parameters) -- Count and target retain their distinct query roles at the shared C boundary.
[[nodiscard]] auto run(
  const std::string_view operation,
  const query::Kind kind,
  const CelestialAltitudeCondition* conditions,
  const uint32_t condition_count,
  const uint32_t target,
  const double latitude_deg,
  const double longitude_deg,
  const double elevation_m,
  const double start_jde_tt,
  const double end_jde_tt,
  uint32_t* count,
  CelestialAltitudeRecord* slots,
  const uint32_t capacity
) -> uint32_t {
  return lib::wrap_export(operation, [=] {
    lib::check_count(count, "count");
    lib::check_slots(slots, capacity);
    const auto decoded = requests(conditions, condition_count);
    auto budget = query::query_budget();
    const auto computed = query::compute(
      kind,
      astro::observer::Observer {
        .location = {
          .latitude = astro::toolbox::AngleDeg { latitude_deg },
          .longitude = astro::toolbox::AngleDeg { longitude_deg },
        },
        .elevation = astro::toolbox::DistanceKm { elevation_m / 1000.0 },
      },
      { .start_jde_tt = start_jde_tt, .end_jde_tt = end_jde_tt },
      decoded,
      target,
      budget
    );
    const auto output = records(kind, decoded, target, computed);
    if (output.size() > std::numeric_limits<uint32_t>::max()) {
      throw std::runtime_error { "The complete record count cannot be represented" };
    }

    *count = static_cast<uint32_t>(output.size());
    const uint32_t written = std::min(*count, capacity);
    const auto destination = std::span { slots, written };
    for (uint32_t index = 0; index < written; ++index) {
      output.write(index, destination[index]);
    }
    return written;
  });
}
// NOLINTEND(bugprone-easily-swappable-parameters)

} // namespace

extern "C" {

auto celestial_altitude_events(
  const CelestialAltitudeCondition* condition,
  const double latitude_deg,
  const double longitude_deg,
  const double elevation_m,
  const double start_jde_tt,
  const double end_jde_tt,
  uint32_t* record_count,
  CelestialAltitudeRecord* slots,
  const uint32_t slot_count
) -> uint32_t {
  return run(
    __func__,
    query::Kind::ALTITUDE,
    condition,
    1,
    0,
    latitude_deg,
    longitude_deg,
    elevation_m,
    start_jde_tt,
    end_jde_tt,
    record_count,
    slots,
    slot_count
  );
}

auto celestial_upper_transits(
  const uint32_t target_code,
  const double latitude_deg,
  const double longitude_deg,
  const double elevation_m,
  const double start_jde_tt,
  const double end_jde_tt,
  uint32_t* record_count,
  CelestialAltitudeRecord* slots,
  const uint32_t slot_count
) -> uint32_t {
  return run(
    __func__,
    query::Kind::TRANSITS,
    nullptr,
    0,
    target_code,
    latitude_deg,
    longitude_deg,
    elevation_m,
    start_jde_tt,
    end_jde_tt,
    record_count,
    slots,
    slot_count
  );
}

auto celestial_observing_windows(
  const CelestialAltitudeCondition* conditions,
  const uint32_t condition_count,
  const double latitude_deg,
  const double longitude_deg,
  const double elevation_m,
  const double start_jde_tt,
  const double end_jde_tt,
  uint32_t* record_count,
  CelestialAltitudeRecord* slots,
  const uint32_t slot_count
) -> uint32_t {
  return run(
    __func__,
    query::Kind::WINDOWS,
    conditions,
    condition_count,
    0,
    latitude_deg,
    longitude_deg,
    elevation_m,
    start_jde_tt,
    end_jde_tt,
    record_count,
    slots,
    slot_count
  );
}

} // extern "C"
