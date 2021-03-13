/**
  @file  failover_topology.c
  @brief Failover functions.
*/

#include <stdio.h>  // temporary for quick printfs
#include "failover.h"


CLUSTER_TOPOLOGY_INFO::CLUSTER_TOPOLOGY_INFO() {
    update_time();
}

CLUSTER_TOPOLOGY_INFO::~CLUSTER_TOPOLOGY_INFO() {
    for (auto p : writers) {
        delete p;
    }
    writers.clear();

    for (auto p : readers) {
        delete p;
    }
    readers.clear();
}

void CLUSTER_TOPOLOGY_INFO::add_host(HOST_INFO* hi) {
    hi->is_host_writer() ? writers.push_back(hi) : readers.push_back(hi);
    update_time();
}

bool CLUSTER_TOPOLOGY_INFO::is_multi_writer_cluster() {
    return writers.size() > 1;
}

int CLUSTER_TOPOLOGY_INFO::total_hosts() {
    return writers.size() + readers.size();
}

std::time_t  CLUSTER_TOPOLOGY_INFO::time_last_updated() { 
    return last_updated; 
}

std::vector<HOST_INFO*>& CLUSTER_TOPOLOGY_INFO::writer_hosts() {
    return writers;
}

std::vector<HOST_INFO*>& CLUSTER_TOPOLOGY_INFO::reader_hosts() {
    return readers;
}

// TODO harmonize time function accross objects so the times are comparable
void CLUSTER_TOPOLOGY_INFO::update_time() {
    last_updated = time(0);
}


TOPOLOGY_SERVICE::TOPOLOGY_SERVICE()
    : cluster_instance_host{ NULL }, refresh_rate_in_milliseconds{ DEFAULT_REFRESH_RATE_IN_MILLISECONDS }
{
    //TODO get better initial cluster id
    time_t now = time(0);
    cluster_id = std::to_string(now) + ctime(&now);
}

void TOPOLOGY_SERVICE::set_cluster_id(const char* cluster_id) {
    this->cluster_id = cluster_id;
}

void TOPOLOGY_SERVICE::set_cluster_instance_template(HOST_INFO* host_template) {

    // NOTE, this may not have to be a pointer. Copy the information passed to this function.
    // Alernatively the create host function should be part of the Topologyservice, even protected or private one
    // and host information passed as separate parameters.
    if (cluster_instance_host != NULL)
        delete cluster_instance_host;

    cluster_instance_host = host_template;

}

void TOPOLOGY_SERVICE::set_refresh_rate(int refresh_rate) { 
    refresh_rate_in_milliseconds = refresh_rate; 
}

//TODO (re)consider signagures of the following functions and implement  
HOST_INFO* TOPOLOGY_SERVICE::get_last_used_reader_host() { return nullptr; }
void TOPOLOGY_SERVICE::set_last_used_reader_host(HOST_INFO* reader) {}
std::vector<std::string>* TOPOLOGY_SERVICE::get_down_hosts() { return nullptr; }
void TOPOLOGY_SERVICE::add_to_down_host_list(HOST_INFO* down_host) {}
void TOPOLOGY_SERVICE::remove_from_down_host_list(HOST_INFO* host) {}

void TOPOLOGY_SERVICE::clear_all() {}
void TOPOLOGY_SERVICE::clear() {}
TOPOLOGY_SERVICE::~TOPOLOGY_SERVICE()
{
    if (cluster_instance_host != NULL)
        delete cluster_instance_host;

    for (auto p : topology_cache) {
        delete p.second;
    }
    topology_cache.clear();
}


const CLUSTER_TOPOLOGY_INFO* TOPOLOGY_SERVICE::get_cached_topology() { 
    return get_from_cache(); 
}

