/**
  @file failover.h
  @brief Definitions needed for failover
*/

#ifndef __FAILOVER_H__
#define __FAILOVER_H__

#include <stddef.h>
#include <vector>
#include <list>
#include <string>
#include <map>
#include <set>
#include <ctime>

#include <mysql.h>

struct DBC;

//TODO - decide whether or not to keep all the class/struct definitions in on header file or split.
// Given that driver.h contains all the definitions for the driver struc/class having one file here may be consistent with
// the way driver.h is

//TODO Think about char types, strings for now, but should SQLCHAR *, or CHAR * be employed? 
struct HOST_INFO {
    HOST_INFO();
    //TODO - probably choose one of the following constructors, or more precisely choose which data type they should take
    HOST_INFO(std::string host, int port);
    HOST_INFO(const char* host, int port);
    const int NO_PORT = -1;

    std::string get_host();
    int  get_port();
    std::string get_host_port_pair();
    std::map<std::string, std::string> host_properties();
    std::string get_property(std::string key);
    void add_property(std::string name, std::string value);
    std::string get_database();
    bool  equal_host_port_pair(HOST_INFO& hi);
    bool is_host_down();
    bool is_host_writer();
    void mark_as_down(bool down);
    void mark_as_writer(bool writer);

private:
    const std::string HOST_PORT_SEPARATOR = ":";
    const std::string host;
    const int port;
    std::map<std::string, std::string> properties;
    bool is_down;
    bool is_writer;
};

class CLUSTER_TOPOLOGY_INFO {

public:
    CLUSTER_TOPOLOGY_INFO();
    ~CLUSTER_TOPOLOGY_INFO();

    void add_host(HOST_INFO* hi);
    bool is_multi_writer_cluster();
    int total_hosts();
    std::time_t time_last_updated();

    //TODO perhaps no need to return the entire lists, have more specific functions, e.g. specific writter or reader info returned
    // or even specific property and not entire HOST_INFO so that other parts don't need to know about HOST_INFO
    // just brain storming
    std::vector<HOST_INFO*>& writer_hosts();
    std::vector<HOST_INFO*>& reader_hosts();

private:
    std::time_t last_updated; 
    std::set<std::string> down_hosts; // maybe not needed, HOST_INFO has is_host_down() method
    //std::vector<HOST_INFO*> hosts;
    HOST_INFO last_used_reader;

    // TODO - can we do withthout pointers
    std::vector<HOST_INFO*> writers;
    std::vector<HOST_INFO*> readers;

    void update_time();
};


class TOPOLOGY_SERVICE
{
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

    bool refresh_needed(std::time_t last_updated);
    CLUSTER_TOPOLOGY_INFO* query_for_topology(MYSQL* conn);
    HOST_INFO* create_host(MYSQL_ROW& row);
    std::string  get_host_endpoint(const char* node_name);

    CLUSTER_TOPOLOGY_INFO* get_from_cache();
    void put_to_cache(CLUSTER_TOPOLOGY_INFO* topology_info);

public:
    TOPOLOGY_SERVICE();
    ~TOPOLOGY_SERVICE();

    void set_cluster_id(const char* cluster_id);
    void set_cluster_instance_template(HOST_INFO* host_template);  //is this equivalent to setcluster_instance_host
    //std::vector<HOST_INFO*>* get_topology(MYSQL* conn);
    const CLUSTER_TOPOLOGY_INFO* get_topology(MYSQL* conn, bool force_update = false); 
    const CLUSTER_TOPOLOGY_INFO* get_cached_topology();

    HOST_INFO* get_last_used_reader_host();
    void set_last_used_reader_host(HOST_INFO* reader);
    std::vector<std::string>* get_down_hosts();
    void add_to_down_host_list(HOST_INFO* down_host);
    void remove_from_down_host_list(HOST_INFO* host);
    void set_refresh_rate(int refresh_rate);
    void clear_all();
    void clear();

    // Property Keys
    const std::string SESSION_ID = "TOPOLOGY_SERVICE_SESSION_ID";
    const std::string LAST_UPDATED = "TOPOLOGY_SERVICE_LAST_UPDATE_TIMESTAMP";
    const std::string REPLICA_LAG = "TOPOLOGY_SERVICE_REPLICA_LAG_IN_MILLISECONDS";
    const std::string INSTANCE_NAME = "TOPOLOGY_SERVICE_SERVER_ID";
};


struct READER_FAILOVER_RESULT {
    //TODO
};

class FAILOVER_READER_HANDLER {
public:
    FAILOVER_READER_HANDLER();
    READER_FAILOVER_RESULT* failover(std::vector<HOST_INFO*> hosts, HOST_INFO* currentHost);
    READER_FAILOVER_RESULT* getReaderConnection(std::vector<HOST_INFO*> hosts);
    ~FAILOVER_READER_HANDLER();
};

struct WRITER_FAILOVER_RESULT {
    //TODO
};

class FAILOVER_WRITER_HANDLER {
public:
    FAILOVER_WRITER_HANDLER();
    WRITER_FAILOVER_RESULT* failover(std::vector<HOST_INFO*> hosts);
    ~FAILOVER_WRITER_HANDLER();
};

class FAILOVER_HANDLER {
private:
    DBC* dbc = nullptr;
    TOPOLOGY_SERVICE* ts = nullptr;
    FAILOVER_READER_HANDLER* frh = nullptr;
    FAILOVER_WRITER_HANDLER* fwh = nullptr;
    std::vector<HOST_INFO*>* hosts = nullptr; //topology

public:
    FAILOVER_HANDLER(DBC* dbc);
    bool triggerFailoverIfNeeded(const char* errorCode, const char*& newErrorCode);
    ~FAILOVER_HANDLER();
};

#endif /* __FAILOVER_H__ */