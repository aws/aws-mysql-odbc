/**
  @file failover.h
  @brief Definitions needed for failover
*/

#ifndef __FAILOVER_H__
#define __FAILOVER_H__

#include "driver.h"

#include <stddef.h>
#include <vector>
#include <list>
#include <string>
#include <map>
#include <set>
#include <ctime>
#include <functional> 
#include <condition_variable>

#include <mysql.h>

struct DBC;


//TODO Think about char types. Using strings for now, but should SQLCHAR *, or CHAR * be employed? 
//Most of the strings are for internal failover things
struct HOST_INFO {
    HOST_INFO();
    //TODO - probably choose one of the following constructors, or more precisely choose which data type they should take
    HOST_INFO(std::string url, std::string host, int port);
    HOST_INFO(const char* url, const char* host, int port);
    const int NO_PORT = -1;

    int get_port();
    std::string get_host();
    std::string get_host_port_pair();
    bool equal_host_port_pair(HOST_INFO& hi);
    bool is_host_down();
    bool is_host_writer();
    void mark_as_down(bool down);
    void mark_as_writer(bool writer);

    // used to be properties - TODO - remove the not needed one's
    std::string session_id;
    std::string last_updated;
    std::string replica_lag;
    std::string instance_name;

private:
    const std::string HOST_PORT_SEPARATOR = ":";  
    const std::string original_url;
    const std::string host;
    const int port;

    bool is_down;  // TODO is_down is probably not needed. Doesn't give us anything since we're operating on copies. Use down hosts set as in JDBC
    bool is_writer;
};

// This class holds topology information for one cluster.
// Cluster topology consists of an instance endpoint, a set of nodes in the cluster,
// the type of each node in the cluster, and the status of each node in the cluster.
class CLUSTER_TOPOLOGY_INFO {

public:
    CLUSTER_TOPOLOGY_INFO();
    CLUSTER_TOPOLOGY_INFO(const CLUSTER_TOPOLOGY_INFO& src_info); //copy constructor
    virtual ~CLUSTER_TOPOLOGY_INFO();

    void add_host(HOST_INFO* host_info);
    bool is_multi_writer_cluster();
    int total_hosts();
    int num_readers(); // return number of readers in the cluster
    std::time_t time_last_updated();

    HOST_INFO* get_writer();
    HOST_INFO* get_next_reader();
    // TODO - Ponder if the get_reader below is needed. In general user of this should not need to deal with indexes. 
    // One case that comes to mind, if we were to try to do a random shuffle of readers or hosts in general like JDBC driver
    // we could do random shuffle of host indices and call the get_reader for specific index in order we wanted.
    HOST_INFO* get_reader(int i);  
   
private:
    int current_reader = -1; 
    std::time_t last_updated; 
    std::set<std::string> down_hosts; // maybe not needed, HOST_INFO has is_host_down() method
    //std::vector<HOST_INFO*> hosts;
    HOST_INFO last_used_reader;  // TODO perhaps this overlaps with current_reader and is not needed

    // TODO - can we do without pointers - 
    // perhaps ok for now, we are using copies CLUSTER_TOPOLOGY_INFO returned by get_topology and get_cached_topology from TOPOLOGY_SERVICE. 
    // However, perhaps smart shared pointers could be used.
    std::vector<HOST_INFO*> writers;
    std::vector<HOST_INFO*> readers;

    void update_time();
};


class TOPOLOGY_SERVICE
{
public:
    TOPOLOGY_SERVICE();
    virtual ~TOPOLOGY_SERVICE();

    void set_cluster_id(const char* cluster_id);
    void set_cluster_instance_template(HOST_INFO* host_template);  //is this equivalent to setcluster_instance_host

    std::unique_ptr<CLUSTER_TOPOLOGY_INFO> get_topology(MYSQL* conn, bool force_update = false);
    std::unique_ptr<CLUSTER_TOPOLOGY_INFO> get_cached_topology();

    HOST_INFO* get_last_used_reader();
    void set_last_used_reader(HOST_INFO* reader);
    std::vector<std::string>* get_down_hosts();
    void mark_host_down(HOST_INFO* down_host);
    void unmark_host_down(HOST_INFO* host);
    void set_refresh_rate(int refresh_rate);
    void clear_all();
    void clear();