//std::vector<HOST_INFO*>* TOPOLOGY_SERVICE::get_topology(MYSQL* conn)
//TODO consider the return value
//Note to determine whether or not force_update succeeded one would compare 
// CLUSTER_TOPOLOGY_INFO->time_last_updated() prior and after the call if non-null information was given prior.
const CLUSTER_TOPOLOGY_INFO* TOPOLOGY_SERVICE::get_topology(MYSQL* conn, bool force_update)
{
    //TODO reconsider using this cache. It appears that we only store information for the current cluster Id.
    // therefore instead of a map we can just keep CLUSTER_TOPOLOGY_INFO* topology_info member variable.
    CLUSTER_TOPOLOGY_INFO* topology_info = get_from_cache();

    if (topology_info == NULL
        || force_update
        || refresh_needed(topology_info->time_last_updated()))
    {
        put_to_cache(query_for_topology(conn));
        topology_info = get_from_cache();
    }

    return topology_info;
}

// TODO consider thread safety and usage of pointers
CLUSTER_TOPOLOGY_INFO* TOPOLOGY_SERVICE::get_from_cache() {
    if (topology_cache.empty())
        return NULL;
    auto result = topology_cache.find(cluster_id);
    return result != topology_cache.end() ? result->second : NULL;
}

// TODO consider thread safety and usage of pointers
void TOPOLOGY_SERVICE::put_to_cache(CLUSTER_TOPOLOGY_INFO* topology_info) {
    if (topology_info == NULL)
        return;
    if (!topology_cache.empty())
    {
        auto result = topology_cache.find(cluster_id);
        if (result != topology_cache.end()) {
            delete result->second;
            result->second = topology_info;
            return;
        }
    }
    topology_cache[cluster_id] = topology_info;
}

// TODO harmonize time function accross objects so the times are comparable
bool TOPOLOGY_SERVICE::refresh_needed(std::time_t last_updated) {

    return  time(0) - last_updated > (refresh_rate_in_milliseconds / 1000);
}

HOST_INFO* TOPOLOGY_SERVICE::create_host(MYSQL_ROW& row) {

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

    std::string host_endpoint = get_host_endpoint(row[SERVER_ID]);

    //TODO check cluster_instance_host for NULL, or decide what is needed out of it
    HOST_INFO* host_info = new HOST_INFO(
        host_endpoint,
        cluster_instance_host->get_port()
    );

    //TODO do case-insensitive comparison
    // side note: how stable this is on the server side? If it changes there we will not detect a writter.
    if (WRITER_SESSION_ID == row[SESSION])
    {
        host_info->mark_as_writer(true);
    }

    // add properties from server
    host_info->add_property(INSTANCE_NAME, row[SERVER_ID] ? row[SERVER_ID] : "");
    host_info->add_property(SESSION_ID, row[SESSION] ? row[SESSION] : "");
    host_info->add_property(LAST_UPDATED, row[LAST_UPDATE_TIMESTAMP] ? row[LAST_UPDATE_TIMESTAMP] : "");
    host_info->add_property(REPLICA_LAG, row[REPLICA_LAG_MILLISECONDS] ? row[REPLICA_LAG_MILLISECONDS] : "");

    // TODO copy properties from cluster_instance_host equivalent from JDBC, but need to ponder about it
    // if really needed to copy the same to each host info if they are present in cluster_instance_host
    // depending on the full new interface of the topology service
    // here is a snippet from JDBC
    //if (this.cluster_instance_host != null) {
    //    properties.putAll(this.cluster_instance_host.host_properties());
   // }

    return host_info;
}

// If no host information retrieved return NULL
CLUSTER_TOPOLOGY_INFO* TOPOLOGY_SERVICE::query_for_topology(MYSQL* conn) {

    CLUSTER_TOPOLOGY_INFO* topology_info = NULL;

    MYSQL_RES* query_result = NULL;
    if ((mysql_query(conn, RETRIEVE_TOPOLOGY_SQL) == 0) && (query_result = mysql_store_result(conn)) != NULL) {
        topology_info = new CLUSTER_TOPOLOGY_INFO();
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(query_result))) {
            HOST_INFO* hi = create_host(row);
            if (hi != NULL) {
                topology_info->add_host(hi);
            }
        }
        mysql_free_result(query_result);

        if (topology_info->total_hosts() == 0) {
            delete topology_info;
            topology_info = NULL;
        }
    }
    return topology_info;
}


