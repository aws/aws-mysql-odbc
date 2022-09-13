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

#ifndef __MONITORSERVICE_H__
#define __MONITORSERVICE_H__

#include "monitor_thread_container.h"

class MONITOR_SERVICE : public std::enable_shared_from_this<MONITOR_SERVICE> {
public:
    MONITOR_SERVICE(bool enable_logging = false);
    MONITOR_SERVICE(
        std::shared_ptr<MONITOR_THREAD_CONTAINER> monitor_thread_container,
        bool enable_logging = false);
    virtual ~MONITOR_SERVICE() = default;
    
    virtual std::shared_ptr<MONITOR_CONNECTION_CONTEXT> start_monitoring(
        DBC* dbc,
        DataSource* ds,
        std::set<std::string> node_keys,
        std::shared_ptr<HOST_INFO> host,
        std::chrono::milliseconds failure_detection_time,
        std::chrono::milliseconds failure_detection_interval,
        int failure_detection_count,
        std::chrono::milliseconds disposal_time);
    virtual void stop_monitoring(std::shared_ptr<MONITOR_CONNECTION_CONTEXT> context);
    virtual void stop_monitoring_for_all_connections(std::set<std::string> node_keys);
    void notify_unused(std::shared_ptr<MONITOR> monitor);
    void release_resources();

private:
    std::shared_ptr<MONITOR_THREAD_CONTAINER> thread_container;
    std::shared_ptr<FILE> logger;
};

#endif /* __MONITORSERVICE_H__ */
