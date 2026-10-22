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
#include <format>
#include <span>
#include <utility>
#include <vector>

#include "lib.hpp"
#include "lib_chart.hpp"
#include "celestial.h"

#include "capability.hpp"
#include "chart.hpp"
#include "house.hpp"

namespace {

using astro::ephemeris::Target;

auto check_record_count_ptr(uint32_t * const record_count) -> void {
  if (record_count == nullptr) {
    throw std::invalid_argument { "Argument `record_count` is null." };
  }
  *record_count = 0;
}

auto check_selection(const std::span<const uint32_t> codes) -> void {
  if (codes.empty()) {
    throw std::invalid_argument { "Argument `target_count` must be positive (an empty selection is invalid)." };
  }

  // Duplicated targets are rejected like a self-paired difference event: one call owns one
  // answer per target. The seen set also bounds the scan regardless of the caller's count.
  auto seen = std::array<bool, 14> {};
  for (const uint32_t code : codes) {
    if (code >= 14) {
      throw std::invalid_argument { std::format("Argument `target_codes[?]` must be a CHART_TARGET_* code in [0, 13], got {}", code) };
    }
    if (seen.at(code)) {
      throw std::invalid_argument { std::format("Argument `target_codes` duplicates target code {}", code) };
    }
    seen.at(code) = true;
  }
}

} // namespace


extern "C" {

// #355: the independent queries. position_query_v1 composes the same public position operation
// the snapshot uses — no rate stencil, no joint window; house_query_v1 runs the snapshot's civil
// conversion without the snapshot joint window and records the resolved model identities.

// NOLINTBEGIN(bugprone-easily-swappable-parameters): selection pointer/count is the published query contract.
auto position_query_v1(
  const uint32_t * const target_codes,
  const uint32_t target_count,
  const double jde_tt,
  uint32_t * const record_count,
  PositionRecordV1 * const slots,
  const uint32_t slot_count
) -> uint32_t {
  return lib::wrap_export(__func__, [=] {
    check_record_count_ptr(record_count);
    if (slots == nullptr and slot_count > 0) {
      throw std::invalid_argument { "Argument `slots` is null, but `slot_count` is greater than 0." };
    }
    if (target_codes == nullptr) {
      throw std::invalid_argument { "Argument `target_codes` is null, but `target_count` is greater than 0." };
    }
    const auto codes = std::span<const uint32_t> { target_codes, target_count };
    check_selection(codes);
    if (not std::isfinite(jde_tt)) {
      throw std::invalid_argument { std::format("Argument `jde_tt` is not finite, got {}", jde_tt) };
    }

    // The selection is validated up front, so a failing target cannot leave a partial list:
    // the count is published only after every position evaluated.
    auto positions = std::vector<PositionRecordV1>();
    positions.reserve(target_count);
    for (const uint32_t code : codes) {
      const auto target = static_cast<Target>(code);
      const auto coordinate = astro::chart::position(target, jde_tt);
      positions.push_back({
        .target_code = code,
        .present_fields = coordinate.distance.has_value() ? POSITION_PRESENT_DISTANCE : 0,
        .longitude_deg = coordinate.longitude.deg(),
        .latitude_deg = coordinate.latitude.deg(),
        .distance_au = coordinate.distance.has_value() ? coordinate.distance->au() : 0.0,
        .source_code = std::to_underlying(astro::capability::position_source_of(target)),
        .flags = 0,
      });
    }

    *record_count = static_cast<uint32_t>(positions.size());
    const auto num_written = std::min(static_cast<uint32_t>(positions.size()), slot_count);
    std::copy_n(positions.cbegin(), num_written, slots);
    return num_written;
  });
}
// NOLINTEND(bugprone-easily-swappable-parameters)

// NOLINTBEGIN(bugprone-easily-swappable-parameters): the civil/location/selector order is the published query contract.
auto house_query_v1(
  const int32_t year,
  const uint32_t month,
  const uint32_t day,
  const double fraction,
  const uint32_t civil_scale_code,
  const uint32_t delta_t_model_code,
  const uint32_t nutation_model_code,
  const double latitude_deg,
  const double longitude_deg,
  const uint32_t house_system_code
) -> HouseQueryResultV1 {
  return lib::wrap_export(__func__, [=] {
    const auto scale = lib::chart::civil_scale(civil_scale_code);
    const auto house_system = lib::chart::house_system(house_system_code);
    const auto delta_t_model = astro::capability::resolve_delta_t_code(delta_t_model_code);
    const auto nutation_model = astro::capability::resolve_nutation_code(nutation_model_code);
    if (not delta_t_model.has_value()) {
      throw std::invalid_argument { std::format("Unknown `delta_t_model_code` {}", delta_t_model_code) };
    }
    if (not nutation_model.has_value()) {
      throw std::invalid_argument { std::format("Unknown `nutation_model_code` {}", nutation_model_code) };
    }

    lib::chart::validate_civil(year, month, day, fraction);
    const auto times = astro::chart::detail::resolve_times_for_houses(
      calendar::Datetime { util::to_ymd(year, month, day), fraction },
      scale,
      *delta_t_model
    );
    const auto houses = astro::house::calculate(
      times.jd_ut1, times.jde_tt,
      astro::GeoLocation { .latitude = astro::toolbox::AngleDeg { latitude_deg }, .longitude = astro::toolbox::AngleDeg { longitude_deg } },
      house_system,
      *nutation_model
    );

    auto result = HouseQueryResultV1 {
      .valid = true,
      .house_system_code = house_system_code,
      .delta_t_identity_code = std::to_underlying(*delta_t_model),
      .nutation_identity_code = std::to_underlying(*nutation_model),
      .flags = 0,
      .jd_ut1 = times.jd_ut1,
      .jde_tt = times.jde_tt,
      .houses = {
        .ascendant_deg = houses.ascendant.deg(),
        .midheaven_deg = houses.midheaven.deg(),
        .descendant_deg = houses.descendant.deg(),
        .imum_coeli_deg = houses.imum_coeli.deg(),
        .cusps_deg = {},
      },
    };
    for (std::size_t index = 0; index < houses.cusps.size(); ++index) {
      std::span { result.houses.cusps_deg }[index] = houses.cusps.at(index).deg();
    }
    return result;
  });
}
// NOLINTEND(bugprone-easily-swappable-parameters)

} // extern "C"
