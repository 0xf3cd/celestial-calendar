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
#include <cstddef>
#include <cstdint>
#include <format>
#include <stdexcept>
#include <string_view>

#include "celestial.h"
#include "delta_t.hpp"
#include "earth.hpp"
#include "ephemeris.hpp"
#include "house.hpp"

namespace lib::codes {

/** @brief One explicit C code, core value and optional model name. */
template <typename Value>
struct Entry {
  uint32_t code {};
  Value value {};
  std::string_view name;
};

// NOLINTBEGIN(modernize-use-designated-initializers): Keep code, core identity and name aligned as table columns.
inline constexpr std::array DELTA_T_MODELS {
  Entry { CELESTIAL_DELTA_T_MODEL_ALGO1, astro::delta_t::Model::ALGO1, "Algo1" },
  Entry { CELESTIAL_DELTA_T_MODEL_ALGO2, astro::delta_t::Model::ALGO2, "Algo2" },
  Entry { CELESTIAL_DELTA_T_MODEL_ALGO3, astro::delta_t::Model::ALGO3, "Algo3" },
  Entry { CELESTIAL_DELTA_T_MODEL_ALGO4, astro::delta_t::Model::ALGO4, "Algo4" },
  Entry { CELESTIAL_DELTA_T_MODEL_ALGO5, astro::delta_t::Model::ALGO5, "Algo5" },
};

inline constexpr std::array NUTATION_MODELS {
  Entry { CELESTIAL_NUTATION_MODEL_MEEUS, astro::earth::nutation::Model::MEEUS, "Meeus low-precision" },
  Entry { CELESTIAL_NUTATION_MODEL_IAU_1980, astro::earth::nutation::Model::IAU_1980, "IAU 1980" },
};

inline constexpr std::array POSITION_SOURCES {
  Entry { CELESTIAL_POSITION_SOURCE_SUN_VSOP87D, astro::ephemeris::PositionSource::SUN_VSOP87D, "Sun VSOP87D" },
  Entry { CELESTIAL_POSITION_SOURCE_MOON_ELP2000_82B_TRUNC, astro::ephemeris::PositionSource::MOON_ELP2000_82B_TRUNC, "Moon truncated ELP2000-82B" },
  Entry { CELESTIAL_POSITION_SOURCE_PLANET_VSOP87D, astro::ephemeris::PositionSource::PLANET_VSOP87D, "planet VSOP87D" },
  Entry { CELESTIAL_POSITION_SOURCE_PLUTO_PLUTO95_APPARENT, astro::ephemeris::PositionSource::PLUTO_PLUTO95_APPARENT, "Pluto PLUTO95 apparent place" },
  Entry { CELESTIAL_POSITION_SOURCE_NODE_MEAN, astro::ephemeris::PositionSource::NODE_MEAN, "mean lunar node" },
  Entry { CELESTIAL_POSITION_SOURCE_NODE_TRUE_MEEUS_FIVE_TERM, astro::ephemeris::PositionSource::NODE_TRUE_MEEUS_FIVE_TERM, "Meeus five-term true lunar node" },
};

inline constexpr std::array TARGETS {
  Entry { CELESTIAL_TARGET_SUN, astro::ephemeris::Target::SUN, {} },
  Entry { CELESTIAL_TARGET_MOON, astro::ephemeris::Target::MOON, {} },
  Entry { CELESTIAL_TARGET_MERCURY, astro::ephemeris::Target::MERCURY, {} },
  Entry { CELESTIAL_TARGET_VENUS, astro::ephemeris::Target::VENUS, {} },
  Entry { CELESTIAL_TARGET_MARS, astro::ephemeris::Target::MARS, {} },
  Entry { CELESTIAL_TARGET_JUPITER, astro::ephemeris::Target::JUPITER, {} },
  Entry { CELESTIAL_TARGET_SATURN, astro::ephemeris::Target::SATURN, {} },
  Entry { CELESTIAL_TARGET_URANUS, astro::ephemeris::Target::URANUS, {} },
  Entry { CELESTIAL_TARGET_NEPTUNE, astro::ephemeris::Target::NEPTUNE, {} },
  Entry { CELESTIAL_TARGET_PLUTO, astro::ephemeris::Target::PLUTO, {} },
  Entry { CELESTIAL_TARGET_MEAN_ASCENDING, astro::ephemeris::Target::MEAN_ASCENDING, {} },
  Entry { CELESTIAL_TARGET_MEAN_DESCENDING, astro::ephemeris::Target::MEAN_DESCENDING, {} },
  Entry { CELESTIAL_TARGET_TRUE_ASCENDING, astro::ephemeris::Target::TRUE_ASCENDING, {} },
  Entry { CELESTIAL_TARGET_TRUE_DESCENDING, astro::ephemeris::Target::TRUE_DESCENDING, {} },
};
// NOLINTEND(modernize-use-designated-initializers)

namespace detail {

/** @brief Enumerate named Delta T models so a new model cannot silently miss its boundary mapping. */
[[nodiscard]] constexpr auto is_known(const astro::delta_t::Model model) -> bool {
  using enum astro::delta_t::Model;
  switch (model) {
    case ALGO1:
    case ALGO2:
    case ALGO3:
    case ALGO4:
    case ALGO5:
      return true;
  }
  return false;
}

/** @brief Enumerate named nutation models independently of their C selector values. */
[[nodiscard]] constexpr auto is_known(const astro::earth::nutation::Model model) -> bool {
  using enum astro::earth::nutation::Model;
  switch (model) {
    case MEEUS:
    case IAU_1980:
      return true;
  }
  return false;
}

/** @brief Enumerate named providers; their C identities need not match core ordinals. */
[[nodiscard]] constexpr auto is_known(const astro::ephemeris::PositionSource source) -> bool {
  using enum astro::ephemeris::PositionSource;
  switch (source) {
    case SUN_VSOP87D:
    case MOON_ELP2000_82B_TRUNC:
    case PLANET_VSOP87D:
    case PLUTO_PLUTO95_APPARENT:
    case NODE_MEAN:
    case NODE_TRUE_MEEUS_FIVE_TERM:
      return true;
  }
  return false;
}

/** @brief Require exactly one mapping for every distinct named uint8_t model value. */
template <typename Value, std::size_t Size>
[[nodiscard]] consteval auto complete_models(const std::array<Entry<Value>, Size>& mapping) -> bool {
  for (uint16_t code = 0; code <= UINT8_MAX; ++code) {
    const auto value = static_cast<Value>(code);
    std::size_t rows = 0;
    for (const auto& entry : mapping) {
      rows += entry.value == value ? std::size_t { 1 } : std::size_t { 0 };
    }
    if (rows != (is_known(value) ? std::size_t { 1 } : std::size_t { 0 })) {
      return false;
    }
  }
  return true;
}

/** @brief Look up a full-width C code before converting it to a core enum. */
template <typename Value, std::size_t Size>
[[nodiscard]] constexpr auto from_code(
  const std::array<Entry<Value>, Size>& mapping,
  const uint32_t code
) -> const Entry<Value>& {
  for (const auto& entry : mapping) {
    if (entry.code == code) {
      return entry;
    }
  }
  throw std::invalid_argument { std::format("Unknown C code {} for the requested kind", code) };
}

/** @brief Look up a core value; an unmapped producer value is an internal mismatch. */
template <typename Value, std::size_t Size>
[[nodiscard]] constexpr auto from_value(
  const std::array<Entry<Value>, Size>& mapping,
  const Value value
) -> const Entry<Value>& {
  for (const auto& entry : mapping) {
    if (entry.value == value) {
      return entry;
    }
  }
  throw std::logic_error { "Core value has no C boundary identity" };
}

/** @brief Require a one-to-one mapping, independently of the underlying enum ordinals. */
template <typename Value, std::size_t Size>
[[nodiscard]] consteval auto unique(const std::array<Entry<Value>, Size>& mapping) -> bool {
  for (std::size_t first = 0; first < Size; ++first) {
    for (std::size_t second = first + 1; second < Size; ++second) {
      if (mapping.at(first).code == mapping.at(second).code or mapping.at(first).value == mapping.at(second).value) {
        return false;
      }
    }
  }
  return true;
}

/** @brief Every core target and its source must have one explicit boundary identity. */
[[nodiscard]] consteval auto complete_targets() -> bool {
  if (TARGETS.size() != astro::ephemeris::TARGET_FACTS.size()) {
    return false;
  }
  for (const auto& facts : astro::ephemeris::TARGET_FACTS) {
    static_cast<void>(from_value(TARGETS, facts.target));
    static_cast<void>(from_value(POSITION_SOURCES, facts.source));
  }
  return true;
}

static_assert(unique(DELTA_T_MODELS) and unique(NUTATION_MODELS) and unique(POSITION_SOURCES) and unique(TARGETS));
static_assert(complete_models(DELTA_T_MODELS) and complete_models(NUTATION_MODELS) and complete_models(POSITION_SOURCES));
static_assert(complete_targets());

} // namespace detail

/** @brief Resolve a Delta T selector; DEFAULT is input-only. */
[[nodiscard]] inline auto delta_t_model(const uint32_t code) -> astro::delta_t::Model {
  return code == CELESTIAL_DELTA_T_MODEL_DEFAULT
    ? astro::delta_t::DEFAULT_MODEL
    : detail::from_code(DELTA_T_MODELS, code).value;
}

/** @brief Resolve a house selector from the operation-owned nutation default. */
[[nodiscard]] inline auto nutation_model(const uint32_t code) -> astro::earth::nutation::Model {
  return code == CELESTIAL_NUTATION_MODEL_DEFAULT
    ? astro::house::DEFAULT_NUTATION_MODEL
    : detail::from_code(NUTATION_MODELS, code).value;
}

/** @brief Decode a target without narrowing the raw input code. */
[[nodiscard]] inline auto target(const uint32_t code) -> astro::ephemeris::Target {
  return detail::from_code(TARGETS, code).value;
}

/** @brief Encode a resolved Delta T identity, never DEFAULT. */
[[nodiscard]] inline auto code_of(const astro::delta_t::Model value) -> uint32_t {
  return detail::from_value(DELTA_T_MODELS, value).code;
}

/** @brief Encode a resolved nutation identity, never DEFAULT. */
[[nodiscard]] inline auto code_of(const astro::earth::nutation::Model value) -> uint32_t {
  return detail::from_value(NUTATION_MODELS, value).code;
}

/** @brief Encode the provider identity attached to a computed position. */
[[nodiscard]] inline auto code_of(const astro::ephemeris::PositionSource value) -> uint32_t {
  return detail::from_value(POSITION_SOURCES, value).code;
}

/** @brief Encode a core target for a caller-owned record. */
[[nodiscard]] inline auto code_of(const astro::ephemeris::Target value) -> uint32_t {
  return detail::from_value(TARGETS, value).code;
}

} // namespace lib::codes