    // Property Keys
    const std::string SESSION_ID = "TOPOLOGY_SERVICE_SESSION_ID";
    const std::string LAST_UPDATED = "TOPOLOGY_SERVICE_LAST_UPDATE_TIMESTAMP";
    const std::string REPLICA_LAG = "TOPOLOGY_SERVICE_REPLICA_LAG_IN_MILLISECONDS";
    const std::string INSTANCE_NAME = "TOPOLOGY_SERVICE_SERVER_ID";

private:

    //TODO - consider - do we really need miliseconds for refresh? - the default numbers here are already 30 seconds.
    const int DEFAULT_REFRESH_RATE_IN_MILLISECONDS = 30000;
    const int DEFAULT_CACHE_EXPIRE_MS = 5 * 60 * 1000; // 5 min

    const std::string GET_INSTANCE_NAME_SQL = "SELECT @@aurora_server_id";
    const std::string GET_INSTANCE_NAME_COL = "@@aurora_server_id";
    const std::string WRITER_SESSION_ID = "MASTER_SESSION_ID";

    const std::string FIELD_SERVER_ID = "SERVER_ID";
    const std::string FIELD_SESSION_ID = "SESSION_ID";
    const std::string FIELD_LAST_UPDATED = "LAST_UPDATE_TIMESTAMP";
    const std::string FIELD_REPLICA_LAG = "REPLICA_LAG_IN_MILLISECONDS";

    const char* RETRIEVE_TOPOLOGY_SQL =
        "SELECT SERVER_ID, SESSION_ID, LAST_UPDATE_TIMESTAMP, REPLICA_LAG_IN_MILLISECONDS \
		FROM information_schema.replica_host_status \
		WHERE time_to_sec(timediff(now(), LAST_UPDATE_TIMESTAMP)) <= 300 \
		ORDER BY LAST_UPDATE_TIMESTAMP DESC";

protected:
    const int NO_CONNECTION_INDEX = -1;
    int refresh_rate_in_milliseconds;
    
    std::string cluster_id;
    HOST_INFO* cluster_instance_host;

    //TODO performance metrics
    //bool gather_perf_Metrics = false;

    //TODO implement cache
    std::map<std::string, CLUSTER_TOPOLOGY_INFO*> topology_cache;
    std::set<std::string> down_hosts; //TODO review the use of it

    bool refresh_needed(std::time_t last_updated);
    CLUSTER_TOPOLOGY_INFO* query_for_topology(MYSQL* conn);
    HOST_INFO* create_host(MYSQL_ROW& row);
    std::string  get_host_endpoint(const char* node_name);

    CLUSTER_TOPOLOGY_INFO* get_from_cache();
    void put_to_cache(CLUSTER_TOPOLOGY_INFO* topology_info);
};

class FAILOVER_CONNECTION_HANDLER {
public:
    FAILOVER_CONNECTION_HANDLER(DBC* dbc);
    virtual ~FAILOVER_CONNECTION_HANDLER();

    MYSQL* connect(/*required info*/ HOST_INFO* = NULL); // should use original host if NULL passed to connect?
    void update_connection(MYSQL* new_conn);
    void release_connection(MYSQL* mysql);

private:
    // originalHost information maybe here or maybe does not belong here. 
    // The idea is that if no topology is available, re-connection may be attempted using initial user supplied information.
    // just by caling connect(). This could be passed as a prameter to connect, but if used by failover handlers, they
    // would need original host informatin as well, while the FAILOVER_CONNECTION_HANDLER is passed to them. Unless, like in JDBC the original URL is
    // part of HOST_INFO
    HOST_INFO* original_host; 
    DBC* dbc;
    
    DBC* clone_dbc(DBC* source_dbc);
    void release_dbc(DBC* dbc);
};

struct READER_FAILOVER_RESULT {
    bool is_connected;
    std::vector<HOST_INFO*>* new_hosts;
    HOST_INFO* new_host;
    DBC* new_host_connection;
};

class FAILOVER_READER_HANDLER {
public:
    FAILOVER_READER_HANDLER(TOPOLOGY_SERVICE* topology_service);
    READER_FAILOVER_RESULT* failover(std::vector<HOST_INFO*>* hosts, HOST_INFO* current_host);
    READER_FAILOVER_RESULT* getReaderConnection(std::vector<HOST_INFO*> hosts);

    MYSQL* get_reader_connection(CLUSTER_TOPOLOGY_INFO& topology_info, FAILOVER_CONNECTION_HANDLER* conn_handler, const std::function <bool()> is_canceled);

