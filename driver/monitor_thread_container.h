/*
 * AWS ODBC Driver for MySQL
 * Copyright Amazon.com Inc. or affiliates.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation and/or
 * other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef __MONITORTHREADCONTAINER_H__
#define __MONITORTHREADCONTAINER_H__

#include "monitor.h"

#include <ctpl_stl.h>
#include <future>
#include <map>
#include <queue>

class MONITOR_THREAD_CONTAINER {
public:
    std::string get_node(std::set<std::string> node_keys);
    std::shared_ptr<MONITOR> get_monitor(std::string node);
    std::shared_ptr<MONITOR> get_or_create_monitor(
        std::set<std::string> node_keys,
        std::shared_ptr<HOST_INFO> host,
        std::chrono::milliseconds disposal_time,
        MONITOR_SERVICE* monitor_service);
    void add_task(std::shared_ptr<MONITOR> monitor);
    void reset_resource(std::shared_ptr<MONITOR> monitor);
    void release_resource(std::shared_ptr<MONITOR> monitor);

protected:
    void populate_monitor_map(std::set<std::string> node_keys, std::shared_ptr<MONITOR> monitor);
    void remove_monitor_mapping(std::shared_ptr<MONITOR> monitor);
    std::shared_ptr<MONITOR> get_available_monitor();
    virtual std::shared_ptr<MONITOR> create_monitor(
        std::shared_ptr<HOST_INFO> host,
        std::chrono::milliseconds disposal_time,
        MONITOR_SERVICE* monitor_service);

    std::map<std::string, std::shared_ptr<MONITOR>> monitor_map;
    std::map<std::shared_ptr<MONITOR>, std::future<void>*> task_map;
    std::queue<std::shared_ptr<MONITOR>> available_monitors;
    ctpl::thread_pool thread_pool;
};

#endif /* __MONITORTHREADCONTAINER_H__ */
