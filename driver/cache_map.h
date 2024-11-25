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

#ifndef __CACHE_MAP__
#define __CACHE_MAP__

#include <chrono>
#include <memory>
#include <unordered_map>

template <class K, class V>
class CACHE_MAP {
 public:
  class CACHE_ITEM {
   public:
    CACHE_ITEM() = default;
    CACHE_ITEM(V item, std::chrono::steady_clock::time_point expiration_time)
        : item(item), expiration_time(expiration_time){};
    ~CACHE_ITEM() = default;
    V item;

    bool is_expired() { return std::chrono::steady_clock::now() > this->expiration_time; }

   private:
    std::chrono::steady_clock::time_point expiration_time;
  };

  CACHE_MAP() = default;
  ~CACHE_MAP() = default;

  void put(K key, V value, long long item_expiration_nanos);
  V get(K key, V default_value);
  V get(K key, V default_value, long long item_expiration_nanos);
  void remove(K key);
  int size();
  void clear();

 protected:
  void clean_up();
  const long long clean_up_time_interval_nanos = 60000000000;  // 10 minute
  std::atomic<std::chrono::steady_clock::time_point> clean_up_time_nanos;

 private:
  std::unordered_map<K, std::shared_ptr<CACHE_ITEM>> cache;
};

#endif