    ~FAILOVER_READER_HANDLER();
};

struct WRITER_FAILOVER_RESULT {
    bool is_connected;
    bool is_new_host;
    std::vector<HOST_INFO*>* new_hosts;
    DBC* new_host_connection;
};

class FAILOVER_WRITER_HANDLER {
public:
    FAILOVER_WRITER_HANDLER(TOPOLOGY_SERVICE* topology_service, FAILOVER_READER_HANDLER* failover_reader_handler);
    WRITER_FAILOVER_RESULT failover(TOPOLOGY_SERVICE* topology_service, FAILOVER_CONNECTION_HANDLER* conn_handler, FAILOVER_READER_HANDLER& failover_reader_handler);
    ~FAILOVER_WRITER_HANDLER();
protected: 
    int read_topology_interval_ms = 5000;   // 5 sec
    int reconnect_writer_interval_ms = 5000; // 5 sec
    int writer_failover_timeout_ms = 300000; // 300 sec    TODO change it to something smaller e.g. - 60000; - 60 seconds, now for debuging is set to 5 minutes
    // see if this function may need to be used in other places so move it to some 'util' or static, etc.
    bool is_host_same(HOST_INFO* h1, HOST_INFO* h2);
    void sleep(int miliseconds); // similarily this function maybe used in more places.
};

class FAILOVER_HANDLER {
private:
    DBC* dbc = nullptr;
    TOPOLOGY_SERVICE* topology_service;
    FAILOVER_READER_HANDLER* failover_reader_handler;
    FAILOVER_WRITER_HANDLER* failover_writer_handler;      
    std::vector<HOST_INFO*>* hosts = nullptr; //topology  - TODO not needed?
    HOST_INFO* current_host = nullptr;
    FAILOVER_CONNECTION_HANDLER* conn_handler = nullptr;

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
    bool trigger_failover_if_needed(const char* error_code, const char*& new_error_code);
    ~FAILOVER_HANDLER();
};

// ************************************************************************************************
// These are failover utilities/helpers. Perhaps belong to a separate header file, but here for now
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
    std::condition_variable cond_var;
    bool is_done();
};

class FAILOVER {
public:
    explicit FAILOVER(std::shared_ptr<FAILOVER_CONNECTION_HANDLER> conn_handler);
    virtual ~FAILOVER();
    void cancel();
    bool is_canceled();
    bool connected();
    std::shared_ptr<MYSQL> get_connection();

protected:
    bool connect(std::shared_ptr<HOST_INFO> host_info);
    void sleep(int miliseconds);
    std::shared_ptr<FAILOVER_CONNECTION_HANDLER> conn_handler;

private:
    std::atomic_bool canceled;
    std::shared_ptr<MYSQL> new_conn;  // TODO probably wrap this 'raw' MYSQL* pointer in some result class so all the methods and interfaces never show it directly

    void close_connection();
};

class FAILOVER_RECONNECT_HANDLER : public FAILOVER {
public:  
    FAILOVER_RECONNECT_HANDLER(std::shared_ptr<FAILOVER_CONNECTION_HANDLER> conn_handler, int conn_interval);  // probably use smart shared pointer here
    ~FAILOVER_RECONNECT_HANDLER();

    void operator()(std::shared_ptr<HOST_INFO> hi, FAILOVER_SYNC& f_sync);

   
private:
    int reconnect_interval_ms; 
};

class WAIT_NEW_WRITER_HANDLER : public FAILOVER {
public:
    WAIT_NEW_WRITER_HANDLER(std::shared_ptr<FAILOVER_CONNECTION_HANDLER> conn_handler, std::shared_ptr<TOPOLOGY_SERVICE> topology_service, FAILOVER_READER_HANDLER& failover_reader_handler, int conn_interval);
    ~WAIT_NEW_WRITER_HANDLER();

    void operator()(FAILOVER_SYNC& f_sync);
private:
    int read_topology_interval_ms = 5000; // TODO - initialize in constructor and define constant for default value
    std::shared_ptr<TOPOLOGY_SERVICE> topology_service;
    FAILOVER_READER_HANDLER& reader_handler;

    bool refreshTopologyAndConnectToNewWriter(std::shared_ptr<MYSQL> reader_connection);
    std::shared_ptr<MYSQL> get_reader_connection();
};

#endif /* __FAILOVER_H__ */
