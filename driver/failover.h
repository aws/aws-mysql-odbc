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

#ifndef __FAILOVER_H__
#define __FAILOVER_H__

#include "connection_handler.h"
#include "connection_proxy.h"
#include "topology_service.h"
#include "mylog.h"

#include <condition_variable>

struct READER_FAILOVER_RESULT {
    bool connected = false;
    std::shared_ptr<HOST_INFO> new_host;
    CONNECTION_PROXY* new_connection;

    READER_FAILOVER_RESULT()
        : connected{false}, new_host{nullptr}, new_connection{nullptr} {}
        
    READER_FAILOVER_RESULT(bool connected, std::shared_ptr<HOST_INFO> new_host,
                           CONNECTION_PROXY* new_connection)
        : connected{connected},
          new_host{new_host},
          new_connection{new_connection} {}
};

// FAILOVER_SYNC enables synchronization between threads
class FAILOVER_SYNC {
   public:
    FAILOVER_SYNC(int num_tasks);
    void increment_task();
    void mark_as_complete(bool cancel_other_tasks);
    void wait_and_complete(int milliseconds);
    virtual bool is_completed();

   private:
    int num_tasks;
    std::mutex mutex_;
    std::condition_variable cv;
};

class FAILOVER_READER_HANDLER {
   public:
    FAILOVER_READER_HANDLER(
        std::shared_ptr<TOPOLOGY_SERVICE> topology_service,
        std::shared_ptr<CONNECTION_HANDLER> connection_handler,
        ctpl::thread_pool& thread_pool,
        int failover_timeout_ms, int failover_reader_connect_timeout,
        bool enable_strict_reader_failover,
        unsigned long dbc_id, bool enable_logging = false);
    
        ~FAILOVER_READER_HANDLER();
    
        std::shared_ptr<READER_FAILOVER_RESULT> failover(
            std::shared_ptr<CLUSTER_TOPOLOGY_INFO> topology_info);
    
        virtual std::shared_ptr<READER_FAILOVER_RESULT> get_reader_connection(
            std::shared_ptr<CLUSTER_TOPOLOGY_INFO> topology_info,
            std::shared_ptr<FAILOVER_SYNC> f_sync);

        std::vector<std::shared_ptr<HOST_INFO>> build_hosts_list(
            std::shared_ptr<CLUSTER_TOPOLOGY_INFO> topology_info,
            bool contain_writers);

        std::shared_ptr<READER_FAILOVER_RESULT> get_connection_from_hosts(
            std::vector<std::shared_ptr<HOST_INFO>> hosts_list,
            std::shared_ptr<FAILOVER_SYNC> global_sync);

    protected:
        int reader_connect_timeout_ms = 30000;   // 30 sec
        int max_failover_timeout_ms = 60000;  // 60 sec

    private:
        std::shared_ptr<TOPOLOGY_SERVICE> topology_service;
        std::shared_ptr<CONNECTION_HANDLER> connection_handler; 
        const int READER_CONNECT_INTERVAL_SEC = 1;  // 1 sec
        bool enable_strict_reader_failover = false;
        std::shared_ptr<FILE> logger = nullptr;
        unsigned long dbc_id = 0;
        ctpl::thread_pool& thread_pool;
};

// This struct holds results of Writer Failover Process.
struct WRITER_FAILOVER_RESULT {
    bool connected = false;
    bool is_new_host = false;  // True if process connected to a new host. False if
                               // process re-connected to the same host
    std::shared_ptr<CLUSTER_TOPOLOGY_INFO> new_topology;
    CONNECTION_PROXY* new_connection;

    WRITER_FAILOVER_RESULT()
        : connected{false},
          is_new_host{false},
          new_topology{nullptr},
          new_connection{nullptr} {}

    WRITER_FAILOVER_RESULT(bool connected, bool is_new_host,
                           std::shared_ptr<CLUSTER_TOPOLOGY_INFO> new_topology,
                           CONNECTION_PROXY* new_connection)
        : connected{connected},
          is_new_host{is_new_host},
          new_topology{new_topology},
          new_connection{new_connection} {}
};

