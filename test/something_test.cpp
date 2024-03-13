#include <gtest/gtest.h>
#include "something.hpp"

namespace {

TEST(SomethingTest, SomeTest) {
  EXPECT_EQ(0, someFunc());
}

}
