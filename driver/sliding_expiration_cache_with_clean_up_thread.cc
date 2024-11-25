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

#include "sliding_expiration_cache_with_clean_up_thread.h"

#include <string>
#include <utility>

#include "custom_endpoint_monitor.h"

template <class K, class V>
void SLIDING_EXPIRATION_CACHE_WITH_CLEAN_UP_THREAD<K, V>::init_clean_up_thread() {
  if (!this->is_initialized) {
    std::unique_lock lock(mutex_);
    if (!this->is_initialized) {
      this->clean_up_thread_pool.resize(this->clean_up_thread_pool.size() + 1);

      this->clean_up_thread_pool.push([=](int id) {
        while (!should_stop) {
          const std::chrono::nanoseconds clean_up_interval = std::chrono::nanoseconds(this->clean_up_interval_nanos);
          std::this_thread::sleep_for(clean_up_interval);
          this->clean_up_time_nanos.store(std::chrono::steady_clock::now() + clean_up_interval);
          std::vector<K> keys;
          keys.reserve(this->cache.size());
          for (auto& [key, cache_item] : this->cache) {
            keys.push_back(key);
          }
          for (const auto& key : keys) {
            this->remove_if_expired(key);
          }
        }
      });
      this->is_initialized = true;
    }
  }
}

template <class K, class V>
SLIDING_EXPIRATION_CACHE_WITH_CLEAN_UP_THREAD<K, V>::SLIDING_EXPIRATION_CACHE_WITH_CLEAN_UP_THREAD() {
  this->init_clean_up_thread();
}

template <class K, class V>
SLIDING_EXPIRATION_CACHE_WITH_CLEAN_UP_THREAD<K, V>::SLIDING_EXPIRATION_CACHE_WITH_CLEAN_UP_THREAD(
    std::shared_ptr<SHOULD_DISPOSE_FUNC<V>> should_dispose_func,
    std::shared_ptr<ITEM_DISPOSAL_FUNC<V>> item_disposal_func)
    : SLIDING_EXPIRATION_CACHE<K, V>(std::move(should_dispose_func), std::move(item_disposal_func)) {
  this->init_clean_up_thread();
}

template <class K, class V>
SLIDING_EXPIRATION_CACHE_WITH_CLEAN_UP_THREAD<K, V>::SLIDING_EXPIRATION_CACHE_WITH_CLEAN_UP_THREAD(
    std::shared_ptr<SHOULD_DISPOSE_FUNC<V>> should_dispose_func,
    std::shared_ptr<ITEM_DISPOSAL_FUNC<V>> item_disposal_func, long long clean_up_interval_nanos)
    : SLIDING_EXPIRATION_CACHE<K, V>(std::move(should_dispose_func), std::move(item_disposal_func), clean_up_interval_nanos) {
  this->init_clean_up_thread();
}

#ifdef UNIT_TEST_BUILD
template <class K, class V>
SLIDING_EXPIRATION_CACHE_WITH_CLEAN_UP_THREAD<K, V>::SLIDING_EXPIRATION_CACHE_WITH_CLEAN_UP_THREAD(
    long long clean_up_interval_nanos) {
  this->clean_up_interval_nanos = clean_up_interval_nanos;
  this->init_clean_up_thread();
}
#endif

template <class K, class V>
void SLIDING_EXPIRATION_CACHE_WITH_CLEAN_UP_THREAD<K, V>::release_resources() {
  this->should_stop = true;
  this->clean_up_thread_pool.stop(true);
  this->clean_up_thread_pool.resize(0);
  this->is_initialized = false;
}

template class SLIDING_EXPIRATION_CACHE_WITH_CLEAN_UP_THREAD<std::string, std::string>;
template class SLIDING_EXPIRATION_CACHE_WITH_CLEAN_UP_THREAD<std::string, std::shared_ptr<CUSTOM_ENDPOINT_MONITOR>>;
