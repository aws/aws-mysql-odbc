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

#include "monitor.h"
#include "monitor_service.h"

#define THREAD_SLEEP_WHEN_INACTIVE std::chrono::milliseconds(100)

MONITOR::MONITOR(
    std::shared_ptr<HOST_INFO> host_info,
    std::chrono::milliseconds monitor_disposal_time,
    MONITOR_SERVICE* service) {
    
    this->host = host_info;
    this->disposal_time = monitor_disposal_time;
    this->monitor_service = service;
    this->connection_check_interval = std::chrono::milliseconds::max();
}

void MONITOR::start_monitoring(std::shared_ptr<MONITOR_CONNECTION_CONTEXT> context) {
    std::chrono::milliseconds detection_interval = context->get_failure_detection_interval();
    if (detection_interval < this->connection_check_interval) {
        this->connection_check_interval = detection_interval;
    }

    auto current_time = std::chrono::steady_clock::now();
    context->set_start_monitor_time(current_time);
    this->last_context_timestamp = current_time;
    this->contexts.push_back(context);
}

void MONITOR::stop_monitoring(std::shared_ptr<MONITOR_CONNECTION_CONTEXT> context) {
    if (context == nullptr) {
        // TODO: Log Warning
        return;
    }

    this->contexts.remove(context);
    context->invalidate();

    this->connection_check_interval = this->find_shortest_interval();
}

bool MONITOR::is_stopped() {
    return this->stopped;
}

void MONITOR::clear_contexts() {
    this->contexts.clear();
    this->connection_check_interval = std::chrono::milliseconds::max();
}

// Periodically ping the server and update the contexts' connection status.
void MONITOR::run() {
    try {
        this->stopped = false;
        while (true) {
            if (!this->contexts.empty()) {
                auto status_check_start_time = std::chrono::steady_clock::now();
                this->last_context_timestamp = status_check_start_time;

                CONNECTION_STATUS status = check_connection_status(this->connection_check_interval);

                for (auto it = this->contexts.begin(); it != this->contexts.end(); it++) {
                    std::shared_ptr<MONITOR_CONNECTION_CONTEXT> context = *it;
                    context->update_connection_status(
                        status_check_start_time,
                        status_check_start_time + status.elapsed_time,
                        status.is_valid);
                }

                auto sleep_time = this->connection_check_interval - status.elapsed_time;
                if (sleep_time > std::chrono::milliseconds(0)) {
                    std::this_thread::sleep_for(sleep_time);
                }
            }
            else {
                auto current_time = std::chrono::steady_clock::now();
                if ((current_time - this->last_context_timestamp) >= this->disposal_time) {
                    this->monitor_service->notify_unused(std::shared_ptr<MONITOR>(this));
                    break;
                }
                std::this_thread::sleep_for(THREAD_SLEEP_WHEN_INACTIVE);
            }
        }
    }
    catch (...) {
        // TODO: close connection if needed
        this->stopped = true;
    }
}

CONNECTION_STATUS MONITOR::check_connection_status(std::chrono::milliseconds shortest_detection_interval) {
    // TODO: Implement
    return CONNECTION_STATUS{ false, std::chrono::milliseconds(0) };
}

std::chrono::milliseconds MONITOR::find_shortest_interval() {
    auto min = std::chrono::milliseconds::max();
    if (this->contexts.empty()) {
        return min;
    }

    for (auto it = this->contexts.begin(); it != this->contexts.end(); it++) {
        auto failure_detection_interval = (*it)->get_failure_detection_interval();
        if (failure_detection_interval < min) {
            min = failure_detection_interval;
        }
    }

    return min;
}
