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
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <span>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

#include "altitude_events.hpp"
#include "celestial.h"
#include "lib.hpp"
#include "lib_codes.hpp"

namespace {

namespace query = astro::observer::detail::altitude_events;
namespace observer = astro::observer;

static_assert(sizeof(CelestialAltitudeCondition) == 40);
static_assert(sizeof(CelestialAltitudeRecord) == 88);
static_assert(std::is_trivially_copy_assignable_v<CelestialAltitudeRecord>);

/** @brief Boundary codes indexed by the admitted core target, reconciled with the owning tables. */
struct TargetIdentity {
  uint32_t target;
  uint32_t source;
};

constexpr auto TARGET_IDENTITIES = std::invoke([] {
  std::array<TargetIdentity, lib::codes::TARGETS.size()> result {};
  for (const auto& entry : lib::codes::TARGETS) {
    result.at(static_cast<std::size_t>(entry.value)) = {
      .target = entry.code,
      .source = lib::codes::code_of(astro::ephemeris::source_of(entry.value)),
    };
  }
  return result;
});

constexpr uint32_t CLOCK = lib::codes::code_of(astro::delta_t::Model::ALGO5);

/** @brief Resolve a full-width physical-target code before narrowing to a core identity. */
[[nodiscard]] auto physical_target(const uint32_t code) -> astro::ephemeris::Target {
  for (const auto& entry : lib::codes::TARGETS) {
    if (entry.code == code and not astro::ephemeris::target_facts(entry.value).node.has_value()) {
      return entry.value;
    }
  }
  throw std::invalid_argument { "A physical target is required" };
}

/** @brief Decode all selectors, admit observer/time, then validate targets and levels in list order. */
[[nodiscard]] auto decode_conditions(
  const CelestialAltitudeCondition* inputs,
  const uint32_t count,
  const observer::Observer& site,
  const query::search::Interval interval
) -> std::vector<observer::AltitudeCondition> {
  if (inputs == nullptr and count > 0) {
    throw std::invalid_argument { "A nonempty condition list requires input storage" };
  }
  if (count > query::MAX_CONDITIONS) {
    throw std::runtime_error { "The condition-list work budget is exhausted" };
  }

  const auto requested = std::span { inputs, count };
  for (const auto& input : requested) {
    const bool known_selectors = input.sense_code <= CELESTIAL_ALTITUDE_SENSE_ABOVE
      and input.refraction_code <= CELESTIAL_REFRACTION_SAEMUNDSSON;
    if (not known_selectors or input.reserved != 0) {
      throw std::invalid_argument { "Invalid condition selector or reserved storage" };
    }
    if (input.refraction_code == CELESTIAL_REFRACTION_NONE and (input.temperature_c != 0.0 or input.pressure_hpa != 0.0)) {
      throw std::invalid_argument { "Geometric input has nonzero unused weather storage" };
    }
  }
  query::validate_interval(site, interval);

  std::vector<observer::AltitudeCondition> result;
  result.reserve(count);
  for (const auto& input : requested) {
    const auto target = physical_target(input.target_code);
    query::validate_target(target, interval);
    const auto atmosphere = lib::codes::refraction_params(input.refraction_code, input.temperature_c, input.pressure_hpa);
    const auto altitude = astro::toolbox::AngleDeg { input.level_deg };
    const auto level = atmosphere.has_value()
      ? observer::AltitudeLevel { observer::RefractedLevel {
          .altitude = altitude,
          .params = *atmosphere,
        } }
      : observer::AltitudeLevel { observer::GeometricLevel { .altitude = altitude } };
    query::validate_level(level);
    result.push_back({
      .target = target,
      .level = level,
      .sense = input.sense_code == CELESTIAL_ALTITUDE_SENSE_ABOVE
        ? observer::AltitudeSense::ABOVE : observer::AltitudeSense::BELOW,
    });
  }
  return result;
}

/** @brief Completed condition stream; valid-index projection cannot allocate, validate or throw. */
struct ConditionRecords {
  std::span<const observer::AltitudeCondition> requested;
  const query::ConditionQuery& computed;
  std::span<const observer::ObservingWindow> windows;

  [[nodiscard]] auto size() const noexcept -> std::size_t {
    return requested.size() + computed.composed.reported_events.size() + windows.size();
  }

