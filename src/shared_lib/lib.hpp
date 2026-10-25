/*
 * CelestialCalendar: 
 *   A C++23-style library that performs astronomical calculations and date conversions between
 *   Gregorian and Chinese Lunar calendars.
 * 
 * Copyright (C) 2024 Ningqi Wang (0xf3cd)
 * Email: nq.maigre@gmail.com
 * Repo : https://github.com/0xf3cd/celestial-calendar
 *  
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <algorithm>
#include <cstdint>
#include <format>
#include <functional>
#include <limits>
#include <new>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace lib {

/** @brief The failure category recorded by a C export. */
enum class ErrorKind : uint8_t {
  NONE = 0,
  INVALID_INPUT = 1,
  COMPUTATION = 2,
  RESOURCE = 3,
  INTERNAL = 4,
};

/** @brief Clear the calling thread's error kind and message. */
auto clear_last_error() noexcept -> void;

/**
 * @brief Record a failure without allowing message allocation to erase its kind.
 * @note Storage has one definition in lib.cpp; hidden inline TLS is not portable across shared-library TUs.
 */
auto set_last_error(ErrorKind kind, std::string_view message) noexcept -> void;

/** @brief Record an export-specific fallback for a non-standard exception. */
auto set_unknown_error(std::string_view operation) noexcept -> void;

/** @brief Read the calling thread's last-error message (empty if none). */
[[nodiscard]] auto last_error_message() noexcept -> const char*;

/** @brief Read the calling thread's error kind without clearing it. */
[[nodiscard]] auto last_error_kind() noexcept -> ErrorKind;

/** @brief Reject a null count and initialize its failure value before query validation or evaluation. */
inline auto check_count(uint32_t* const count, const std::string_view name) -> void {
  if (count == nullptr) {
    throw std::invalid_argument { std::format("Argument `{}` is null.", name) };
  }
  *count = 0;
}

/** @brief A null slot pointer is permitted only for a count-only call. */
inline auto check_slots(const auto* const slots, const uint32_t capacity) -> void {
  if (slots == nullptr and capacity > 0) {
    throw std::invalid_argument { "Argument `slots` is null, but `slot_count` is greater than 0." };
  }
}

/** @brief Size or copy UTF-8 text, counting the NUL and terminating any positive-capacity output. */
[[nodiscard]] inline auto copy_text(const std::string_view text, char* const buffer, const uint32_t capacity) -> uint32_t {
  if (buffer == nullptr and capacity > 0) {
    throw std::invalid_argument { "Argument `buffer` is null, but `capacity` is greater than 0." };
  }
  if (text.size() >= std::numeric_limits<uint32_t>::max()) {
    throw std::length_error { "Text size cannot be represented by the C ABI" };
  }

  if (capacity > 0) {
    const auto copied = std::min(text.size(), static_cast<std::size_t>(capacity - 1));
    const auto output = std::span { buffer, capacity };
    std::copy_n(text.cbegin(), copied, output.begin());
    output[copied] = '\0';
  }
  return static_cast<uint32_t>(text.size() + 1);
}

/** @brief Translate all exceptions from a C export to its zero-initialized failure sentinel. */
template <typename Function>
[[nodiscard]] inline auto wrap_export(
  const std::string_view operation,
  Function&& function
) noexcept -> std::invoke_result_t<Function> {
  using Result = std::invoke_result_t<Function>;

  clear_last_error();
  try {
    return std::invoke(std::forward<Function>(function));
  } catch (const std::invalid_argument& error) {
    set_last_error(ErrorKind::INVALID_INPUT, error.what());
  } catch (const std::out_of_range& error) {
    set_last_error(ErrorKind::INVALID_INPUT, error.what());
  } catch (const std::bad_alloc& error) {
    set_last_error(ErrorKind::RESOURCE, error.what());
  } catch (const std::runtime_error& error) {
    set_last_error(ErrorKind::COMPUTATION, error.what());
  } catch (const std::exception& error) {
    set_last_error(ErrorKind::INTERNAL, error.what());
  } catch (...) {
    set_unknown_error(operation);
  }
  return Result {};
}


} // namespace lib
