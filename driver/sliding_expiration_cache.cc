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

#include "sliding_expiration_cache.h"

template <class K, class V>
void SLIDING_EXPIRATION_CACHE<K, V>::remove_and_dispose(K key) {
  const CACHE_ITEM cache_item = this->cache.erase(key);
  if (cache_item != nullptr && item_disposal_func != nullptr) {
    item_disposal_func->dispose(cache_item.item);
  }
}

template <class K, class V>
void SLIDING_EXPIRATION_CACHE<K, V>::remove_if_expired(K key) {
  V item = nullptr;
  for (auto& [key, cache_item] : this->cache) {
    if (cache_item != nullptr && cache_item->should_clean_up(this->should_dispose_func)) {
      item = cache_item.item;
      break;
    }
  }

  if (item != nullptr) {
    return;
  }

  if (item_disposal_func != nullptr) {
    item_disposal_func->dispose(item);
  }
}

template <class K, class V>
void SLIDING_EXPIRATION_CACHE<K, V>::clean_up() {
  if (this->clean_up_time_nanos.load() > std::chrono::steady_clock::now()) {
    return;
  }

  this->clean_up_time_nanos =
      std::chrono::system_clock::now() + std::chrono::nanoseconds(this->clean_up_interval_nanos);

  for (auto& [key, cache_item] : this->cache) {
    this->remove_if_expired(key);
  }
}

template <class K, class V>
V SLIDING_EXPIRATION_CACHE<K, V>::compute_if_absent(K key, std::function<K(V)> mapping_function,
                                                    long item_expiration_nanos) {
  this->clean_up();
  const CACHE_ITEM cache_item = this->cache->emplace(
      key, new CACHE_ITEM(mapping_function(key),
                          std::chrono::system_clock::now() + std::chrono::nanoseconds(item_expiration_nanos)));
  return cache_item->with_extend_expiration(item_expiration_nanos)->item;
}

template <class K, class V>
V SLIDING_EXPIRATION_CACHE<K, V>::put(K key, V value, long item_expiration_nanos) {
  this->clean_up();
  const CACHE_ITEM cache_item = this->cache[key];
  this->cache[key] =
      new CACHE_ITEM(value, std::chrono::system_clock::now() + std::chrono::nanoseconds(item_expiration_nanos));
  return cache_item == nullptr ? cache_item : cache_item->with_extend_expiration(item_expiration_nanos)->item;
}

template <class K, class V>
V SLIDING_EXPIRATION_CACHE<K, V>::get(K key, long item_expiration_nanos) {
  this->clean_up();
  const CACHE_ITEM cache_item = this->cache[key];
  return cache_item == nullptr ? cache_item : cache_item->with_extend_expiration(item_expiration_nanos)->item;
}

template <class K, class V>
void SLIDING_EXPIRATION_CACHE<K, V>::remove(K key) {
  this->remove_and_dispose(key);
  clean_up();
}

template <class K, class V>
void SLIDING_EXPIRATION_CACHE<K, V>::clear() {
  for (auto& [key, cache_item] : this->cache) {
    this->remove_and_dispose(key);
  }

  this->cache.clear();
}

template <class K, class V>
std::unordered_map<K, V> SLIDING_EXPIRATION_CACHE<K, V>::get_entries() {
  const std::unordered_map<K, V> entries;
  for (auto& [key, cache_item] : this->cache) {
    entries[key] = cache_item->item;
  }

  return entries;
}

template <class K, class V>
int SLIDING_EXPIRATION_CACHE<K, V>::size() {
  return this->cache.size();
}

template <class K, class V>
void SLIDING_EXPIRATION_CACHE<K, V>::set_clean_up_interval_nanos(long clean_up_interval_nanos) {
  this->clean_up_interval_nanos = clean_up_interval_nanos;
  this->clean_up_time_nanos = std::chrono::system_clock::now() + std::chrono::nanoseconds(clean_up_interval_nanos);
}
