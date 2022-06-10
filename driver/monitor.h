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

#ifndef __MONITOR_H__
#define __MONITOR_H__

#include "host_info.h"
#include "monitor_connection_context.h"

#include <queue>

struct CONNECTION_STATUS {
    bool is_valid;
    std::chrono::milliseconds elapsed_time;
};

class MONITOR {
public:
    MONITOR(std::shared_ptr<HOST_INFO> host, std::chrono::milliseconds disposal_time);

    void start_monitoring(std::shared_ptr<MONITOR_CONNECTION_CONTEXT> context);
    void stop_monitoring(std::shared_ptr<MONITOR_CONNECTION_CONTEXT> context);
    bool is_stopped();
    void clear_contexts();

private:
    std::shared_ptr<HOST_INFO> host;
    std::chrono::milliseconds connection_check_interval = std::chrono::milliseconds(INT_MAX);
    std::chrono::milliseconds disposal_time;
    std::queue<std::shared_ptr<MONITOR_CONNECTION_CONTEXT>> contexts;

    CONNECTION_STATUS check_connection_status(int shortest_detection_interval);
};

#endif /* __MONITOR_H__ */
