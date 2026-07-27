#include <gtest/gtest.h>
#include <print>
#include <atomic>
#include <ranges>
#include <thread>
#include <unordered_set>
#include "util.hpp"

namespace util::test {

using namespace util;

TEST(Util, ToYmd) {
  using namespace std::literals;

  ASSERT_EQ(to_ymd(1901, 1, 1), 1901y / 1 / 1);
  ASSERT_EQ(to_ymd(2024, 3, 15), 2024y / 3 / 15);
  ASSERT_EQ(to_ymd(2024.0F, 3, 15), 2024y / 3 / 15);
  ASSERT_EQ(to_ymd(2024U, 3, 15), 2024y / 3 / 15);
  ASSERT_EQ(to_ymd(2024LL, 3, 15), 2024y / 3 / 15);
  ASSERT_EQ(to_ymd(2024LLU, 3, 15), 2024y / 3 / 15);
  ASSERT_EQ(to_ymd(static_cast<int32_t>(2024), 3, 15), 2024y / 3 / 15);
  ASSERT_EQ(to_ymd(static_cast<int16_t>(2024), 3, 15), 2024y / 3 / 15);
}

TEST(Util, FromYmd) {
  using namespace std::literals;

  {
    const auto [y, m, d] = from_ymd(1901y / 1 / 1);
    ASSERT_EQ(y, 1901);
    ASSERT_EQ(m, 1);
    ASSERT_EQ(d, 1); 
  }

  {
    const auto [y, m, d] = from_ymd(2024y / 3 / 15);
    ASSERT_EQ(y, 2024);
    ASSERT_EQ(m, 3);
    ASSERT_EQ(d, 15); 
  }

  {
    const auto [y, m, d] = from_ymd(0y / 3 / 15);
    ASSERT_EQ(y, 0);
    ASSERT_EQ(m, 3);
    ASSERT_EQ(d, 15); 
  }
}

TEST(Util, OperatorAdd) {
  using namespace std::literals;
  using namespace util::ymd_operator;
  constexpr auto ymd = to_ymd(1901, 1, 1);

  ASSERT_EQ(ymd + std::chrono::days { -365 }, 1900y / 1 / 1);
  ASSERT_EQ(ymd + std::chrono::days { -1 }, 1900y / 12 / 31);
  ASSERT_EQ(ymd + std::chrono::days { 0 }, 1901y / 1 / 1);
  ASSERT_EQ(ymd + std::chrono::days { 1 }, 1901y / 1 / 2);
  ASSERT_EQ(ymd + std::chrono::days { 365 }, 1902y / 1 / 1);

  ASSERT_EQ(ymd + (-365), 1900y / 1 / 1);
  ASSERT_EQ(ymd + (-1), 1900y / 12 / 31);
  ASSERT_EQ(ymd + 0, 1901y / 1 / 1);
  ASSERT_EQ(ymd + 1, 1901y / 1 / 2);
  ASSERT_EQ(ymd + 365, 1902y / 1 / 1);

  ASSERT_EQ(-365 + ymd, 1900y / 1 / 1);
  ASSERT_EQ(-1 + ymd, 1900y / 12 / 31);
  ASSERT_EQ(0 + ymd, 1901y / 1 / 1);
  ASSERT_EQ(1 + ymd, 1901y / 1 / 2);
  ASSERT_EQ(365 + ymd, 1902y / 1 / 1);
}

TEST(Util, OperatorSub) {
  using namespace std::literals;
  using namespace util::ymd_operator;
  
  constexpr auto ymd = to_ymd(1901, 1, 1);

  ASSERT_EQ(ymd - std::chrono::days { 365 }, 1900y / 1 / 1);
  ASSERT_EQ(ymd - std::chrono::days { 1 }, 1900y / 12 / 31);
  ASSERT_EQ(ymd - std::chrono::days { 0 }, 1901y / 1 / 1);
  ASSERT_EQ(ymd - std::chrono::days { -1 }, 1901y / 1 / 2);
  ASSERT_EQ(ymd - std::chrono::days { -365 }, 1902y / 1 / 1);

  ASSERT_EQ(ymd - 365, 1900y / 1 / 1);
  ASSERT_EQ(ymd - 1, 1900y / 12 / 31);
  ASSERT_EQ(ymd - 0, 1901y / 1 / 1);
  ASSERT_EQ(ymd - (-1), 1901y / 1 / 2);
  ASSERT_EQ(ymd - (-365), 1902y / 1 / 1);
}

TEST(Util, GenRandomValue1) {
  for (size_t i = 0; i < 5000; i++) {
    const auto random_value = random<double>();
    ASSERT_GE(random_value, std::numeric_limits<double>::min());
    ASSERT_LE(random_value, std::numeric_limits<double>::max());
  }

  for (size_t i = 0; i < 5000; i++) {
    const auto random_value = random<uint8_t>();
    ASSERT_GE(random_value, std::numeric_limits<uint8_t>::min());
    ASSERT_LE(random_value, std::numeric_limits<uint8_t>::max());
  }

  for (size_t i = 0; i < 5000; i++) {
    const auto random_value = random<int16_t>();
    ASSERT_GE(random_value, std::numeric_limits<int16_t>::min());
    ASSERT_LE(random_value, std::numeric_limits<int16_t>::max());
  }
}


TEST(Util, GenRandomValue2) {
  for (size_t i = 0; i < 5000; i++) {
    // Not sure if the subtest of float is 100% correct.
    // Maybe an epsilon is needed when comparing?
    const auto random_value1 = random<float>();
    const auto random_value2 = random<float>();
    if (random_value1 == random_value2) {
      continue;
    }
    
    const auto random_value3 = random<float>(
      std::min(random_value1, random_value2), 
      std::max(random_value1, random_value2)
    );
    ASSERT_GE(random_value3, std::min(random_value1, random_value2));
    ASSERT_LE(random_value3, std::max(random_value1, random_value2));
  }

  for (size_t i = 0; i < 5000; i++) {
    const auto random_value1 = random<uint64_t>();
    const auto random_value2 = random<uint64_t>();
    if (random_value1 == random_value2) {
      continue;
    }

    const auto random_value3 = random<uint64_t>(
      std::min(random_value1, random_value2), 
      std::max(random_value1, random_value2)
    );
    ASSERT_GE(random_value3, std::min(random_value1, random_value2));
    ASSERT_LT(random_value3, std::max(random_value1, random_value2));
  }

  for (size_t i = 0; i < 100; i++) {
    const auto gap = random<uint16_t>(1, 20);
    const auto random_value1 = std::invoke([&] {
      while (true) {
        const auto random_value = random<uint16_t>();
        if (random_value < std::numeric_limits<uint16_t>::max() - gap) {
          return random_value;
        }
      }
    });
    const auto random_value2 = random_value1 + gap;

    for (size_t j = 0; j < 100; j++) {
      const auto random_value3 = random<uint16_t>(random_value1, random_value2);
      ASSERT_GE(random_value3, random_value1);
      ASSERT_LE(random_value3, random_value2);
    }
  }
}


TEST(Util, TupleHash) {
  { // Test double, int32_t
    std::unordered_set<std::size_t> s;
    s.insert(hash::hash(0.0, 0));
    s.insert(hash::hash(0.1, 0));
    s.insert(hash::hash(0.1, 1));
    s.insert(hash::hash(0.1, 1));
    ASSERT_EQ(s.size(), 3);
  }

  { // Test int64_t, float, std::string
    std::unordered_set<std::size_t> s;
    s.insert(hash::hash(0LL, 0.0F, ""));
    s.insert(hash::hash(0LL, 0.1F, ""));
    s.insert(hash::hash(0LL, 0.1F, "a"));
    s.insert(hash::hash(0LL, 0.1F, "b"));
    s.insert(hash::hash(0LL, 0.1F, "b"));
    ASSERT_EQ(s.size(), 4);
  }

  { // Test tuple
    std::unordered_set<std::size_t> s;
    s.insert(hash::hash(std::make_tuple(0, 0.0F, "")));
    s.insert(hash::hash(std::make_tuple(0, 0.1F, "")));
    s.insert(hash::hash(std::make_tuple(0, 0.1F, "a")));
    s.insert(hash::hash(std::make_tuple(0, 0.1F, "b")));
    s.insert(hash::hash(std::make_tuple(5, 0.1F, "b")));
    ASSERT_EQ(s.size(), 5);
  }
}


TEST(Util, HashCollision) {
  constexpr auto try_count = 80000;

  std::unordered_set<std::tuple<int, double, float>, hash::TupleHash<int, double, float>> tuples;
  std::unordered_set<std::size_t> hash_values;

  for (auto _ = 0; _ < try_count; _++) {
    const auto v1 = util::random<int>();
    const auto v2 = util::random<double>();
    const auto v3 = util::random<float>();
    tuples.emplace(v1, v2, v3);
    hash_values.insert(hash::hash(v1, v2, v3));
  }

  std::println("{} unique tuples", tuples.size());
  std::println("{} unique hash values", hash_values.size());
  std::println("{} collisions", tuples.size() - hash_values.size());

  ASSERT_NEAR(tuples.size(), hash_values.size(), try_count * 0.00005);
}


TEST(Util, MakeCached1) {
  const auto f = [](int a, int b) {
    std::this_thread::sleep_for(std::chrono::microseconds(50));
    return a + b;
  };
  const auto cached_f = util::cache::cache_func(f);

  // Time the original function.
  const auto original_start_time = std::chrono::steady_clock::now();
  std::vector<int> original_results;
  for (int i = 0; i < 10; i++) {
    for (int j = 0; j < 10; j++) {
      original_results.emplace_back(f(i, j));
      original_results.emplace_back(f(i, j));
      original_results.emplace_back(f(i, j));
    }
  }
  const auto original_end_time = std::chrono::steady_clock::now();
  const auto original_elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(original_end_time - original_start_time).count();

  // Time the cached function.
  const auto cached_start_time = std::chrono::steady_clock::now();
  std::vector<int> cached_results;
  for (int i = 0; i < 10; i++) {
    for (int j = 0; j < 10; j++) {
      cached_results.emplace_back(cached_f(i, j));
      cached_results.emplace_back(cached_f(i, j));
      cached_results.emplace_back(cached_f(i, j));
    }
  }
  const auto cached_end_time = std::chrono::steady_clock::now();
  const auto cached_elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(cached_end_time - cached_start_time).count();

  ASSERT_EQ(original_results, cached_results);

  ASSERT_GT(original_elapsed_time, cached_elapsed_time);
  std::println("original_elapsed_time = {}ms, cached_elapsed_time = {}ms, {}x faster",
               original_elapsed_time, cached_elapsed_time, 
               static_cast<double>(original_elapsed_time) / static_cast<double>(cached_elapsed_time));
}


TEST(Util, MakeCached2) {
  const auto f = [](int a, double b) {
    std::this_thread::sleep_for(std::chrono::microseconds(50));
    return a * b;
  };
  const auto cached_f = util::cache::cache_func(f);

  // Time the original function.
  const auto original_start_time = std::chrono::steady_clock::now();
  std::vector<double> original_results;
  for (int i = 0; i < 10; i++) {
    for (int j = 0; j < 10; j++) {
      original_results.emplace_back(f(i, j));
      original_results.emplace_back(f(i, j));
      original_results.emplace_back(f(i, j));
    }
  }
  const auto original_end_time = std::chrono::steady_clock::now();
  const auto original_elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(original_end_time - original_start_time).count();

  // Time the cached function.
  const auto cached_start_time = std::chrono::steady_clock::now();
  std::vector<double> cached_results;
  for (int i = 0; i < 10; i++) {
    for (int j = 0; j < 10; j++) {
      cached_results.emplace_back(cached_f(i, j));
      cached_results.emplace_back(cached_f(i, j));
      cached_results.emplace_back(cached_f(i, j));
    }
  }
  const auto cached_end_time = std::chrono::steady_clock::now();
  const auto cached_elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(cached_end_time - cached_start_time).count();

  ASSERT_EQ(original_results, cached_results);

  ASSERT_GT(original_elapsed_time, cached_elapsed_time);
  std::println("original_elapsed_time = {}ms, cached_elapsed_time = {}ms, {}x faster",
               original_elapsed_time, cached_elapsed_time,
               static_cast<double>(original_elapsed_time) / static_cast<double>(cached_elapsed_time));
}


TEST(Util, MakeCachedCopyShares) {
  // #78: copies of a cached function must share one cache, not fork divergent replicas.
  std::atomic<int32_t> call_count { 0 };
  const auto f = [&call_count](int32_t a) {
    ++call_count;
    return a * 2;
  };
  const auto cached_f = util::cache::cache_func(f);

  ASSERT_EQ(cached_f(21), 42);
  ASSERT_EQ(call_count, 1);

  const auto copied_f = cached_f; // NOLINT(performance-unnecessary-copy-initialization): the copy is the point.
  ASSERT_EQ(copied_f(21), 42);
  ASSERT_EQ(call_count, 1); // Hit through the copy — no recomputation.

  ASSERT_EQ(copied_f(4), 8);
  ASSERT_EQ(call_count, 2);
  ASSERT_EQ(cached_f(4), 8);
  ASSERT_EQ(call_count, 2); // The original sees the copy's insertion.
}


TEST(Util, MakeCachedThreadSafe) {
  // #78: concurrent callers used to race on the captured `unordered_map` (UB, TSAN-verified).
  std::atomic<int32_t> call_count { 0 };
  const auto f = [&call_count](int32_t a) {
    ++call_count;
    return a * a;
  };
  const auto cached_f = util::cache::cache_func(f);

  constexpr int32_t THREAD_COUNT = 8;
  constexpr int32_t KEY_COUNT = 64;
  constexpr int32_t ROUND_COUNT = 4;

  // gtest assertions are not thread-safe everywhere — threads only record, main thread asserts.
  std::vector<std::vector<int32_t>> results(THREAD_COUNT);

  {
    std::vector<std::thread> threads;
    threads.reserve(THREAD_COUNT);
    for (int32_t t = 0; t < THREAD_COUNT; ++t) {
      threads.emplace_back([&cached_f, &results, t] {
        auto& thread_results = results[t];
        thread_results.reserve(static_cast<size_t>(ROUND_COUNT) * KEY_COUNT);
        for (int32_t round = 0; round < ROUND_COUNT; ++round) {
          for (int32_t k = 0; k < KEY_COUNT; ++k) {
            thread_results.emplace_back(cached_f(k));
          }
        }
      });
    }
    for (auto& thread : threads) {
      thread.join();
    }
  }

  for (const auto& thread_results : results) {
    ASSERT_EQ(thread_results.size(), ROUND_COUNT * KEY_COUNT);
    for (int32_t round = 0; round < ROUND_COUNT; ++round) {
      for (int32_t k = 0; k < KEY_COUNT; ++k) {
        ASSERT_EQ(thread_results[(round * KEY_COUNT) + k], k * k);
      }
    }
  }

  // Every key computed at least once; concurrent misses on a key may compute a few extras.
  ASSERT_GE(call_count, KEY_COUNT);
  ASSERT_LE(call_count, THREAD_COUNT * KEY_COUNT);
}

} // namespace util::test
