/**
  @file failover.h
  @brief Definitions needed for failover
*/

#ifndef __FAILOVER_H__
#define __FAILOVER_H__

//#include "sqltypes.h"
//#include "sql.h"

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
    HOST_INFO(std::string url, std::string host, int port, std::string user, std::string password);
    HOST_INFO(const char* url, const char* host, int port, const char* user, const char* password);
    const int NO_PORT = -1;

    //TODO naming convention - should function names be get_host() instead of getHost() etc.?
    std::string getHost();

    int  getPort();
    std::string getHostPortPair();
    std::string getUser();
    std::string getPassword();
    bool  isPasswordless();
    bool isDown() { return is_down; }
    bool isWriter() { return is_writer; }
    std::map<std::string, std::string> getHostProperties();
    std::string getProperty(std::string key);
    std::string getDatabase();
    std::string getDatabaseUrl();
    bool  equalHostPortPair(HOST_INFO& hi);
    void markAsDown(bool down) { is_down = down; }
    void markAswriter(bool writer) { is_writer = writer; }
    void addProperty(std::string name, std::string value);


private:
    const std::string HOST_PORT_SEPARATOR = ":";

    //const DatabaseUrlContainer originalUrl;  TODO - do we need an equivalent structure/class to DatabaseUrlContainer? Maybe not?
    const std::string original_url;
    const std::string host;
    const int port;
    const std::string user;
    const std::string password;
    const bool is_passwordless;
    std::map<std::string, std::string> hostProperties;
    bool is_down; // added, exploring possibility of using this vs having a separate set of down host - TODO decide if needed
    bool is_writer;

};

struct ClusterTopologyInfo {

public:
    ClusterTopologyInfo() { updateTime(); }
    void addHost(HOST_INFO* hi) {
        hi->isWriter() ? writers.push_back(hi) : readers.push_back(hi);
        updateTime();
    }
    bool isMultiWriterCluster() { return writers.size() > 1; }
    int totalHosts() { return writers.size() + readers.size(); }

    //TODO perhaps no need to return the entire lists, have more specific functions, e.g. specific writter or reader info returned
    // or even specific property and not entire HOST_INFO so that other parts don't need to know about HOST_INFO
    // just brain storming

    std::vector<HOST_INFO*>& Writers() { return writers; }
    std::vector<HOST_INFO*>& Readers() { return readers; }

    std::time_t timeLastUpdated() { return lastUpdated; }

    ~ClusterTopologyInfo();

private:
    std::time_t lastUpdated;  // is this time_t sufficient or we need to think about something else
    std::set<std::string> down_hosts;
    //std::vector<HOST_INFO*> hosts;

    // TODO - can we do withthout pointers
    std::vector<HOST_INFO*> writers;
    std::vector<HOST_INFO*> readers;

    HOST_INFO lastUsedReader;

    // TODO harmonize time function accross objects so the times are comparable
    void updateTime() {
        lastUpdated = time(0);
    }
};



class TOPOLOGY_SERVICE
{
private:

    //TODO - consider - do we really need miliseconds for refresh? - the default numbers here are already 30 seconds.
    const int DEFAULT_REFRESH_RATE_IN_MILLISECONDS = 30000;
    const int DEFAULT_CACHE_EXPIRE_MS = 5 * 60 * 1000; // 5 min

    int refreshRateInMilliseconds;
    //const std::string retrieve_topology_sql =
    //	"select server_id, session_id, last_update_timestamp, replica_lag_in_milliseconds "
    //    "from information_schema.replica_host_status "
    //	"where time_to_sec(timediff(now(), last_update_timestamp)) <= 300 " // 5 min
    //	"order by last_update_timestamp desc";
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

    //protected static final ExpiringCache<std::string, ClusterTopologyInfo> topologyCache =
    //	new ExpiringCache<>(DEFAULT_CACHE_EXPIRE_MS);
    //private static final Object cacheLock = new Object();

    std::string clusterId;
    HOST_INFO* clusterInstanceHost;

    //ClusterAwareTimeMetricsHolder queryTopologyMetrics =
        //new ClusterAwareTimeMetricsHolder("Topology Query");

    bool gatherPerfMetrics = false;
    ClusterTopologyInfo topologyInfoPlaceHolderForNow;

    //TODO implement cache
    std::map<std::string, ClusterTopologyInfo*> topologyCache;
    bool refreshNeeded(std::time_t last_updated);


    ClusterTopologyInfo* queryForTopology(MYSQL* conn);
    HOST_INFO* createHost(MYSQL_ROW& row);
    std::string  getHostEndpoint(const char* nodeName);

    ClusterTopologyInfo* getInfoFromCache();
    void putInfoToCache(ClusterTopologyInfo* topologyInfo);

public:
    TOPOLOGY_SERVICE();
    void setClusterId(const char* clusterId);
    void setClusterInstanceTemplate(HOST_INFO* hostTemplate);  //is this equivalent to setClusterInstanceHost
    //std::vector<HOST_INFO*>* getTopology(MYSQL* conn);
    const ClusterTopologyInfo* getTopology(MYSQL* conn, bool forceUpdate = false); // would this one make more sense returning ClusterTopologyInfo&?
    const ClusterTopologyInfo* getCachedTopology();
    // Should this one (in jdbc implementation use synchronized (cacheLock) {
    // similarily in getTopology when calling topologyCache.get(this.clusterId); some getTopology maybe calling put
    // or probably better, these locks could be inside the ExpiringCache itself, depends. how about other functions like clear, remove, values (while other thread is adding/removing things).
    // but if all access to the ExpiringCache goes through Topology Service then, the synchronization could be in Topology service.

    HOST_INFO* getLastUsedReaderHost();
    void setLastUsedReaderHost(HOST_INFO* reader);
    std::vector<std::string>* getDownHosts();
    void addToDownHostList(HOST_INFO* downHost);
    void removeFromDownHostList(HOST_INFO* host);
    void setRefreshRate(int refreshRate);
    void clearAll();
    void clear();
    ~TOPOLOGY_SERVICE();

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