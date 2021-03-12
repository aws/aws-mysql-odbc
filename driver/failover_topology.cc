/**
  @file  failover_topology.c
  @brief Failover functions.
*/

#include <stdio.h>  // temporary for quick printfs

#include "failover.h"

//TODO naming convensions

ClusterTopologyInfo::~ClusterTopologyInfo() {
    for (auto p : writers) {
        delete p;
    }
    writers.clear();

    for (auto p : readers) {
        delete p;
    }
    readers.clear();
}

TOPOLOGY_SERVICE::TOPOLOGY_SERVICE()
    : clusterInstanceHost{ NULL }, refreshRateInMilliseconds{ DEFAULT_REFRESH_RATE_IN_MILLISECONDS }
{
    //TODO get better initial cluster id
    time_t now = time(0);
    clusterId = std::to_string(now) + ctime(&now);
}

void TOPOLOGY_SERVICE::setClusterId(const char* clusterId) {
    this->clusterId = clusterId;
}

void TOPOLOGY_SERVICE::setClusterInstanceTemplate(HOST_INFO* hostTemplate) {

    // NOTE, this may not have to be a pointer. Copy the information passed to this function.
    // Alernatively the create host function should be part of the Topologyservice, even protected or private one
    // and host information passed as separate parameters.
    if (clusterInstanceHost != NULL)
        delete clusterInstanceHost;

    clusterInstanceHost = hostTemplate;

}

void TOPOLOGY_SERVICE::setRefreshRate(int refreshRate) { refreshRateInMilliseconds = refreshRate; }
//std::vector<HOST_INFO*>* TOPOLOGY_SERVICE::getTopology() { return nullptr; }

//TODO (re)consider signagures of the following functions and implement  

HOST_INFO* TOPOLOGY_SERVICE::getLastUsedReaderHost() { return nullptr; }
void TOPOLOGY_SERVICE::setLastUsedReaderHost(HOST_INFO* reader) {}
std::vector<std::string>* TOPOLOGY_SERVICE::getDownHosts() { return nullptr; }
void TOPOLOGY_SERVICE::addToDownHostList(HOST_INFO* downHost) {}
void TOPOLOGY_SERVICE::removeFromDownHostList(HOST_INFO* host) {}

void TOPOLOGY_SERVICE::clearAll() {}
void TOPOLOGY_SERVICE::clear() {}
TOPOLOGY_SERVICE::~TOPOLOGY_SERVICE()
{
    if (clusterInstanceHost != NULL)
        delete clusterInstanceHost;

    for (auto p : topologyCache) {
        delete p.second;
    }
    topologyCache.clear();
}


const ClusterTopologyInfo* TOPOLOGY_SERVICE::getCachedTopology() { return getInfoFromCache(); }

//std::vector<HOST_INFO*>* TOPOLOGY_SERVICE::getTopology(MYSQL* conn)
//TODO consider the return value
//Note to determine whether or not forceUpdate succeeded one would compare 
// ClusterTopologyInfo->timeLastUpdated() prior and after the call if non-null information was given prior.
const ClusterTopologyInfo* TOPOLOGY_SERVICE::getTopology(MYSQL* conn, bool forceUpdate)
{
    //TODO reconsider using this cache. It appears that we only store information for the current cluster Id.
    // therefore instead of a map we can just keep ClusterTopologyInfo* topologyInfo member variable.
    ClusterTopologyInfo* topologyInfo = getInfoFromCache();

    if (topologyInfo == NULL
        || forceUpdate
        || refreshNeeded(topologyInfo->timeLastUpdated()))
    {
        putInfoToCache(queryForTopology(conn));
        topologyInfo = getInfoFromCache();
    }

    return topologyInfo;
}

// TODO consider thread safety and usage of pointers
ClusterTopologyInfo* TOPOLOGY_SERVICE::getInfoFromCache() {
    if (topologyCache.empty())
        return NULL;
    auto result = topologyCache.find(clusterId);
    return result != topologyCache.end() ? result->second : NULL;
}

// TODO consider thread safety and usage of pointers
void TOPOLOGY_SERVICE::putInfoToCache(ClusterTopologyInfo* topologyInfo) {
    if (topologyInfo == NULL)
        return;
    if (!topologyCache.empty())
    {
        auto result = topologyCache.find(clusterId);
        if (result != topologyCache.end()) {
            delete result->second;
            result->second = topologyInfo;
            return;
        }
    }
    topologyCache[clusterId] = topologyInfo;
}

