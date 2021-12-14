/**
  @file failover.h
  @brief Definitions needed for failover
*/

#ifndef __FAILOVER_H__
#define __FAILOVER_H__

#include "topology_service.h"

#include <functional>
#include <condition_variable>

struct DBC;

class FAILOVER_CONNECTION_HANDLER {
   public:
    FAILOVER_CONNECTION_HANDLER(std::shared_ptr<DBC> dbc);
    virtual ~FAILOVER_CONNECTION_HANDLER();

    std::shared_ptr<CONNECTION_INTERFACE> connect(std::shared_ptr<HOST_INFO> host_info);
    void update_connection(std::shared_ptr<CONNECTION_INTERFACE> new_connection);
    void release_connection(std::shared_ptr<CONNECTION_INTERFACE> connection);

   private:
    std::shared_ptr<DBC> dbc;

    std::shared_ptr<DBC> clone_dbc(std::shared_ptr<DBC> source_dbc);
    void release_dbc(std::shared_ptr<DBC> dbc_clone);
};

struct READER_FAILOVER_RESULT {
    bool connected;
    std::shared_ptr<HOST_INFO> new_host;
    std::shared_ptr<CONNECTION_INTERFACE> new_connection;
};

class FAILOVER_READER_HANDLER {
   public:
    FAILOVER_READER_HANDLER(
        std::shared_ptr<TOPOLOGY_SERVICE> topology_service,
        std::shared_ptr<FAILOVER_CONNECTION_HANDLER> connection_handler);
    ~FAILOVER_READER_HANDLER();
    READER_FAILOVER_RESULT failover(
        std::shared_ptr<CLUSTER_TOPOLOGY_INFO> topology_info,
        const std::function<bool()> is_canceled);
    READER_FAILOVER_RESULT get_reader_connection(
        std::shared_ptr<CLUSTER_TOPOLOGY_INFO> topology_info,
        const std::function<bool()> is_canceled);

   private:
    std::shared_ptr<TOPOLOGY_SERVICE> topology_service;
    std::shared_ptr<FAILOVER_CONNECTION_HANDLER> connection_handler;

    std::vector<std::shared_ptr<HOST_INFO>> build_hosts_list(
        const std::shared_ptr<CLUSTER_TOPOLOGY_INFO>& topology_info,
        bool contain_writers);
    READER_FAILOVER_RESULT get_connection_from_hosts(
        std::vector<std::shared_ptr<HOST_INFO>> hosts_list,
        const std::function<bool()> is_canceled);
};

// This struct holds results of Writer Failover Process.
struct WRITER_FAILOVER_RESULT {
    bool connected;
    bool is_new_host;  // True if process connected to a new host. False if
                       // process re-connected to the same host
    std::shared_ptr<CLUSTER_TOPOLOGY_INFO> new_topology;
    std::shared_ptr<CONNECTION_INTERFACE> new_connection;
};

class FAILOVER_WRITER_HANDLER {
   public:
    FAILOVER_WRITER_HANDLER(
        std::shared_ptr<TOPOLOGY_SERVICE> topology_service,
        std::shared_ptr<FAILOVER_READER_HANDLER> reader_handler,
        std::shared_ptr<FAILOVER_CONNECTION_HANDLER> connection_handler,
        int writer_failover_timeout_ms, int read_topology_interval_ms,
        int reconnect_writer_interval_ms);
    ~FAILOVER_WRITER_HANDLER();
    WRITER_FAILOVER_RESULT failover(
        std::shared_ptr<CLUSTER_TOPOLOGY_INFO> current_topology);

   protected:
    int read_topology_interval_ms = 5000;     // 5 sec
    int reconnect_writer_interval_ms = 5000;  // 5 sec
    int writer_failover_timeout_ms = 60000;   // 60 sec

   private:
    std::shared_ptr<TOPOLOGY_SERVICE> topology_service;
    std::shared_ptr<FAILOVER_CONNECTION_HANDLER> connection_handler;
    std::shared_ptr<FAILOVER_READER_HANDLER> reader_handler;
};

class FAILOVER_HANDLER {
   private:
    DBC* dbc = nullptr;
    TOPOLOGY_SERVICE* topology_service;
    FAILOVER_READER_HANDLER* failover_reader_handler;
    FAILOVER_WRITER_HANDLER* failover_writer_handler;
    std::vector<HOST_INFO*>* hosts = nullptr;  // topology  - TODO not needed?
    HOST_INFO* current_host = nullptr;
    FAILOVER_CONNECTION_HANDLER* connection_handler = nullptr;

    bool is_cluster_topology_available = false;
    bool is_multi_writer_cluster = false;
    bool is_rds_proxy = false;
    bool is_rds = false;
    bool is_rds_custom_cluster = false;

