/**
 * ChineseCalendar: A C++ library that deals with conversions between calendar systems.
 * Copyright (C) 2024 Ningqi Wang (0xf3cd) <https://github.com/0xf3cd>
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

// Codes stolen from my other repos...
// Mostly used for tests.

#include <random>
#include <limits>

namespace util {

/*!
 * @fn random
 * @brief Generate a random value of type T.
 * @return a random value of type T.
 */
template <typename T>
  requires std::integral<T> || std::floating_point<T>
T random() {
  std::random_device rd;
  std::mt19937 gen(rd());

  constexpr auto min = std::numeric_limits<T>::min();
  constexpr auto max = std::numeric_limits<T>::max();

  if constexpr (std::integral<T>) {
    std::uniform_int_distribution<T> dist { min, max };
    return dist(gen);
  } else {
    static_assert(std::floating_point<T>);
    std::uniform_real_distribution<T> dist { min, max };
    return dist(gen);
  }
}


/*!
 * @fn random - bool
 * @brief Generate a random bool value.
 * @return a random bool value.
 */
template <>
bool random() {
  static std::random_device rd;
  static std::mt19937 gen(rd());
  std::bernoulli_distribution dist(0.5);
  return dist(gen);
}


/*!
 * @fn random
 * @brief Generate a random value of type T within the specified range [min, max].
 * @param min the lower bound of the range, inclusive.
 * @param max the upper bound of the range, inclusive.
 * @return a random value of type T.
 */
template <typename T>
  requires std::integral<T> || std::floating_point<T>
T random(const T& min, const T& max) {
  assert(min < max);
  std::random_device rd;
  std::mt19937 gen(rd());

  if constexpr (std::integral<T>) {
    std::uniform_int_distribution<T> dist { min, max };
    return dist(gen);
  } else {
    static_assert(std::floating_point<T>);
    std::uniform_real_distribution<T> dist { min, max };
    return dist(gen);
  }
}

} // namespace util
