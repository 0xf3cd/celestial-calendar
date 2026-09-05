/*
 * CelestialCalendar: 
 *   A C++23-style library that performs astronomical calculations and date conversions among various calendars,
 *   including Gregorian, Lunar, and Chinese Ganzhi calendars.
 * 
 * Copyright (C) 2026 Ningqi Wang (0xf3cd)
 * Email: nq.maigre@gmail.com
 * Repo : https://github.com/0xf3cd/celestial-calendar
 *  
 * SPDX-License-Identifier: MIT
 */

#include <gtest/gtest.h>

namespace build::test {

// #89: The default build type is Release (automation/build.py), which defines NDEBUG and turns
// every `assert` in the library headers into dead code — even inside test binaries.
// `src/test/CMakeLists.txt` strips NDEBUG for test targets; this test fails loudly if that
// ever regresses (e.g. someone reorders or removes the -UNDEBUG option).
TEST(BuildIntegrity, AssertionsAreLive) {
#ifdef NDEBUG
  FAIL() << "Test binary built with NDEBUG: every internal-invariant `assert` is dead code here.";
#else
  SUCCEED();
#endif
}

} // namespace build::test
