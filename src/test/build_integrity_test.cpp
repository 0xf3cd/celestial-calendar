/*
 * CelestialCalendar: 
 *   A C++23-style library that performs astronomical calculations and date conversions among various calendars,
 *   including Gregorian, Lunar, and Chinese Ganzhi calendars.
 * 
 * Copyright (C) 2026 Ningqi Wang (0xf3cd)
 * Email: nq.maigre@gmail.com
 * Repo : https://github.com/0xf3cd/celestial-calendar
 *  
 * This project is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This project is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this project. If not, see <https://www.gnu.org/licenses/>.
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
