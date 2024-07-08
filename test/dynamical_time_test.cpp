#include <gtest/gtest.h>
#include "dynamical_time.hpp"

namespace calendar::dynamical_time {

TEST(DynamicalTime, delta_t_algo1) {
  ASSERT_THROW(delta_t::algo1::compute(-4001), std::out_of_range);
  ASSERT_NEAR(delta_t::algo1::compute(500), 5710.0, 5.0);
  ASSERT_NEAR(delta_t::algo1::compute(1950),  29.0, 0.1);
  ASSERT_NEAR(delta_t::algo1::compute(2008),  66.0, 0.1);
}

}
