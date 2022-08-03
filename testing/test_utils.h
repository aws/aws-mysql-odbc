/*
 * AWS ODBC Driver for MySQL
 * Copyright Amazon.com Inc. or affiliates.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef __TESTUTILS_H__
#define __TESTUTILS_H__

#include "driver/driver.h"

void allocate_odbc_handles(SQLHENV& env, DBC*& dbc, DataSource*& ds);
void cleanup_odbc_handles(SQLHENV& env, DBC*& dbc, DataSource*& ds, bool call_myodbc_end = false);

class TEST_UTILS {
public:
    static std::chrono::milliseconds get_connection_check_interval(std::shared_ptr<MONITOR> monitor);
    static CONNECTION_STATUS check_connection_status(
        std::shared_ptr<MONITOR> monitor, std::chrono::milliseconds shortest_detection_interval);
    static void populate_monitor_map(std::shared_ptr<MONITOR_THREAD_CONTAINER> container,
        std::set<std::string> node_keys, std::shared_ptr<MONITOR> monitor);
    static void populate_task_map(std::shared_ptr<MONITOR_THREAD_CONTAINER> container,
        std::shared_ptr<MONITOR> monitor);
    static bool has_monitor(std::shared_ptr<MONITOR_THREAD_CONTAINER> container, std::string node_key);
    static bool has_task(std::shared_ptr<MONITOR_THREAD_CONTAINER> container, std::shared_ptr<MONITOR> monitor);
    static bool has_any_tasks(std::shared_ptr<MONITOR_THREAD_CONTAINER> container);
    static bool has_available_monitor(std::shared_ptr<MONITOR_THREAD_CONTAINER> container);
    static std::shared_ptr<MONITOR> get_available_monitor(std::shared_ptr<MONITOR_THREAD_CONTAINER> container);
    static size_t get_map_size(std::shared_ptr<MONITOR_THREAD_CONTAINER> container);
    static std::list<std::shared_ptr<MONITOR_CONNECTION_CONTEXT>> get_contexts(std::shared_ptr<MONITOR> monitor);
};

#endif /* __TESTUTILS_H__ */
