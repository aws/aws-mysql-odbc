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

#ifndef __SLIDING_EXPIRATION_CACHE_WITH_CLEAN_UP_THREAD__
#define __SLIDING_EXPIRATION_CACHE_WITH_CLEAN_UP_THREAD__

#include <ctpl_stl.h>
#include <mutex>

#include "sliding_expiration_cache.h"

template <class K, class V>
class SLIDING_EXPIRATION_CACHE_WITH_CLEAN_UP_THREAD : public SLIDING_EXPIRATION_CACHE<K, V> {
 public:
  SLIDING_EXPIRATION_CACHE_WITH_CLEAN_UP_THREAD() = default;
  SLIDING_EXPIRATION_CACHE_WITH_CLEAN_UP_THREAD(std::shared_ptr<SHOULD_DISPOSE_FUNC<V>> should_dispose_func,
                                                std::shared_ptr<ITEM_DISPOSAL_FUNC<V>> item_disposal_func)
      : SLIDING_EXPIRATION_CACHE<K, V>(std::move(should_dispose_func), std::move(item_disposal_func)){};
  SLIDING_EXPIRATION_CACHE_WITH_CLEAN_UP_THREAD(std::shared_ptr<SHOULD_DISPOSE_FUNC<V>> should_dispose_func,
                                                std::shared_ptr<ITEM_DISPOSAL_FUNC<V>> item_disposal_func,
                                                long long clean_up_interval_nanos)
      : SLIDING_EXPIRATION_CACHE<K, V>(std::move(should_dispose_func), std::move(item_disposal_func),
                                 clean_up_interval_nanos){};
  ~SLIDING_EXPIRATION_CACHE_WITH_CLEAN_UP_THREAD() = default;

  /**
   * Stop clean up thread. Should be called at the end of the cache's lifetime.
   */
  void release_resources();
  bool empty();

#ifdef UNIT_TEST_BUILD
  SLIDING_EXPIRATION_CACHE_WITH_CLEAN_UP_THREAD(long long clean_up_interval_nanos) {
    this->clean_up_interval_nanos = clean_up_interval_nanos;
  };
#endif
  void init_clean_up_thread();

 protected:
  bool is_initialized = false;
  bool should_stop = false;
  std::mutex mutex_;
  ctpl::thread_pool clean_up_thread_pool;
};

#endif