class FAILOVER_WRITER_HANDLER {
   public:
    FAILOVER_WRITER_HANDLER(
        std::shared_ptr<TOPOLOGY_SERVICE> topology_service,
        std::shared_ptr<FAILOVER_READER_HANDLER> reader_handler,
        std::shared_ptr<CONNECTION_HANDLER> connection_handler,
        ctpl::thread_pool& thread_pool,
        int writer_failover_timeout_ms, int read_topology_interval_ms,
        int reconnect_writer_interval_ms, unsigned long dbc_id, bool enable_logging = false);
    ~FAILOVER_WRITER_HANDLER();
    std::shared_ptr<WRITER_FAILOVER_RESULT> failover(
        std::shared_ptr<CLUSTER_TOPOLOGY_INFO> current_topology);

   protected:
    int read_topology_interval_ms = 5000;     // 5 sec
    int reconnect_writer_interval_ms = 5000;  // 5 sec
    int writer_failover_timeout_ms = 60000;   // 60 sec

   private:
    std::shared_ptr<TOPOLOGY_SERVICE> topology_service;
    std::shared_ptr<CONNECTION_HANDLER> connection_handler;
    std::shared_ptr<FAILOVER_READER_HANDLER> reader_handler;
    std::shared_ptr<FILE> logger = nullptr;
    unsigned long dbc_id = 0;
    ctpl::thread_pool& thread_pool;
};

class FAILOVER_HANDLER {
   public:
    FAILOVER_HANDLER(DBC* dbc, DataSource* ds);
    FAILOVER_HANDLER(
        DBC* dbc, DataSource* ds,
        std::shared_ptr<CONNECTION_HANDLER> connection_handler,
        std::shared_ptr<TOPOLOGY_SERVICE> topology_service,
        std::shared_ptr<CLUSTER_AWARE_METRICS_CONTAINER> metrics_container);
    ~FAILOVER_HANDLER();
    SQLRETURN init_connection();
    bool trigger_failover_if_needed(const char* error_code, const char*& new_error_code, const char*& error_msg);
    bool is_failover_enabled();
    bool is_rds();
    bool is_rds_proxy();
    bool is_cluster_topology_available();
    void invoke_start_time();
    std::string cluster_id = DEFAULT_CLUSTER_ID;

   private:
    DBC* dbc = nullptr;
    DataSource* ds = nullptr;
    std::shared_ptr<TOPOLOGY_SERVICE> topology_service;
    std::shared_ptr<FAILOVER_READER_HANDLER> failover_reader_handler;
    std::shared_ptr<FAILOVER_WRITER_HANDLER> failover_writer_handler;
    std::shared_ptr<CLUSTER_TOPOLOGY_INFO> current_topology;
    std::shared_ptr<HOST_INFO> current_host = nullptr;
    std::shared_ptr<CONNECTION_HANDLER> connection_handler = nullptr;
    bool m_is_cluster_topology_available = false;
    bool m_is_multi_writer_cluster = false;
    bool m_is_rds_proxy = false;
    bool m_is_rds = false;
    bool m_is_rds_custom_cluster = false;
    bool is_cluster_info_initialized = false;

    void init_cluster_info();
    bool should_connect_to_new_writer();
    void initialize_topology();
    bool is_read_only();
    virtual std::string host_to_IP(std::string host);
    SQLRETURN reconnect(bool failover_enabled);
    static bool is_failover_mode(const char* expected_mode, DataSource* ds);
    bool failover_to_reader(const char*& new_error_code, const char*& error_msg);
    bool failover_to_writer(const char*& new_error_code, const char*& error_msg);

    void set_cluster_id(std::string host, int port);
    void set_cluster_id(std::string cluster_id);
    std::shared_ptr<CLUSTER_AWARE_METRICS_CONTAINER> metrics_container;
    std::chrono::steady_clock::time_point invoke_start_time_ms;
    std::chrono::steady_clock::time_point failover_start_time_ms;

#ifdef UNIT_TEST_BUILD
    // Allows for testing private/protected methods
    friend class TEST_UTILS;
#endif
};

// ************************************************************************************************
// These are failover utilities/helpers. Perhaps belong to a separate header
// file, but here for now
//

