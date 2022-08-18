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
#include "mylog.h"
#include "mysql_proxy.h"

MONITOR::MONITOR(
    std::shared_ptr<HOST_INFO> host_info,
    std::chrono::milliseconds monitor_disposal_time,
    DataSource* ds,
    bool enable_logging)
    : MONITOR(
        std::move(host_info),
        monitor_disposal_time,
        new MYSQL_MONITOR_PROXY(ds),
        enable_logging) {};

MONITOR::MONITOR(
    std::shared_ptr<HOST_INFO> host_info,
    std::chrono::milliseconds monitor_disposal_time,
    MYSQL_MONITOR_PROXY* proxy,
    bool enable_logging) {

    this->host = std::move(host_info);
    this->disposal_time = monitor_disposal_time;
    this->mysql_proxy = proxy;
    this->connection_check_interval = (std::chrono::milliseconds::max)();
    if (enable_logging)
        this->logger = init_log_file();
}

MONITOR::~MONITOR() {
    if (this->mysql_proxy) {
        delete this->mysql_proxy;
        this->mysql_proxy = nullptr;
    }
}

void MONITOR::start_monitoring(std::shared_ptr<MONITOR_CONNECTION_CONTEXT> context) {
    std::chrono::milliseconds detection_interval = context->get_failure_detection_interval();
    if (detection_interval < this->connection_check_interval) {
        this->connection_check_interval = detection_interval;
    }

    auto current_time = get_current_time();
    context->set_start_monitor_time(current_time);
    this->last_context_timestamp = current_time;
    
    {
        std::unique_lock<std::mutex> lock(mutex_);
        this->contexts.push_back(context);
    }
}

void MONITOR::stop_monitoring(std::shared_ptr<MONITOR_CONNECTION_CONTEXT> context) {
    if (context == nullptr) {
        MYLOG_TRACE(
            this->logger.get(), 0,
            "[MONITOR_SERVICE] Invalid context passed into stop_monitoring()");
        return;
    }

    context->invalidate();

    {
        std::unique_lock<std::mutex> lock(mutex_);
        this->contexts.remove(context);
    }

    this->connection_check_interval = this->find_shortest_interval();
}

bool MONITOR::is_stopped() {
    return this->stopped.load();
}

void MONITOR::clear_contexts() {
    {
        std::unique_lock<std::mutex> lock(mutex_);
        this->contexts.clear();
    }

    this->connection_check_interval = (std::chrono::milliseconds::max)();
}

// Periodically ping the server and update the contexts' connection status.
void MONITOR::run(std::shared_ptr<MONITOR_SERVICE> service) {
    this->stopped = false;
    while (true) {
        bool have_contexts;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            have_contexts = !this->contexts.empty();
        }
        if (have_contexts) {
            auto status_check_start_time = this->get_current_time();
            this->last_context_timestamp = status_check_start_time;

            std::chrono::milliseconds check_interval = this->get_connection_check_interval();
            CONNECTION_STATUS status = this->check_connection_status(check_interval);

            {
                std::unique_lock<std::mutex> lock(mutex_);
                for (auto it = this->contexts.begin(); it != this->contexts.end(); ++it) {
                    std::shared_ptr<MONITOR_CONNECTION_CONTEXT> context = *it;
                    context->update_connection_status(
                        status_check_start_time,
                        status_check_start_time + status.elapsed_time,
                        status.is_valid);
                }
            }

            auto sleep_time = check_interval - status.elapsed_time;
            if (sleep_time > std::chrono::milliseconds(0)) {
                std::this_thread::sleep_for(sleep_time);
            }
        }
        else {
            auto current_time = this->get_current_time();
            if ((current_time - this->last_context_timestamp) >= this->disposal_time) {
                service->notify_unused(shared_from_this());
                break;
            }
            std::this_thread::sleep_for(thread_sleep_when_inactive);
        }
    }

    this->stopped = true;
}

std::chrono::milliseconds MONITOR::get_connection_check_interval() {
    std::unique_lock<std::mutex> lock(mutex_);
    if (this->contexts.empty()) {
        return std::chrono::milliseconds(0);
    }

    return this->connection_check_interval;
}

CONNECTION_STATUS MONITOR::check_connection_status(std::chrono::milliseconds shortest_detection_interval) {
    if (this->mysql_proxy == nullptr || !this->mysql_proxy->is_connected()) {
        const auto start = this->get_current_time();
        bool connected = this->connect(shortest_detection_interval);
        return CONNECTION_STATUS{
            connected,
            std::chrono::duration_cast<std::chrono::milliseconds>(this->get_current_time() - start)
        };
    }

    unsigned int timeout_sec = std::chrono::duration_cast<std::chrono::seconds>(shortest_detection_interval).count();
    this->mysql_proxy->options(MYSQL_OPT_CONNECT_TIMEOUT, &timeout_sec);
    this->mysql_proxy->options(MYSQL_OPT_READ_TIMEOUT, &timeout_sec);

    auto start = this->get_current_time();
    bool is_connection_active = this->mysql_proxy->ping() == 0;
    auto duration = this->get_current_time() - start;
    
    return CONNECTION_STATUS{
        is_connection_active,
        std::chrono::duration_cast<std::chrono::milliseconds>(duration)
    };
}

bool MONITOR::connect(std::chrono::milliseconds timeout) {
    this->mysql_proxy->init();

    unsigned int timeout_sec = std::chrono::duration_cast<std::chrono::seconds>(timeout).count();
    this->mysql_proxy->options(MYSQL_OPT_CONNECT_TIMEOUT, &timeout_sec);
    this->mysql_proxy->options(MYSQL_OPT_READ_TIMEOUT, &timeout_sec);

    if (!this->mysql_proxy->connect()) {
        MYLOG_TRACE(this->logger.get(), 0, this->mysql_proxy->error());
        return false;
    }

    return true;
}

std::chrono::milliseconds MONITOR::find_shortest_interval() {
    auto min = (std::chrono::milliseconds::max)();

    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (this->contexts.empty()) {
            return min;
        }

        for (auto it = this->contexts.begin(); it != this->contexts.end(); ++it) {
            auto failure_detection_interval = (*it)->get_failure_detection_interval();
            if (failure_detection_interval < min) {
                min = failure_detection_interval;
            }
        }
    }

    return min;
}

std::chrono::steady_clock::time_point MONITOR::get_current_time() {
    return std::chrono::steady_clock::now();
}
