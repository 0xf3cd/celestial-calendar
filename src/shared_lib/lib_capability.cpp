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
#include <cstdint>
#include <format>
#include <functional>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "lib.hpp"
#include "lib_codes.hpp"
#include "celestial.h"

#include "capability.hpp"
#include "chart.hpp"
#include "leap_second.hpp"

namespace {

using astro::capability::Operation;
using astro::capability::Status;
using astro::ephemeris::PositionSource;

constexpr double UNBOUNDED = std::numeric_limits<double>::quiet_NaN();

/** @brief Decode only operations with numeric TT target records. */
auto operation_of(const uint32_t code) -> Operation {
  switch (code) {
    case CELESTIAL_OPERATION_POSITION: return Operation::POSITION;
    case CELESTIAL_OPERATION_LONGITUDE_RATE: return Operation::LONGITUDE_RATE;
    case CELESTIAL_OPERATION_EVENTS: return Operation::EVENTS;
    case CELESTIAL_OPERATION_EQUATORIAL: return Operation::EQUATORIAL;
    default: throw std::invalid_argument { std::format("Operation {} has no TT target capability", code) };
  }
}

/** @brief Encode the typed rule without relying on enum ordinals. */
auto code_of(const Status status) -> uint32_t {
  switch (status) {
    case Status::SOURCE_WINDOW: return CELESTIAL_STATUS_SOURCE_WINDOW;
    case Status::IMPLEMENTATION_WINDOW: return CELESTIAL_STATUS_IMPLEMENTATION_WINDOW;
    case Status::FINITE_INPUT_EVALUATION: return CELESTIAL_STATUS_FINITE_INPUT_EVALUATION;
  }
  throw std::logic_error { "Unknown core capability status" };
}

/** @brief Shape typed facts into the C-only optional-bound encoding. */
auto to_c_record(const uint32_t operation_code, const astro::capability::Record& record) -> CelestialCapabilityRecord {
  return {
    .operation_code = operation_code,
    .subject_code = lib::codes::code_of(record.target),
    .status_code = code_of(record.status),
    .source_code = lib::codes::code_of(record.source),
    .domain_start_jde_tt = record.domain.has_value() ? record.domain->front() : UNBOUNDED,
    .domain_end_jde_tt = record.domain.has_value() ? record.domain->back() : UNBOUNDED,
    .maximum_span_tt_days = record.maximum_span_tt_days.has_value()
      ? static_cast<uint32_t>(*record.maximum_span_tt_days) : UINT32_C(0),
  };
}

/** @brief Delta T guards apply at the model's evaluation instant, not every converted TT output. */
auto delta_t_domain(const astro::delta_t::Model model) -> std::array<double, 2> {
  using enum astro::delta_t::Model;
  switch (model) {
    case ALGO1: return { astro::delta_t::ALGO1_START_JDE_TT, UNBOUNDED };
    case ALGO2:
    case ALGO5: return { UNBOUNDED, UNBOUNDED };
    case ALGO3: return { UNBOUNDED, astro::delta_t::ALGO3_END_JDE_TT };
    case ALGO4: return { UNBOUNDED, astro::delta_t::ALGO4_END_JDE_TT };
  }
  throw std::logic_error { "Unknown core Delta T identity" };
}

/** @brief Classify each concrete model explicitly; a new model needs a status decision. */
auto delta_t_status(const astro::delta_t::Model model) -> uint32_t {
  using enum astro::delta_t::Model;
  switch (model) {
    case ALGO1:
    case ALGO2:
    case ALGO3:
    case ALGO4:
      return CELESTIAL_MODEL_STATUS_FROZEN;
    case ALGO5:
      return CELESTIAL_MODEL_STATUS_CURRENT;
  }
  throw std::logic_error { "Unknown core Delta T identity" };
}

/** @brief Encode the source interval whose target consistency is checked in the core roster. */
auto source_domain(const PositionSource source) -> std::array<double, 2> {
  for (const auto& facts : astro::ephemeris::TARGET_FACTS) {
    if (facts.source == source) {
      return facts.position_domain.value_or(std::array { UNBOUNDED, UNBOUNDED });
    }
  }
  throw std::logic_error { "Position source has no provider" };
}

/** @brief Enumerate explicit model keys; DEFAULT resolution belongs to operations, not this registry. */
auto model_identities(const uint32_t kind) -> std::vector<CelestialModelIdentityRecord> {
  const bool known_kind = kind == CELESTIAL_MODEL_KIND_ALL or kind == CELESTIAL_MODEL_KIND_DELTA_T
    or kind == CELESTIAL_MODEL_KIND_NUTATION or kind == CELESTIAL_MODEL_KIND_POSITION_SOURCE;
  if (not known_kind) {
    throw std::invalid_argument { std::format("Unknown model kind {}", kind) };
  }

  auto result = std::vector<CelestialModelIdentityRecord>();
  if (kind == CELESTIAL_MODEL_KIND_ALL or kind == CELESTIAL_MODEL_KIND_DELTA_T) {
    for (const auto& entry : lib::codes::DELTA_T_MODELS) {
      const auto domain = delta_t_domain(entry.value);
      result.push_back({
        .kind_code = CELESTIAL_MODEL_KIND_DELTA_T,
        .identity_code = entry.code,
        .status_code = delta_t_status(entry.value),
        .domain_start_jde_tt = domain.front(),
        .domain_end_jde_tt = domain.back(),
      });
    }
  }
  if (kind == CELESTIAL_MODEL_KIND_ALL or kind == CELESTIAL_MODEL_KIND_NUTATION) {
    for (const auto& entry : lib::codes::NUTATION_MODELS) {
      result.push_back({
        .kind_code = CELESTIAL_MODEL_KIND_NUTATION,
        .identity_code = entry.code,
        .status_code = CELESTIAL_MODEL_STATUS_CURRENT,
        .domain_start_jde_tt = UNBOUNDED,
        .domain_end_jde_tt = UNBOUNDED,
      });
    }
  }
  if (kind == CELESTIAL_MODEL_KIND_ALL or kind == CELESTIAL_MODEL_KIND_POSITION_SOURCE) {
    for (const auto& entry : lib::codes::POSITION_SOURCES) {
      const auto domain = source_domain(entry.value);
      result.push_back({
        .kind_code = CELESTIAL_MODEL_KIND_POSITION_SOURCE,
        .identity_code = entry.code,
        .status_code = CELESTIAL_MODEL_STATUS_CURRENT,
        .domain_start_jde_tt = domain.front(),
        .domain_end_jde_tt = domain.back(),
      });
    }
  }
  return result;
}

/** @brief Describe the provider's frame and corrections without turning validation samples into a domain. */
auto source_text(const PositionSource source) -> std::string_view {
  switch (source) {
    case PositionSource::SUN_VSOP87D:
      return "VSOP87D Earth series, FK5 adjustment, solar aberration and fixed IAU1980 longitude nutation";
    case PositionSource::MOON_ELP2000_82B_TRUNC:
      return "truncated ELP2000-82B lunar series (Meeus Chapter 47), with fixed IAU1980 longitude nutation";
    case PositionSource::PLANET_VSOP87D:
      return "VSOP87D heliocentric series, light time, aberration, FK5 adjustment and fixed IAU1980 longitude nutation";
    case PositionSource::PLUTO_PLUTO95_APPARENT:
      return "Meeus Chapter 37, Table 37.A series, with apparent-place corrections; PLUTO95 supplies independent references, not the runtime evaluator";
    case PositionSource::NODE_MEAN:
      return "Meeus mean-node polynomial, tropical and referred to the mean equinox of date before adding fixed IAU1980 longitude nutation; a mathematical point, not a physical body";
    case PositionSource::NODE_TRUE_MEEUS_FIVE_TERM:
      return "Meeus five-term true-node approximation plus fixed IAU1980 longitude nutation; a mathematical point, not an osculating node";
  }
  throw std::logic_error { "Unknown core position source" };
}

/** @brief Cite validation owners rather than duplicating their measured coverage counts. */
auto reference_tests(const PositionSource source) -> std::string_view {
  switch (source) {
    case PositionSource::SUN_VSOP87D: return "sun_test.cpp and ephemeris_test.cpp";
    case PositionSource::MOON_ELP2000_82B_TRUNC: return "moon_test.cpp and ephemeris_test.cpp";
    case PositionSource::PLANET_VSOP87D:
    case PositionSource::PLUTO_PLUTO95_APPARENT: return "planet_test.cpp";
    case PositionSource::NODE_MEAN:
    case PositionSource::NODE_TRUE_MEEUS_FIVE_TERM: return "lunar_node_test.cpp";
  }
  throw std::logic_error { "Unknown core position source" };
}

/** @brief Explain the authoritative decimal-year guard and the actual frozen/current model branches. */
auto delta_t_text(const astro::delta_t::Model model) -> std::string {
  using enum astro::delta_t::Model;
  switch (model) {
    case ALGO1:
      return std::format("Frozen Xu Jianwei, 寿星万年历2008版(V1.3.2); finite decimal year >= {}.", astro::delta_t::algo1::MINIMUM_YEAR);
    case ALGO2:
      return "Frozen Espenak and Meeus, NASA/TP-2006-214141, Section 2.7; finite decimal year, no declared hard year interval.";
    case ALGO3:
      return std::format("Frozen Fred Espenak 2014 eclipse canon; finite decimal year < {}.", astro::delta_t::algo3::MAXIMUM_YEAR_EXCLUSIVE);
    case ALGO4:
      return "Frozen AstroTime-Analysis predecessor: Algo2 before 2005, Bulletin A fit for 2005-2024 and USNO prediction fit for 2024-2035. Finite decimal year < 2035; coefficients remain tied to its retained generation record.";
    case ALGO5:
      return std::format(
        "Current AstroTime-Analysis model: Algo2 before 2005, fitted IERS Bulletin A segment through decimal year {} inclusive, then an anchored integrated-LOD extrapolation (Morrison et al. 2021 addendum and corrected HMNAO combined expression). No declared hard year interval. Source-derived terms retain their source terms; coefficients and provenance are tied to the library version and algo5 generation record.",
        astro::delta_t::algo5::LAST_OBSERVATION_YEAR
      );
  }
  throw std::logic_error { "Unknown core Delta T identity" };
}

/** @brief Resolve a model key by its named code, never by registry position. */
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): A key is the published kind/identity pair.
auto identity_text(const uint32_t kind, const uint32_t code) -> std::string {
  switch (kind) {
    case CELESTIAL_MODEL_KIND_DELTA_T: {
      const auto& entry = lib::codes::detail::from_code(lib::codes::DELTA_T_MODELS, code);
      return std::format("Delta T {}: {} Guards apply at the evaluation instant: UT1 for forward conversion, TT for reverse conversion. They are not universal output-TT limits. Validation: delta_t_test.cpp.", entry.name, delta_t_text(entry.value));
    }
    case CELESTIAL_MODEL_KIND_NUTATION: {
      const auto& entry = lib::codes::detail::from_code(lib::codes::NUTATION_MODELS, code);
      return std::format("Nutation {}: current implementation; longitude and obliquity nutation use this model together. No declared hard input interval, not an accuracy guarantee. Validation: earth_test.cpp. Exact coefficients require the library/source version.", entry.name);
    }
    case CELESTIAL_MODEL_KIND_POSITION_SOURCE: {
      const auto& entry = lib::codes::detail::from_code(lib::codes::POSITION_SOURCES, code);
      return std::format("Position source {}: {}. Geocentric apparent ecliptic and equinox of date; distance in AU for physical bodies, absent for nodes. Declared target/provider windows apply. Validation: {}. Exact coefficients require the library/source version.", entry.name, source_text(entry.value), reference_tests(entry.value));
    }
    default: throw std::invalid_argument { std::format("Unknown concrete model kind {}", kind) };
  }
}

