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
    
    MYLOG_TRACE(init_log_file().get(), 0, "[MONITOR] enable_logging = %d", enable_logging);
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
        MYLOG_TRACE(this->logger.get(), 0, "[MONITOR] %p pushed context; num_contexts = %d", this, this->contexts.size());
    }
}

void MONITOR::stop_monitoring(std::shared_ptr<MONITOR_CONNECTION_CONTEXT> context) {
    if (context == nullptr) {
        MYLOG_TRACE(
            this->logger.get(), 0,
            "[MONITOR] Invalid context passed into stop_monitoring()");
        return;
    }

    context->invalidate();

    {
        std::unique_lock<std::mutex> lock(mutex_);
        this->contexts.remove(context);
        MYLOG_TRACE(this->logger.get(), 0, "[MONITOR] %p removed context; num_contexts = %d", this, this->contexts.size());
    }

    this->connection_check_interval = this->find_shortest_interval();
}

bool MONITOR::is_stopped() {
    return this->stopped.load(); 
}

void MONITOR::stop() {
    this->stopped.store(true);
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
    MYLOG_TRACE(this->logger.get(), 0, "[MONITOR] Running monitor at address %p using ms with address %p", this, service.get());
    MYLOG_TRACE(this->logger.get(), 0, "[MONITOR] ms_use_count = %d", service.use_count());
    long long loop_counter = 0;
    this->stopped = false;
    while (!this->stopped) {
        MYLOG_TRACE(this->logger.get(), 0, "[MONITOR] %p loop_counter = %d", this, ++loop_counter);
        bool have_contexts;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            have_contexts = !this->contexts.empty();
        }
        MYLOG_TRACE(this->logger.get(), 0, "[MONITOR] %p have_contexts = %d", this, have_contexts);
        if (have_contexts) {
            MYLOG_TRACE(this->logger.get(), 0, "[MONITOR] %p auto status_check_start_time = this->get_current_time();");
            auto status_check_start_time = this->get_current_time();
            MYLOG_TRACE(this->logger.get(), 0, "[MONITOR] %p this->last_context_timestamp = status_check_start_time;");
            this->last_context_timestamp = status_check_start_time;

            MYLOG_TRACE(this->logger.get(), 0, "[MONITOR] %p std::chrono::milliseconds check_interval = this->get_connection_check_interval();", this);
            std::chrono::milliseconds check_interval = this->get_connection_check_interval();
            MYLOG_TRACE(this->logger.get(), 0, "[MONITOR] %p CONNECTION_STATUS status = this->check_connection_status(check_interval);", this);
            CONNECTION_STATUS status = this->check_connection_status(check_interval);

            {
                MYLOG_TRACE(this->logger.get(), 0, "[MONITOR] %p updating connection status", this);
                std::unique_lock<std::mutex> lock(mutex_);
                for (auto it = this->contexts.begin(); it != this->contexts.end(); ++it) {
                    MYLOG_TRACE(this->logger.get(), 0, "[MONITOR] %p std::shared_ptr<MONITOR_CONNECTION_CONTEXT> context = *it;", this);
                    std::shared_ptr<MONITOR_CONNECTION_CONTEXT> context = *it;
                    MYLOG_TRACE(this->logger.get(), 0, "[MONITOR] %p context->update_connection_status", this);
                    context->update_connection_status(
                        status_check_start_time,
                        status_check_start_time + status.elapsed_time,
                        status.is_valid);
                }
            }

            MYLOG_TRACE(this->logger.get(), 0, "[MONITOR] %p auto sleep_time = check_interval - status.elapsed_time;", this);
            auto sleep_time = check_interval - status.elapsed_time;
            if (sleep_time > std::chrono::milliseconds(0)) {
                MYLOG_TRACE(this->logger.get(), 0, "[MONITOR] %p sleep thread", this);
                std::this_thread::sleep_for(sleep_time);
            }
        }
        else {
            auto time_inactive = std::chrono::duration_cast<std::chrono::milliseconds>(this->get_current_time() - this->last_context_timestamp);
            MYLOG_TRACE(this->logger.get(), 0, "[MONITOR] %p time_inactive = %d, disposal_time = %d", this, time_inactive.count(), disposal_time.count());
            if (time_inactive >= this->disposal_time) {
                MYLOG_TRACE(this->logger.get(), 0, "[MONITOR] %p Breaking out of loop", this);
                break;
            }
            MYLOG_TRACE(this->logger.get(), 0, "[MONITOR] %p sleep thread", this);
            std::this_thread::sleep_for(thread_sleep_when_inactive);
        }
    }

    MYLOG_TRACE(this->logger.get(), 0, "[MONITOR] %p Outside loop", this);

    service->notify_unused(shared_from_this());

    this->stopped = true;
    MYLOG_TRACE(this->logger.get(), 0, "[MONITOR] %p Finished running monitor", this);
}