// TODO harmonize time function accross objects so the times are comparable
bool TOPOLOGY_SERVICE::refreshNeeded(std::time_t last_updated) {

    return  time(0) - last_updated > (refreshRateInMilliseconds / 1000);
}

HOST_INFO* TOPOLOGY_SERVICE::createHost(MYSQL_ROW& row) {

    //TEMP and TODO figure out how to fetch values from row by name, not by ordinal for now this enum is matching 
    // order of columns in the query
    enum COLUMNS {
        SERVER_ID,
        SESSION,
        LAST_UPDATE_TIMESTAMP,
        REPLICA_LAG_MILLISECONDS
    };

    if (row[SERVER_ID] == NULL) {
        return NULL;  // will not be able to generate host endpoint so no point. TODO: log this condition?
    }

    std::string hostEndpoint = getHostEndpoint(row[SERVER_ID]);
    printf("Endpoint %s\n", hostEndpoint.c_str());

    //TODO check clusterInstanceHost for NULL, or decide what is needed out of it
    HOST_INFO* hostInfo = new HOST_INFO(
        "", // TODO no URL for now, it can be deduced similarily as in getUrlFromEndpoint in JDBC, or instead of having it as field, have function that return the expected string
        hostEndpoint,
        clusterInstanceHost->getPort(),
        clusterInstanceHost->getUser(),
        clusterInstanceHost->getPassword()
    );

    //TODO do case-insensitive comparison
    // side note: how stable this is on the server side? If it changes there we will not detect a writter.
    if (WRITER_SESSION_ID == row[SESSION])
    {
        hostInfo->markAswriter(true);
    }

    // add properties from server
    hostInfo->addProperty(INSTANCE_NAME, row[SERVER_ID] ? row[SERVER_ID] : "");
    hostInfo->addProperty(SESSION_ID, row[SESSION] ? row[SESSION] : "");
    hostInfo->addProperty(LAST_UPDATED, row[LAST_UPDATE_TIMESTAMP] ? row[LAST_UPDATE_TIMESTAMP] : "");
    hostInfo->addProperty(REPLICA_LAG, row[REPLICA_LAG_MILLISECONDS] ? row[REPLICA_LAG_MILLISECONDS] : "");

    // TODO copy properties from clusterInstanceHost equivalent from JDBC, but need to ponder about it
    // if really needed to copy the same to each host info if they are present in clusterInstanceHost
    // depending on the full new interface of the topology service
    // here is a snippet from JDBC
    //if (this.clusterInstanceHost != null) {
    //    properties.putAll(this.clusterInstanceHost.getHostProperties());
   // }

    //Temp Test properties TODO delete the printfs below.
    printf("%s :  %s\n", INSTANCE_NAME.c_str(), hostInfo->getProperty(INSTANCE_NAME).c_str());
    printf("%s :  %s\n", SESSION_ID.c_str(), hostInfo->getProperty(SESSION_ID).c_str());
    printf("%s :  %s\n", LAST_UPDATED.c_str(), hostInfo->getProperty(LAST_UPDATED).c_str());
    printf("%s :  %s\n", REPLICA_LAG.c_str(), hostInfo->getProperty(REPLICA_LAG).c_str());

    return hostInfo;
}

// If no host information retrieved return NULL
ClusterTopologyInfo* TOPOLOGY_SERVICE::queryForTopology(MYSQL* conn) {

    ClusterTopologyInfo* topologyInfo = NULL;

    MYSQL_RES* queryResult = NULL;
    if ((mysql_query(conn, RETRIEVE_TOPOLOGY_SQL) == 0) && (queryResult = mysql_store_result(conn)) != NULL) {
        topologyInfo = new ClusterTopologyInfo();
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(queryResult))) {
            HOST_INFO* hi = createHost(row);
            if (hi != NULL) {
                topologyInfo->addHost(hi);
            }
        }
        mysql_free_result(queryResult);

        if (topologyInfo->totalHosts() == 0) {
            delete topologyInfo;
            topologyInfo = NULL;
        }
    }
    return topologyInfo;
}


std::string TOPOLOGY_SERVICE::getHostEndpoint(const char* nodeName) {
    std::string host = clusterInstanceHost->getHost();
    host.replace(host.find("?"), 1, nodeName);

    return host;
}


//TDOO 
// the entire HOST_INFO needs to be reviewed based on needed interfaces and other objects like ClusterTopologyInfo
// most/all of the HOST_INFO potentially could be internal to ClusterTopologyInfo and specfic information may be accessed
// through ClusterTopologyInfo
// Move the implementation to it's own file

