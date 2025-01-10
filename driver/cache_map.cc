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

#include "cache_map.h"

#include <utility>

#include "custom_endpoint_info.h"

template <class K, class V>
void CACHE_MAP<K, V>::put(K key, V value, long long item_expiration_nanos) {
  this->cache[key] = std::make_shared<CACHE_ITEM>(
    value, std::chrono::steady_clock::now() + std::chrono::nanoseconds(item_expiration_nanos));
  this->clean_up();
}

template <class K, class V>
V CACHE_MAP<K, V>::get(K key, V default_value) {
  if (cache.count(key) > 0 && !cache[key]->is_expired()) {
    return this->cache[key]->item;
  }
  return default_value;
}

template <class K, class V>
V CACHE_MAP<K, V>::get(K key, V default_value, long long item_expiration_nanos) {
  if (cache.count(key) == 0 || this->cache[key]->is_expired()) {
    this->put(key, std::move(default_value), item_expiration_nanos);
  }
  return this->cache[key]->item;
}

template <class K, class V>
void CACHE_MAP<K, V>::remove(K key) {
  if (this->cache.count(key)) {
    this->cache.erase(key);
  }
  this->clean_up();
}

template <class K, class V>
int CACHE_MAP<K, V>::size() {
  return this->cache.size();
}

template <class K, class V>
void CACHE_MAP<K, V>::clear() {
  this->cache.clear();
}

template <class K, class V>
void CACHE_MAP<K, V>::clean_up() {
  if (std::chrono::steady_clock::now() > this->clean_up_time_nanos.load()) {
    this->clean_up_time_nanos =
      std::chrono::steady_clock::now() + std::chrono::nanoseconds(this->clean_up_time_interval_nanos);
    std::vector<K> keys;
    keys.reserve(this->cache.size());
    for (auto& [key, cache_item] : this->cache) {
      keys.push_back(key);
    }
    for (const auto& key : keys) {
      if (this->cache[key]->is_expired()) {
        this->cache.erase(key);
      }
    }
  }
}

template class CACHE_MAP<std::string, std::shared_ptr<CUSTOM_ENDPOINT_INFO>>;
