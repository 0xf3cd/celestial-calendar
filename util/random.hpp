// Codes stolen from my other repos...

#ifndef __UTIL_RANDOM_HPP__
#define __UTIL_RANDOM_HPP__

#include <random>
#include <limits>

namespace util {

/*!
 * @fn gen_random_value
 * @brief Generate a random value of type T.
 * @return a random value of type T.
 */
template <typename T>
  requires std::integral<T> || std::floating_point<T>
T gen_random_value() {
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
 * @fn gen_random_value - bool
 * @brief Generate a random bool value.
 * @return a random bool value.
 */
template <>
bool gen_random_value() {
  return gen_random_value<int>() % 2 == 0;
}


/*!
 * @fn gen_random_value
 * @brief Generate a random value of type T within the specified range [min, max].
 * @param min the lower bound of the range, inclusive.
 * @param max the upper bound of the range, inclusive.
 * @return a random value of type T.
 */
template <typename T>
  requires std::integral<T> || std::floating_point<T>
T gen_random_value(const T& min, const T& max) {
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

#endif // __UTIL_RANDOM_HPP__