class FAILOVER {
   public:
    FAILOVER(std::shared_ptr<CONNECTION_HANDLER> connection_handler,
             std::shared_ptr<TOPOLOGY_SERVICE> topology_service,
             unsigned long dbc_id, bool enable_logging = false);
    virtual ~FAILOVER() = default;
    bool is_writer_connected();

   protected:
    bool connect(std::shared_ptr<HOST_INFO> host_info);
    void sleep(int miliseconds);
    void release_new_connection();
    std::shared_ptr<CONNECTION_HANDLER> connection_handler;
    std::shared_ptr<TOPOLOGY_SERVICE> topology_service;
    CONNECTION_PROXY* new_connection;
    std::shared_ptr<FILE> logger = nullptr;
    unsigned long dbc_id = 0;
};

class CONNECT_TO_READER_HANDLER : public FAILOVER {
public:
    CONNECT_TO_READER_HANDLER(
        std::shared_ptr<CONNECTION_HANDLER> connection_handler,
     std::shared_ptr<TOPOLOGY_SERVICE> topology_service,
     unsigned long dbc_id, bool enable_logging = false);
    ~CONNECT_TO_READER_HANDLER();

    void operator()(
        int id,
        std::shared_ptr<HOST_INFO> reader,
        std::shared_ptr<FAILOVER_SYNC> f_sync,
        std::shared_ptr<READER_FAILOVER_RESULT> result);
};

class RECONNECT_TO_WRITER_HANDLER : public FAILOVER {
   public:
    RECONNECT_TO_WRITER_HANDLER(
        std::shared_ptr<CONNECTION_HANDLER> connection_handler,
        std::shared_ptr<TOPOLOGY_SERVICE> topology_service,
        int connection_interval, unsigned long dbc_id, bool enable_logging = false);
    ~RECONNECT_TO_WRITER_HANDLER();

    void operator()(
        int id,
        std::shared_ptr<HOST_INFO> original_writer,
        std::shared_ptr<FAILOVER_SYNC> f_sync,
        std::shared_ptr<WRITER_FAILOVER_RESULT> result);

   private:
    int reconnect_interval_ms;

    bool is_current_host_writer(
        std::shared_ptr<HOST_INFO> original_writer,
        std::shared_ptr<CLUSTER_TOPOLOGY_INFO> latest_topology);
};

class WAIT_NEW_WRITER_HANDLER : public FAILOVER {
   public:
    WAIT_NEW_WRITER_HANDLER(
        std::shared_ptr<CONNECTION_HANDLER> connection_handler,
        std::shared_ptr<TOPOLOGY_SERVICE> topology_service,
        std::shared_ptr<CLUSTER_TOPOLOGY_INFO> current_topology,
        std::shared_ptr<FAILOVER_READER_HANDLER> reader_handler,
        int connection_interval, unsigned long dbc_id, bool enable_logging = false);
    ~WAIT_NEW_WRITER_HANDLER();

    void operator()(
        int id,
        std::shared_ptr<HOST_INFO> original_writer,
        std::shared_ptr<FAILOVER_SYNC> f_sync,
        std::shared_ptr<WRITER_FAILOVER_RESULT> result);

   private:
    // TODO - initialize in constructor and define constant for default value
    int read_topology_interval_ms = 5000;
    std::shared_ptr<FAILOVER_READER_HANDLER> reader_handler;
    std::shared_ptr<CLUSTER_TOPOLOGY_INFO> current_topology;
    CONNECTION_PROXY* reader_connection = nullptr;  // To retrieve latest topology
    std::shared_ptr<HOST_INFO> current_reader_host = nullptr;

    void refresh_topology_and_connect_to_new_writer(
        std::shared_ptr<HOST_INFO> original_writer, std::shared_ptr<FAILOVER_SYNC> f_sync);
    void connect_to_reader(std::shared_ptr<FAILOVER_SYNC> f_sync);
    bool connect_to_writer(std::shared_ptr<HOST_INFO> writer_candidate);
    void clean_up_reader_connection();
};

#endif /* __FAILOVER_H__ */