  auto write(std::size_t index, CelestialAltitudeRecord& record) const noexcept -> void {
    record = {};
    if (index < requested.size()) {
      const auto& condition = requested[index];
      const auto identity = std::span { TARGET_IDENTITIES }[static_cast<std::size_t>(condition.target)];
      record.condition_index = static_cast<uint32_t>(index);
      record.target_code = identity.target;
      record.position_source_code = identity.source;
      record.delta_t_model_code = CLOCK;
      record.sense_code = condition.sense == observer::AltitudeSense::ABOVE
        ? CELESTIAL_ALTITUDE_SENSE_ABOVE : CELESTIAL_ALTITUDE_SENSE_BELOW;
      record.initially_satisfied = static_cast<uint32_t>(computed.composed.states[index].satisfied);
      record.geometric_level_deg = computed.conditions[index].geometric_degrees;
      assert(not condition.level.valueless_by_exception());
      if (const auto* level = std::get_if<observer::GeometricLevel>(&condition.level); level != nullptr) {
        record.requested_level_deg = level->altitude.deg();
      } else if (const auto* refracted = std::get_if<observer::RefractedLevel>(&condition.level); refracted != nullptr) {
        record.requested_level_deg = refracted->altitude.deg();
        record.refraction_code = refracted->params.model == query::refraction::Model::BENNETT
          ? CELESTIAL_REFRACTION_BENNETT : CELESTIAL_REFRACTION_SAEMUNDSSON;
        record.temperature_c = refracted->params.temperature_c;
        record.pressure_hpa = refracted->params.pressure_hpa;
      }
      return;
    }
    index -= requested.size();
    if (index < computed.composed.reported_events.size()) {
      const auto& event = computed.composed.reported_events[index];
      record.tag = CELESTIAL_ALTITUDE_RECORD_EVENT;
      record.condition_index = event.condition_index;
      record.direction_code = static_cast<uint32_t>(event.direction);
      record.jde_tt = event.jde_tt;
      return;
    }
    index -= computed.composed.reported_events.size();
    const auto& window = windows[index];
    record.tag = CELESTIAL_ALTITUDE_RECORD_WINDOW;
    record.jde_tt = window.start_jde_tt;
    record.end_jde_tt = window.end_jde_tt;
  }
};

/** @brief Validate the completed condition metadata before publishing any record. */
[[nodiscard]] auto records(
  const std::span<const observer::AltitudeCondition> requested,
  const query::ConditionQuery& computed,
  const std::span<const observer::ObservingWindow> windows
) -> ConditionRecords {
  if (computed.composed.states.size() != requested.size() or computed.conditions.size() != requested.size()) {
    throw std::runtime_error { "The completed condition metadata is inconsistent" };
  }
  if (computed.composed.reported_events.size() != computed.composed.companion_events) {
    throw std::runtime_error { "The typed merger lost a reported condition event" };
  }
  return {
    .requested = requested,
    .computed = computed,
    .windows = windows,
  };
}

/** @brief Completed transit stream, with no condition or window state. */
struct TransitRecords {
  TargetIdentity identity;
  std::span<const double> times;

  [[nodiscard]] auto size() const noexcept -> std::size_t { return times.size(); }

  auto write(const std::size_t index, CelestialAltitudeRecord& record) const noexcept -> void {
    record = {};
    record.tag = CELESTIAL_ALTITUDE_RECORD_UPPER_TRANSIT;
    record.target_code = identity.target;
    record.position_source_code = identity.source;
    record.delta_t_model_code = CLOCK;
    record.jde_tt = times[index];
  }
};

/** @brief Publish a complete stream's count and requested prefix after all preparation succeeds. */
template <typename Records>
[[nodiscard]] auto publish(
  const Records& output,
  uint32_t* count,
  CelestialAltitudeRecord* slots,
  const uint32_t capacity
) -> uint32_t {
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
}

/** @brief Check caller storage, prepare the complete stream, then publish its count and prefix. */
template <typename Prepare>
[[nodiscard]] auto run(
  const std::string_view operation,
  uint32_t* count,
  CelestialAltitudeRecord* slots,
  const uint32_t capacity,
  const Prepare& prepare
) -> uint32_t {
  return lib::wrap_export(operation, [&] {
    lib::check_count(count, "count");
    lib::check_slots(slots, capacity);
    return prepare();
  });
}

/** @brief Decode the C site's degrees and metres into the core's physical units. */
[[nodiscard]] auto site(
  const double latitude_deg,
  const double longitude_deg,
  const double elevation_m
) -> observer::Observer {
  return {
    .location = {
      .latitude = astro::toolbox::AngleDeg { latitude_deg },
      .longitude = astro::toolbox::AngleDeg { longitude_deg },
    },
    .elevation = astro::toolbox::DistanceKm { elevation_m / 1000.0 },
  };
}

} // namespace

extern "C" {

// NOLINTBEGIN(bugprone-easily-swappable-parameters): Parameters retain the published C ABI order.
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
  return run(__func__, record_count, slots, slot_count, [=] {
    const auto observer = site(latitude_deg, longitude_deg, elevation_m);
    const query::search::Interval interval {
      .start_jde_tt = start_jde_tt,
      .end_jde_tt = end_jde_tt,
    };
    const auto decoded = decode_conditions(condition, 1, observer, interval);
    auto budget = query::query_budget();
    const auto computed = query::search_conditions(observer, interval, decoded, budget);
    const auto output = records(decoded, computed, {});
    return publish(output, record_count, slots, slot_count);
  });
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
  return run(__func__, record_count, slots, slot_count, [=] {
    const auto observer = site(latitude_deg, longitude_deg, elevation_m);
    const query::search::Interval interval {
      .start_jde_tt = start_jde_tt,
      .end_jde_tt = end_jde_tt,
    };
    query::validate_interval(observer, interval);
    const auto target = physical_target(target_code);
    query::validate_target(target, interval);
    auto budget = query::query_budget();
    const auto times = query::search_transits(observer, interval, target, budget);
    const TransitRecords output {
      .identity = std::span { TARGET_IDENTITIES }[static_cast<std::size_t>(target)],
      .times = times,
    };
    return publish(output, record_count, slots, slot_count);
  });
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
  return run(__func__, record_count, slots, slot_count, [=] {
    const auto observer = site(latitude_deg, longitude_deg, elevation_m);
    const query::search::Interval interval {
      .start_jde_tt = start_jde_tt,
      .end_jde_tt = end_jde_tt,
    };
    const auto decoded = decode_conditions(conditions, condition_count, observer, interval);
    auto budget = query::query_budget();
    const auto computed = query::search_conditions(observer, interval, decoded, budget);
    const auto output = records(decoded, computed, computed.composed.windows);
    return publish(output, record_count, slots, slot_count);
  });
}

// NOLINTEND(bugprone-easily-swappable-parameters)

} // extern "C"