    void init_cluster_info();
    bool is_failover_enabled();
    bool is_dns_pattern_valid(std::string host);
    bool is_rds_dns(std::string host);
    bool is_rds_proxy_dns(std::string host);
    bool is_rds_custom_cluster_dns(std::string host);
    void create_connection_and_initialize_topology();
    std::string get_rds_cluster_host_url(std::string host);
    std::string get_rds_instance_host_pattern(std::string host);
    bool is_ipv4(std::string host);
    bool is_ipv6(std::string host);
    bool failover_to_reader(const char*& new_error_code);
    bool failover_to_writer(const char*& new_error_code);
    void refresh_topology();

   public:
    FAILOVER_HANDLER(DBC* dbc, TOPOLOGY_SERVICE* topology_service);
    bool trigger_failover_if_needed(const char* error_code,
                                    const char*& new_error_code);
    ~FAILOVER_HANDLER();
};

// ************************************************************************************************
// These are failover utilities/helpers. Perhaps belong to a separate header
// file, but here for now
//

// FAILOVER_SYNC enables synchronization between threads
class FAILOVER_SYNC {
   public:
    FAILOVER_SYNC();
    void mark_as_done();
    void wait_for_done();
    void wait_for_done(int milliseconds);

   private:
    bool done_;
    std::mutex mutex_;
    std::condition_variable cv;
};

class FAILOVER {
   public:
    FAILOVER(std::shared_ptr<FAILOVER_CONNECTION_HANDLER> connection_handler,
             std::shared_ptr<TOPOLOGY_SERVICE> topology_service);
    virtual ~FAILOVER();
    void cancel();
    bool is_canceled();
    bool is_writer_connected();
    std::shared_ptr<CONNECTION_INTERFACE> get_connection();

   protected:
    bool connect(std::shared_ptr<HOST_INFO> host_info);
    void sleep(int miliseconds);
    std::shared_ptr<FAILOVER_CONNECTION_HANDLER> connection_handler;
    std::shared_ptr<TOPOLOGY_SERVICE> topology_service;

   private:
    std::atomic_bool canceled;
    std::shared_ptr<CONNECTION_INTERFACE> new_connection;

    void close_connection();
};

class RECONNECT_TO_WRITER_HANDLER : public FAILOVER {
   public:
    RECONNECT_TO_WRITER_HANDLER(
        std::shared_ptr<FAILOVER_CONNECTION_HANDLER> connection_handler,
        std::shared_ptr<TOPOLOGY_SERVICE> topology_servicets,
        int connection_interval);
    ~RECONNECT_TO_WRITER_HANDLER();

    WRITER_FAILOVER_RESULT operator()(
        const std::shared_ptr<HOST_INFO>& original_writer,
        FAILOVER_SYNC& f_sync);

   private:
    int reconnect_interval_ms;

    bool is_current_host_writer(
        const std::shared_ptr<HOST_INFO>& original_writer,
        const std::shared_ptr<CLUSTER_TOPOLOGY_INFO>& latest_topology);
};

class WAIT_NEW_WRITER_HANDLER : public FAILOVER {
   public:
    WAIT_NEW_WRITER_HANDLER(
        std::shared_ptr<FAILOVER_CONNECTION_HANDLER> connection_handler,
        std::shared_ptr<TOPOLOGY_SERVICE> topology_service,
        std::shared_ptr<CLUSTER_TOPOLOGY_INFO> current_topology,
        std::shared_ptr<FAILOVER_READER_HANDLER> reader_handler,
        int connection_interval);
    ~WAIT_NEW_WRITER_HANDLER();

    WRITER_FAILOVER_RESULT operator()(
        const std::shared_ptr<HOST_INFO>& original_writer,
        FAILOVER_SYNC& f_sync);

   private:
    // TODO - initialize in constructor and define constant for default value
    int read_topology_interval_ms = 5000;
    std::shared_ptr<FAILOVER_READER_HANDLER> reader_handler;
    std::shared_ptr<CLUSTER_TOPOLOGY_INFO> current_topology;
    std::shared_ptr<CONNECTION_INTERFACE> reader_connection;  // To retrieve latest topology
    std::shared_ptr<CONNECTION_INTERFACE> current_connection;
    std::shared_ptr<HOST_INFO> current_reader_host;

    void refresh_topology_and_connect_to_new_writer(
        const std::shared_ptr<HOST_INFO>& original_writer);
    void connect_to_reader();
    bool connect_to_writer(const std::shared_ptr<HOST_INFO>& writer_candidate);
    void clean_up_reader_connection();
};

#endif /* __FAILOVER_H__ */
