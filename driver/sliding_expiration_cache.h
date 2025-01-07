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

#ifndef __SLIDING_EXPIRATION_CACHE__
#define __SLIDING_EXPIRATION_CACHE__

#include <atomic>
#include <chrono>
#include <functional>
#include <unordered_map>

template <class T>
class SHOULD_DISPOSE_FUNC {
 public:
  virtual bool should_dispose(T item);
};

template <class T>
class ITEM_DISPOSAL_FUNC {
 public:
  virtual void dispose(T item);
};

template <class K, class V>
class SLIDING_EXPIRATION_CACHE {
 public:
  class CACHE_ITEM {
   public:
    CACHE_ITEM() = default;
    CACHE_ITEM(V item, std::chrono::steady_clock::time_point expiration_time)
        : item(item), expiration_time(expiration_time){};
    ~CACHE_ITEM() = default;
    V item;

    CACHE_ITEM* with_extend_expiration(long long item_expiration_nanos) {
      this->expiration_time = std::chrono::steady_clock::now() + std::chrono::nanoseconds(item_expiration_nanos);
      return this;
    }

    bool should_clean_up(SHOULD_DISPOSE_FUNC<V>* should_dispose_func) {
      if (should_dispose_func != nullptr) {
        return std::chrono::steady_clock::now() > this->expiration_time &&
               should_dispose_func->should_dispose(this->item);
      }

      return std::chrono::steady_clock::now() > this->expiration_time;
    }

   private:
    std::chrono::steady_clock::time_point expiration_time;
  };

  SLIDING_EXPIRATION_CACHE() {
    this->should_dispose_func = nullptr;
    this->item_disposal_func = nullptr;
  }

  SLIDING_EXPIRATION_CACHE(SHOULD_DISPOSE_FUNC<V>* should_dispose_func, ITEM_DISPOSAL_FUNC<V>* item_disposal_func)
      : should_dispose_func(should_dispose_func), item_disposal_func(item_disposal_func){};
  SLIDING_EXPIRATION_CACHE(SHOULD_DISPOSE_FUNC<V>* should_dispose_func, ITEM_DISPOSAL_FUNC<V>* item_disposal_func,
                           long long clean_up_interval_nanos)
      : clean_up_interval_nanos(clean_up_interval_nanos),
        should_dispose_func(should_dispose_func),
        item_disposal_func(item_disposal_func){};

  V compute_if_absent(K key, std::function<K(V)> mapping_function, long long item_expiration_nanos);

  V put(K key, V value, long long item_expiration_nanos);
  V get(K key, long long item_expiration_nanos, V default_value);
  void remove(K key);

  /**
   * Remove and dispose of all entries in the cache.
   */
  void clear();

  /**
   * Get a map copy of all entries in the cache, including expired entries.
   */
  std::unordered_map<K, V> get_entries();

  /**
   * Get the current size of the cache, including expired entries.
   */
  int size();

  /**
   * Set the cleanup interval for the cache. At cleanup time, expired entries marked for cleanup via
   * ShouldDisposeFunc (if defined) are disposed.
   */
  void set_clean_up_interval_nanos(long long clean_up_interval_nanos);

 protected:
  std::unordered_map<K, CACHE_ITEM*> cache;
  long long clean_up_interval_nanos = 6000000000;  // 1 minutes
  std::atomic<std::chrono::steady_clock::time_point> clean_up_time_nanos;
  SHOULD_DISPOSE_FUNC<V>* should_dispose_func;
  ITEM_DISPOSAL_FUNC<V>* item_disposal_func;

  void remove_and_dispose(K key);
  void remove_if_expired(K key) {
    std::vector<V> items;
    if (this->cache.count(key)) {
      CACHE_ITEM* cache_item = this->cache[key];
      if (cache_item != nullptr && cache_item->should_clean_up(this->should_dispose_func)) {
        if (item_disposal_func != nullptr) {
          item_disposal_func->dispose(items[0]);
        }
        this->cache.erase(key);
      }
    }
  }
  void clean_up();
};

#endif