/** @brief Describe model selection owned by non-target operations. */
auto operation_model_text(const uint32_t operation) -> std::string {
  const auto delta_t = lib::codes::detail::from_value(lib::codes::DELTA_T_MODELS, astro::delta_t::DEFAULT_MODEL).name;
  const auto nutation = lib::codes::detail::from_value(lib::codes::NUTATION_MODELS, astro::house::DEFAULT_NUTATION_MODEL).name;
  switch (operation) {
    case CELESTIAL_OPERATION_DELTA_T:
      return std::format("Delta T: selectable model; DEFAULT resolves to {}. The result records the resolved identity. TT - UT1 is in seconds; model guards apply at the decimal-year evaluation instant. Model sources/status: celestial_model_identity_text; validation: delta_t_test.cpp.", delta_t);
    case CELESTIAL_OPERATION_HOUSES:
    case CELESTIAL_OPERATION_CHART:
      return std::format(
        "{}: Delta T DEFAULT resolves to {}; house nutation DEFAULT resolves to {}. Civil policy [{}, {}), UTC from 1972-01-01; Algo4 additionally requires derived TT before 2035-01-01. Future UTC holds the last known leap-table offset; UT1 is modelled, not observed DUT1. Houses use the selected nutation for longitude/obliquity corrections and the IAU1980 mean-obliquity cubic. Geographic/pole/unresolved-angle checks still apply. {} Model sources/status: celestial_model_identity_text; validation: chart_test.cpp and house_test.cpp.",
        operation == CELESTIAL_OPERATION_CHART ? "Chart" : "Civil houses",
        delta_t, nutation, astro::chart::CIVIL_START_YEAR, astro::chart::CIVIL_END_YEAR_EXCLUSIVE,
        operation == CELESTIAL_OPERATION_CHART
          ? "Chart additionally intersects every target's rate domain; bodies keep fixed IAU1980 corrections and each records its source."
          : "No chart target/rate window applies; celestial_house_capability reports the civil policy."
      );
    default: throw std::invalid_argument { std::format("Operation {} has no non-target model-use description", operation) };
  }
}