HOST_INFO::HOST_INFO()
    : HOST_INFO("", "", -1, "", "")
{
    //this(null, null, NO_PORT, null, null, true, null);
}

HOST_INFO::HOST_INFO(std::string url, std::string host, int port, std::string user, std::string password)
    : original_url{ url }, host{ host }, port{ port }, user{ user }, password{ password }, is_passwordless{ password == "" }, is_down{ false }, is_writer{ false }
{

}

// would need some checks for nulls
HOST_INFO::HOST_INFO(const char* url, const char* host, int port, const char* user, const char* password)
    : original_url{ url }, host{ host }, port{ port }, user{ user }, password{ password }, is_passwordless{ password == NULL }, is_down{ false }, is_writer{ false }
{

}


/**
 * Returns the host.
 *
 * @return the host
 */
std::string HOST_INFO::getHost() {
    return host;
}

/**
 * Returns the port.
 *
 * @return the port
 */
int  HOST_INFO::getPort() {
    return port;
}

/**
 * Returns a host:port representation of this host.
 *
 * @return the host:port representation of this host
 */
std::string HOST_INFO::getHostPortPair() {
    return getHost() + HOST_PORT_SEPARATOR + std::to_string(getPort());
}

/**
 * Returns the user name.
 *
 * @return the user name
 */
std::string HOST_INFO::getUser() {
    return user;
}

/**
 * Returns the password.
 *
 * @return the password
 */
std::string HOST_INFO::getPassword() {
    return password;
}

/**
 * Returns true if the is the default one, i.e., no password was provided in the connection URL or arguments.
 *
 * @return
 *         true if no password was provided in the connection URL or arguments.
 */
bool  HOST_INFO::isPasswordless() {
    return is_passwordless;
}

/**
 * Returns the properties specific to this host.
 *
 * @return this host specific properties
 */
std::map<std::string, std::string> HOST_INFO::getHostProperties() {
    return  hostProperties;
}

/**
 * Returns the connection argument for the given key.
 *
 * @param key
 *            key
 *
 * @return the connection argument for the given key
 */
std::string HOST_INFO::getProperty(std::string key) {
    if (hostProperties.empty())
        return "";
    auto result = hostProperties.find(key);
    return result != hostProperties.end() ? result->second : "";
}

// TODO think about behaviour. Should adding value with a key that exists in the map be replaced?
// Currently existing value for a key is replaced.
// If not, false should be returned from this method. 
// and use insert(std::make_pair()) instead of the [] operator
void HOST_INFO::addProperty(std::string name, std::string value) {
    hostProperties[name] = value;
}


/**
 * Shortcut to the database connection argument.
 *
 * @return the database name
 */
std::string HOST_INFO::getDatabase() {
    // String database = this.hostProperties.get(PropertyKey.DBNAME.getKeyName());
     //return isNullOrEmpty(database) ? "" : database;
    return "TODO";
}

/**
 * Exposes this host info as a single properties instance. The values for host, port, user and password are added to the properties map with their standard
 * keys.
 *
 * @return a {@link Properties} instance containing the full host information.
 */
 /*/
 public Properties exposeAsProperties() {
     Properties props = new Properties();
     this.hostProperties.entrySet().stream().forEach(e->props.setProperty(e.getKey(), e.getValue() == null ? "" : e.getValue()));
     props.setProperty(PropertyKey.HOST.getKeyName(), getHost());
     props.setProperty(PropertyKey.PORT.getKeyName(), String.valueOf(getPort()));
     props.setProperty(PropertyKey.USER.getKeyName(), getUser());
     props.setProperty(PropertyKey.PASSWORD.getKeyName(), getPassword());
     return props;
 }
 */

 /**
  * Returns the original database URL that produced this host info.
  *
  * @return the original database URL
  */

std::string HOST_INFO::getDatabaseUrl() {
    return original_url;
}

/**
 * Checks if this {@link HostInfo} has the same host and port pair as the given {@link HostInfo}.
 *
 * @param hi
 *            the {@link HostInfo} to compare with.
 * @return
 *         <code>true</code> if both objects have equal host and port pairs, <code>false</code> otherwise.
 */
bool  HOST_INFO::equalHostPortPair(HOST_INFO& hi) {
    // return (getHost() != null && getHost().equals(hi.getHost()) || getHost() == null && hi.getHost() == null) && getPort() == hi.getPort();
      //TODO implement
    return false;
}