std::chrono::milliseconds MONITOR::get_connection_check_interval() {
    MYLOG_TRACE(this->logger.get(), 0, "[MONITOR] %p Entering get_connection_check_interval()", this);
    std::unique_lock<std::mutex> lock(mutex_);
    MYLOG_TRACE(this->logger.get(), 0, "[MONITOR] %p if (this->contexts.empty()) {", this);
    if (this->contexts.empty()) {
        MYLOG_TRACE(this->logger.get(), 0, "[MONITOR] %p return std::chrono::milliseconds(0);", this);
        return std::chrono::milliseconds(0);
    }

    MYLOG_TRACE(this->logger.get(), 0, "[MONITOR] %p return this->connection_check_interval;", this);
    return this->connection_check_interval;
}

CONNECTION_STATUS MONITOR::check_connection_status(std::chrono::milliseconds shortest_detection_interval) {
    MYLOG_TRACE(this->logger.get(), 0, "[MONITOR] %p Entering check_connection_status() on %d", this, shortest_detection_interval.count());
    if (this->mysql_proxy == nullptr || !this->mysql_proxy->is_connected() || true) {
        MYLOG_TRACE(this->logger.get(), 0, "[MONITOR] %p Inside if (this->mysql_proxy == nullptr || !this->mysql_proxy->is_connected()) {", this);
        const auto start = this->get_current_time();
        bool connected = this->connect(shortest_detection_interval);
        MYLOG_TRACE(this->logger.get(), 0, "[MONITOR] %p return CONNECTION_STATUS", this);
        return CONNECTION_STATUS{
            connected,
            std::chrono::duration_cast<std::chrono::milliseconds>(this->get_current_time() - start)
        };
    }

    MYLOG_TRACE(this->logger.get(), 0, "[MONITOR] %p unsigned int timeout_sec = std::chrono::duration_cast<std::chrono::seconds>(shortest_detection_interval).count();", this);
    unsigned int timeout_sec = std::chrono::duration_cast<std::chrono::seconds>(shortest_detection_interval).count();
    MYLOG_TRACE(this->logger.get(), 0, "[MONITOR] %p timeout_sec = %d", this, timeout_sec);
    this->mysql_proxy->options(MYSQL_OPT_CONNECT_TIMEOUT, &timeout_sec);
    this->mysql_proxy->options(MYSQL_OPT_READ_TIMEOUT, &timeout_sec);
    this->mysql_proxy->options(MYSQL_OPT_WRITE_TIMEOUT, &timeout_sec);

    unsigned int connect_timeout = -1;
    unsigned int read_timeout = -1;
    unsigned int write_timeout = -1;
    this->mysql_proxy->get_option(MYSQL_OPT_CONNECT_TIMEOUT, &connect_timeout);
    this->mysql_proxy->get_option(MYSQL_OPT_READ_TIMEOUT, &read_timeout);
    this->mysql_proxy->get_option(MYSQL_OPT_WRITE_TIMEOUT, &write_timeout);
    MYLOG_TRACE(this->logger.get(), 0, "[MONITOR] %p connect_timeout = %d", this, connect_timeout);
    MYLOG_TRACE(this->logger.get(), 0, "[MONITOR] %p read_timeout = %d", this, read_timeout);
    MYLOG_TRACE(this->logger.get(), 0, "[MONITOR] %p write_timeout = %d", this, write_timeout);

    MYLOG_TRACE(this->logger.get(), 0, "[MONITOR] %p auto start = this->get_current_time();", this);
    auto start = this->get_current_time();

    MYLOG_TRACE(this->logger.get(), 0, "[MONITOR] %p bool is_connection_active = this->mysql_proxy->ping() == 0;", this);
    bool is_connection_active = this->mysql_proxy->ping() == 0;

    //MYLOG_TRACE(this->logger.get(), 0, "[MONITOR] %p bool is_connection_active = this->mysql_proxy->real_query() == 0;", this);
    //bool is_connection_active = this->mysql_proxy->real_query("SELECT 1", 8) == 0;

    MYLOG_TRACE(this->logger.get(), 0, "[MONITOR] %p auto duration = this->get_current_time() - start;", this);
    auto duration = this->get_current_time() - start;
    
    MYLOG_TRACE(this->logger.get(), 0, "[MONITOR] %p return CONNECTION_STATUS", this);
    return CONNECTION_STATUS{
        is_connection_active,
        std::chrono::duration_cast<std::chrono::milliseconds>(duration)
    };
}

bool MONITOR::connect(std::chrono::milliseconds timeout) {
    this->mysql_proxy->close();
    this->mysql_proxy->init();

    unsigned int timeout_sec = std::chrono::duration_cast<std::chrono::seconds>(timeout).count();
    MYLOG_TRACE(this->logger.get(), 0, "[MONITOR] %p timeout_sec = %d", this, timeout_sec);
    this->mysql_proxy->options(MYSQL_OPT_CONNECT_TIMEOUT, &timeout_sec);
    this->mysql_proxy->options(MYSQL_OPT_READ_TIMEOUT, &timeout_sec);
    this->mysql_proxy->options(MYSQL_OPT_WRITE_TIMEOUT, &timeout_sec);

    if (!this->mysql_proxy->connect()) {
        MYLOG_TRACE(this->logger.get(), 0, "[MONITOR] %p Unsuccessful connection", this);
        MYLOG_TRACE(this->logger.get(), 0, this->mysql_proxy->error());
        this->mysql_proxy->close();
        return false;
    }

    MYLOG_TRACE(this->logger.get(), 0, "[MONITOR] %p successfully connected", this);

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