/** @brief Describe one target operation while keeping domain, coordinate validity and accuracy distinct. */
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): Operation precedes its subject in the published lookup pair.
auto capability_text(const uint32_t operation_code, const uint32_t subject) -> std::string {
  const bool no_target = operation_code == CELESTIAL_OPERATION_HOUSES or operation_code == CELESTIAL_OPERATION_CHART
    or operation_code == CELESTIAL_OPERATION_DELTA_T;
  if (no_target) {
    if (subject != CELESTIAL_TARGET_ALL) {
      throw std::invalid_argument { "This operation requires CELESTIAL_TARGET_ALL because it has no target" };
    }
    return operation_model_text(operation_code);
  }

  const auto operation = operation_of(operation_code);
  const auto target = lib::codes::target(subject);
  const auto record = astro::capability::domain_record(operation, target);
  const auto name = lib::codes::detail::from_value(lib::codes::TARGETS, target).name;
  const auto domain = record.domain.has_value()
    ? std::format("JDE(TT) in [{}, {})", record.domain->front(), record.domain->back())
    : std::string { "no declared hard input interval; finite-input evaluation can still fail" };

  const auto geometry = std::invoke([&]() -> std::string_view {
    switch (operation) {
      case Operation::POSITION:
        return "Apparent geocentric true ecliptic and equinox of date; longitude/latitude in degrees.";
      case Operation::EQUATORIAL:
        return "Apparent geocentric RA/Dec of date; fixed IAU1980 mean-obliquity cubic plus IAU1980 "
          "obliquity nutation projects the apparent ecliptic longitude/latitude. "
          "Obliquity accuracy degrades over millennia; no extra epsilon cutoff is imposed.";
      case Operation::LONGITUDE_RATE:
        return "Apparent geocentric true-ecliptic longitude rate, in degrees per TT day.";
      case Operation::EVENTS:
        return "Events of apparent geocentric true-ecliptic longitude.";
    }
    throw std::logic_error { "Unknown core capability operation" };
  });
  const auto role = std::invoke([&]() -> std::string {
    switch (operation) {
      case Operation::EVENTS:
        return std::format("Event query, maximum span {} TT days; validation: ephemeris_events_test.cpp and certified bound records.", astro::ephemeris::EVENT_MAXIMUM_QUERY_SPAN_TT_DAYS);
      case Operation::LONGITUDE_RATE:
        return "Seven-point centered longitude rate; the full stencil must fit the declared center domain. Validation: ephemeris_test.cpp and certified bound records.";
      case Operation::POSITION:
      case Operation::EQUATORIAL:
        return "Direct selected-target position; no rate stencil, civil conversion or house calculation.";
    }
    throw std::logic_error { "Unknown core capability operation" };
  });
  const auto status = std::invoke([&]() -> std::string_view {
    switch (record.status) {
      case Status::SOURCE_WINDOW: return "The interval is source-published. ";
      case Status::IMPLEMENTATION_WINDOW: return "The interval is implementation-selected. ";
      case Status::FINITE_INPUT_EVALUATION: return "";
    }
    throw std::logic_error { "Unknown core capability status" };
  });
  const std::string_view distance = operation == Operation::POSITION or operation == Operation::EQUATORIAL
    ? "Distance in AU for physical bodies, absent for nodes. " : "";

  return std::format(
    "{}: {}. {} {} Input policy: {}. {}{}Coordinate validity establishes neither accuracy nor physical "
    "plausibility. Position references: {}. Fixed conventions and source coefficients are tied to the library version.",
    name, source_text(record.source), geometry, role, domain,
    status, distance,
    reference_tests(record.source)
  );
}

} // namespace

