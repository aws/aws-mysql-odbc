// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0
// (GPLv2), as published by the Free Software Foundation, with the
// following additional permissions:
//
// This program is distributed with certain software that is licensed
// under separate terms, as designated in a particular file or component
// or in the license documentation. Without limiting your rights under
// the GPLv2, the authors of this program hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have included with the program.
//
// Without limiting the foregoing grant of rights under the GPLv2 and
// additional permission as to separately licensed software, this
// program is also subject to the Universal FOSS Exception, version 1.0,
// a copy of which can be found along with its FAQ at
// http://oss.oracle.com/licenses/universal-foss-exception.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see
// http://www.gnu.org/licenses/gpl-2.0.html.

#include "driver/sliding_expiration_cache_with_clean_up_thread.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>
#include <thread>

namespace {
const std::string cache_key_a("key_a");
const std::string cache_key_b("key_b");
const std::string cache_val_a("val_a");
const std::string cache_val_b("val_b");
constexpr long long cache_exp_short(3000000000);  // 3 seconds
constexpr long long cache_exp_mid(5000000000);    // 5 seconds
constexpr long long cache_exp_long(10000000000);  // 10 seconds
}  // namespace

class SlidingExpirationCacheTest : public testing::Test {
 protected:
  static void SetUpTestSuite() {}
  static void TearDownTestSuite() {}
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(SlidingExpirationCacheTest, PutCache) {
  SLIDING_EXPIRATION_CACHE<std::string, std::string> cache;
  EXPECT_EQ(0, cache.size());
  cache.put(cache_key_a, cache_val_a, cache_exp_short);
  cache.put(cache_key_b, cache_val_b, cache_exp_mid);
  EXPECT_EQ(2, cache.size());

  EXPECT_STREQ(cache_val_a.c_str(), cache.get(cache_key_a, cache_exp_long, "").c_str());
  EXPECT_STREQ(cache_val_b.c_str(), cache.get(cache_key_b, cache_exp_long, "").c_str());
  cache.clear();
}

TEST_F(SlidingExpirationCacheTest, ComputeIfAbsent) {
  SLIDING_EXPIRATION_CACHE<std::string, std::string> cache;
  EXPECT_EQ(0, cache.size());
  cache.compute_if_absent(
      cache_key_a, [](const auto&) { return "a"; }, cache_exp_short);
  cache.put(cache_key_b, cache_val_b, cache_exp_short);

  cache.compute_if_absent(
      cache_key_b, [](const auto&) { return "b"; }, cache_exp_mid);
  EXPECT_EQ(2, cache.size());

  EXPECT_STREQ("a", cache.get(cache_key_a, cache_exp_long, "").c_str());
  EXPECT_STREQ(cache_val_b.c_str(), cache.get(cache_key_b, cache_exp_long, "").c_str());
  cache.clear();
}

TEST_F(SlidingExpirationCacheTest, PutCacheUpdate) {
  SLIDING_EXPIRATION_CACHE<std::string, std::string> cache;
  cache.set_clean_up_interval_nanos(0);

  EXPECT_EQ(0, cache.size());
  cache.put(cache_key_a, cache_val_a, cache_exp_short);
  cache.put(cache_key_b, cache_val_b, cache_exp_mid);
  EXPECT_EQ(2, cache.size());
  EXPECT_STREQ(cache_val_b.c_str(), cache.get(cache_key_b, cache_exp_long, "").c_str());

  cache.put(cache_key_b, cache_val_b, cache_exp_long);
  std::this_thread::sleep_for(std::chrono::nanoseconds(cache_exp_mid));
  EXPECT_STREQ(cache_val_b.c_str(), cache.get(cache_key_b, cache_exp_long, "").c_str());
  EXPECT_EQ(1, cache.size());
  cache.clear();
}

TEST_F(SlidingExpirationCacheTest, GetCacheMiss) {
  SLIDING_EXPIRATION_CACHE<std::string, std::string> cache;
  EXPECT_STREQ("", cache.get(cache_key_a, cache_exp_long, "").c_str());
  cache.clear();
}

TEST_F(SlidingExpirationCacheTest, GetCacheExpire) {
  SLIDING_EXPIRATION_CACHE<std::string, std::string> cache;
  cache.set_clean_up_interval_nanos(0);

  EXPECT_EQ(0, cache.size());
  cache.put(cache_key_a, cache_val_a, cache_exp_short);
  EXPECT_EQ(1, cache.size());
  std::this_thread::sleep_for(std::chrono::nanoseconds(cache_exp_mid));
  EXPECT_STREQ("", cache.get(cache_key_a, cache_exp_short, "").c_str());
  EXPECT_EQ(0, cache.size());
  cache.clear();
}

TEST_F(SlidingExpirationCacheTest, CacheClear) {
  SLIDING_EXPIRATION_CACHE<std::string, std::string> cache;
  cache.put(cache_key_a, cache_val_a, cache_exp_short);
  cache.put(cache_key_b, cache_val_b, cache_exp_short);
  EXPECT_EQ(2, cache.size());
  cache.clear();
  EXPECT_EQ(0, cache.size());
  cache.clear();
}

TEST_F(SlidingExpirationCacheTest, ExpirationTimeUpdateGet) {
  SLIDING_EXPIRATION_CACHE<std::string, std::string> cache;
  cache.set_clean_up_interval_nanos(0);

  EXPECT_EQ(0, cache.size());
  cache.put(cache_key_a, cache_val_a, cache_exp_mid);
  cache.put(cache_key_b, cache_val_b, cache_exp_mid);
  EXPECT_EQ(2, cache.size());

  EXPECT_STREQ(cache_val_a.c_str(), cache.get(cache_key_a, cache_exp_long, "").c_str());
  std::this_thread::sleep_for(std::chrono::nanoseconds(cache_exp_mid));
  EXPECT_STREQ(cache_val_a.c_str(), cache.get(cache_key_a, cache_exp_long, "").c_str());
  EXPECT_EQ(1, cache.size());
  cache.clear();
}

TEST_F(SlidingExpirationCacheTest, GetCacheExpireThread) {
  SLIDING_EXPIRATION_CACHE_WITH_CLEAN_UP_THREAD<std::string, std::string> cache(cache_exp_short);

  EXPECT_EQ(0, cache.size());
  cache.put(cache_key_a, cache_val_a, cache_exp_short);
  cache.put(cache_key_b, cache_val_b, cache_exp_long);
  EXPECT_EQ(2, cache.size());
  std::this_thread::sleep_for(std::chrono::nanoseconds(cache_exp_mid));
  EXPECT_EQ(1, cache.size());
  cache.clear();
  cache.release_resources();
}
