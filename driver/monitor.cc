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

MONITOR::MONITOR(std::shared_ptr<HOST_INFO> host_info, long monitor_disposal_time) {
    this->host = host_info;
    this->disposal_time = monitor_disposal_time;
}

void MONITOR::start_monitoring(std::shared_ptr<MONITOR_CONNECTION_CONTEXT> context) {
    // TODO: Implement
}

void MONITOR::stop_monitoring(std::shared_ptr<MONITOR_CONNECTION_CONTEXT> context) {
    // TODO: Implement
}

bool MONITOR::is_stopped() {
    // TODO: Implement
    return false;
}

void MONITOR::clear_contexts() {
    // TODO: Implement
}

CONNECTION_STATUS MONITOR::check_connection_status(int shortest_detection_interval) {
    // TODO: Implement
    return CONNECTION_STATUS{ false, 0 };
}