std::string TOPOLOGY_SERVICE::get_host_endpoint(const char* node_name) {
    std::string host = cluster_instance_host->get_host();
    host.replace(host.find("?"), 1, node_name);

    return host;
}


//TDOO 
// the entire HOST_INFO needs to be reviewed based on needed interfaces and other objects like CLUSTER_TOPOLOGY_INFO
// most/all of the HOST_INFO potentially could be internal to CLUSTER_TOPOLOGY_INFO and specfic information may be accessed
// through CLUSTER_TOPOLOGY_INFO
// Move the implementation to it's own file

HOST_INFO::HOST_INFO()
    : HOST_INFO("", -1)
{
    //this(null, null, NO_PORT, null, null, true, null);
}

HOST_INFO::HOST_INFO(std::string host, int port)
    : host{ host }, port{ port }, is_down{ false }, is_writer{ false }
{

}

// would need some checks for nulls
HOST_INFO::HOST_INFO(const char* host, int port)
    : host{ host }, port{ port }, is_down{ false }, is_writer{ false }
{

}


/**
 * Returns the host.
 *
 * @return the host
 */
std::string HOST_INFO::get_host() {
    return host;
}

/**
 * Returns the port.
 *
 * @return the port
 */
int  HOST_INFO::get_port() {
    return port;
}

/**
 * Returns a host:port representation of this host.
 *
 * @return the host:port representation of this host
 */
std::string HOST_INFO::get_host_port_pair() {
    return get_host() + HOST_PORT_SEPARATOR + std::to_string(get_port());
}

/**
 * Returns the properties specific to this host.
 *
 * @return this host specific properties
 */
std::map<std::string, std::string> HOST_INFO::host_properties() {
    return  properties;
}

/**
 * Returns the connection argument for the given key.
 *
 * @param key
 *            key
 *
 * @return the connection argument for the given key
 */
std::string HOST_INFO::get_property(std::string key) {
    if (properties.empty())
        return "";
    auto result = properties.find(key);
    return result != properties.end() ? result->second : "";
}

// TODO think about behaviour. Should adding value with a key that exists in the map be replaced?
// Currently existing value for a key is replaced.
// If not, false should be returned from this method. 
// and use insert(std::make_pair()) instead of the [] operator
void HOST_INFO::add_property(std::string name, std::string value) {
    properties[name] = value;
}


/**
 * Shortcut to the database connection argument.
 *
 * @return the database name
 */
std::string HOST_INFO::get_database() {
    // String database = this.properties.get(PropertyKey.DBNAME.getKeyName());
     //return isNullOrEmpty(database) ? "" : database;
    return "TODO";
}

/**
 * Checks if this {@link host_info} has the same host and port pair as the given {@link host_info}.
 *
 * @param hi
 *            the {@link host_info} to compare with.
 * @return
 *         <code>true</code> if both objects have equal host and port pairs, <code>false</code> otherwise.
 */
bool  HOST_INFO::equal_host_port_pair(HOST_INFO& hi) {
    // return (get_host() != null && get_host().equals(hi.get_host()) || get_host() == null && hi.get_host() == null) && get_port() == hi.get_port();
      //TODO implement
    return false;
}

bool HOST_INFO::is_host_down() {
    return is_down;
}

void HOST_INFO::mark_as_down(bool down) {
    is_down = down;
}

bool HOST_INFO::is_host_writer() {
    return is_writer;
}
void HOST_INFO::mark_as_writer(bool writer) {
    is_writer = writer;
}