extern "C" {

// NOLINTBEGIN(bugprone-easily-swappable-parameters): Preserve the published operation/subject and kind/identity lookup pairs.
auto celestial_capabilities(
  const uint32_t operation_code,
  const uint32_t subject_code,
  uint32_t* const record_count,
  CelestialCapabilityRecord* const slots,
  const uint32_t slot_count
) -> uint32_t {
  return lib::wrap_export(__func__, [=] {
    lib::check_count(record_count, "record_count");
    lib::check_slots(slots, slot_count);
    const auto operation = operation_of(operation_code);
    const auto records = subject_code == CELESTIAL_TARGET_ALL
      ? astro::capability::domain_records(operation)
      : std::vector { astro::capability::domain_record(operation, lib::codes::target(subject_code)) };

    *record_count = static_cast<uint32_t>(records.size());
    const uint32_t written = std::min(*record_count, slot_count);
    for (uint32_t index = 0; index < written; ++index) {
      std::span { slots, slot_count }[index] = to_c_record(operation_code, records.at(index));
    }
    return written;
  });
}

auto celestial_model_identities(
  const uint32_t kind_code,
  uint32_t* const record_count,
  CelestialModelIdentityRecord* const slots,
  const uint32_t slot_count
) -> uint32_t {
  return lib::wrap_export(__func__, [=] {
    lib::check_count(record_count, "record_count");
    lib::check_slots(slots, slot_count);
    const auto records = model_identities(kind_code);

    *record_count = static_cast<uint32_t>(records.size());
    const uint32_t written = std::min(*record_count, slot_count);
    if (written > 0) {
      std::copy_n(records.cbegin(), written, slots);
    }
    return written;
  });
}

auto celestial_model_identity_text(
  const uint32_t kind_code,
  const uint32_t identity_code,
  char* const buffer,
  const uint32_t capacity
) -> uint32_t {
  return lib::wrap_export(__func__, [=] {
    return lib::copy_text(identity_text(kind_code, identity_code), buffer, capacity);
  });
}

auto celestial_capability_text(
  const uint32_t operation_code,
  const uint32_t subject_code,
  char* const buffer,
  const uint32_t capacity
) -> uint32_t {
  return lib::wrap_export(__func__, [=] {
    return lib::copy_text(capability_text(operation_code, subject_code), buffer, capacity);
  });
}

auto celestial_house_capability(const uint32_t delta_t_model_code) -> CelestialCivilHouseCapability {
  return lib::wrap_export(__func__, [=] {
    const auto model = lib::codes::delta_t_model(delta_t_model_code);
    const auto cutoff = astro::chart::civil_derived_tt_cutoff(model);
    const auto [year, month, day] = util::from_ymd(astro::leap_second::MODERN_UTC_START);
    return CelestialCivilHouseCapability {
      .valid = true,
      .status_code = code_of(Status::IMPLEMENTATION_WINDOW),
      .civil_start_year = astro::chart::CIVIL_START_YEAR,
      .civil_end_year_exclusive = astro::chart::CIVIL_END_YEAR_EXCLUSIVE,
      .civil_scale_mask = (UINT32_C(1) << CELESTIAL_CIVIL_SCALE_UTC) | (UINT32_C(1) << CELESTIAL_CIVIL_SCALE_UT1),
      .utc_start_year = year,
      .utc_start_month = month,
      .utc_start_day = day,
      .delta_t_identity_code = lib::codes::code_of(model),
      .has_derived_tt_cutoff = cutoff.has_value(),
      .derived_tt_end_jde_tt = cutoff.value_or(0.0),
    };
  });
}

// NOLINTEND(bugprone-easily-swappable-parameters)
} // extern "C"
