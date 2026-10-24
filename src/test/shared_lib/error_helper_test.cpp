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
#include <cstdint>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <utility>

#include <gtest/gtest.h>

// Private error storage is not a DSO export; compile its real TU into this helper-only test.
#define CELESTIAL_BUILDING_DLL
#include "../../shared_lib/lib.cpp"

namespace lib::test {

static_assert(CELESTIAL_ERROR_NONE == 0);
static_assert(CELESTIAL_ERROR_INVALID_INPUT == 1);
static_assert(CELESTIAL_ERROR_COMPUTATION == 2);
static_assert(CELESTIAL_ERROR_RESOURCE == 3);
static_assert(CELESTIAL_ERROR_INTERNAL == 4);

TEST(ErrorHelper, EveryExceptionCategoryReturnsTheFailureSentinel) {
  using ThrowingCall = int32_t (*)();
  const std::array<std::pair<uint32_t, ThrowingCall>, 9> cases {{
    { CELESTIAL_ERROR_INVALID_INPUT, []() -> int32_t { throw std::invalid_argument { "invalid input" }; } },
    { CELESTIAL_ERROR_INVALID_INPUT, []() -> int32_t { throw std::out_of_range { "outside range" }; } },
    { CELESTIAL_ERROR_RESOURCE, []() -> int32_t { throw std::bad_alloc {}; } },
    { CELESTIAL_ERROR_RESOURCE, []() -> int32_t { throw std::bad_array_new_length {}; } },
    { CELESTIAL_ERROR_COMPUTATION, []() -> int32_t { throw std::runtime_error { "no convergence" }; } },
    { CELESTIAL_ERROR_INTERNAL, []() -> int32_t { throw std::logic_error { "broken invariant" }; } },
    { CELESTIAL_ERROR_INTERNAL, []() -> int32_t { throw std::length_error { "invalid internal size" }; } },
    { CELESTIAL_ERROR_INTERNAL, []() -> int32_t { throw std::exception {}; } },
    { CELESTIAL_ERROR_INTERNAL, []() -> int32_t { throw int32_t { 17 }; } },
  }};
  for (const auto& [kind, function] : cases) {
    set_last_error(ErrorKind::COMPUTATION, "stale message");
    EXPECT_EQ(wrap_export("exception_category_probe", function), 0);
    EXPECT_EQ(celestial_last_error_kind(), kind);
    EXPECT_STRNE(celestial_last_error(), "");
    EXPECT_NE(std::string { celestial_last_error() }, "stale message");
    EXPECT_EQ(static_cast<uint32_t>(last_error_kind()), kind);
  }
  EXPECT_NE(std::string { celestial_last_error() }.find("exception_category_probe"), std::string::npos);
}

TEST(ErrorHelper, EmptyMessageStillRecordsTheFailureKind) {
  EXPECT_EQ(wrap_export("empty_message_probe", []() -> uint32_t {
    throw std::invalid_argument { "" };
  }), 0U);
  EXPECT_STREQ(celestial_last_error(), "");
  EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
  EXPECT_STREQ(celestial_last_error(), "");
  EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INVALID_INPUT);
}

TEST(ErrorHelper, FailureSentinelsMatchTheReturnShape) {
  const auto record = wrap_export("record_probe", []() -> CelestialJulianDay {
    throw std::runtime_error { "record computation" };
  });
  EXPECT_FALSE(record.valid);
  EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_COMPUTATION);
  const auto flag = wrap_export("flag_probe", []() -> bool {
    throw std::logic_error { "flag invariant" };
  });
  EXPECT_FALSE(flag);
  EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_INTERNAL);
}

TEST(ErrorHelper, BothReadersPreserveTheKindMessageAndPointer) {
  set_last_error(ErrorKind::RESOURCE, "resource failure");
  const char* const pointer = celestial_last_error();
  for (const uint32_t repeat : { 0U, 1U, 2U }) {
    SCOPED_TRACE(repeat);
    EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_RESOURCE);
    EXPECT_EQ(celestial_last_error(), pointer);
    EXPECT_STREQ(celestial_last_error(), "resource failure");
    EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_RESOURCE);
  }
}

TEST(ErrorHelper, SuccessfulRecordingCallClearsBothFieldsBeforeExecution) {
  set_last_error(ErrorKind::INTERNAL, "old failure");
  const auto result = wrap_export("successful_probe", []() -> int32_t {
    EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_NONE);
    EXPECT_STREQ(celestial_last_error(), "");
    return 37;
  });
  EXPECT_EQ(result, 37);
  EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_NONE);
  EXPECT_STREQ(celestial_last_error(), "");
  EXPECT_EQ(wrap_export("legal_empty_probe", []() -> uint32_t { return 0; }), 0U);
  EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_NONE);
}

TEST(ErrorHelper, KindAndMessageAreThreadLocal) {
  set_last_error(ErrorKind::RESOURCE, "parent failure");
  std::thread worker { [] {
    EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_NONE);
    EXPECT_STREQ(celestial_last_error(), "");
    EXPECT_EQ(wrap_export("worker_probe", []() -> uint32_t {
      throw std::runtime_error { "worker computation" };
    }), 0U);
    EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_COMPUTATION);
    EXPECT_STREQ(celestial_last_error(), "worker computation");
    EXPECT_EQ(wrap_export("worker_success", []() -> uint32_t { return 1; }), 1U);
    EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_NONE);
    EXPECT_STREQ(celestial_last_error(), "");
  } };
  worker.join();
  EXPECT_EQ(celestial_last_error_kind(), CELESTIAL_ERROR_RESOURCE);
  EXPECT_STREQ(celestial_last_error(), "parent failure");
  clear_last_error();
}

TEST(ErrorHelper, TextCopyCountsNulAndPreservesCallerStorageOutsideCapacity) {
  constexpr std::string_view TEXT = "UTF-8: 立春";
  EXPECT_EQ(copy_text(TEXT, nullptr, 0), TEXT.size() + 1);
  for (const uint32_t capacity : { 1U, 2U, static_cast<uint32_t>(TEXT.size() + 1) }) {
    std::string output(TEXT.size() + 2, '!');
    EXPECT_EQ(copy_text(TEXT, output.data(), capacity), TEXT.size() + 1);
    EXPECT_EQ(output.at(capacity - 1), '\0');
    EXPECT_EQ(output.at(capacity), '!');
    EXPECT_EQ(output.substr(0, capacity - 1), TEXT.substr(0, capacity - 1));
  }
  EXPECT_EQ(copy_text("", nullptr, 0), 1U);
  EXPECT_THROW(std::ignore = copy_text(TEXT, nullptr, 1), std::invalid_argument);
}

} // namespace lib::test
