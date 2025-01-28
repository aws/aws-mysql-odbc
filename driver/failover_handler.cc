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

/**
  @file  failover_handler.c
  @brief Failover functions.
*/

#include <sstream>

#include "driver.h"
#include "mylog.h"
#include "rds_utils.h"

#if defined(__APPLE__) || defined(__linux__)
    #include <arpa/inet.h>    
    #include <netdb.h>
    #include <netinet/in.h>
    #include <sys/socket.h>
    #include <sys/types.h>
#endif

namespace {
const char* MYSQL_READONLY_QUERY = "SELECT @@innodb_read_only AS is_reader";
}  // namespace

FAILOVER_HANDLER::FAILOVER_HANDLER(DBC* dbc, DataSource* ds)
    : FAILOVER_HANDLER(
        dbc, ds, dbc ? dbc->connection_handler : nullptr, dbc ? dbc->get_topology_service() : nullptr,
        std::make_shared<CLUSTER_AWARE_METRICS_CONTAINER>(dbc, ds)) {}

FAILOVER_HANDLER::FAILOVER_HANDLER(DBC* dbc, DataSource* ds,
                                   std::shared_ptr<CONNECTION_HANDLER> connection_handler,
                                   std::shared_ptr<TOPOLOGY_SERVICE> topology_service,
                                   std::shared_ptr<CLUSTER_AWARE_METRICS_CONTAINER> metrics_container) {
    if (!dbc) {
        throw std::runtime_error("DBC cannot be null.");
    }

    if (!ds) {
        throw std::runtime_error("DataSource cannot be null.");
    }

    this->dbc = dbc;
    this->ds = ds;
    this->topology_service = topology_service;
    this->topology_service->set_refresh_rate(ds->opt_TOPOLOGY_REFRESH_RATE);
    this->topology_service->set_gather_metric(ds->opt_GATHER_PERF_METRICS);
    this->connection_handler = connection_handler;

    this->failover_reader_handler = std::make_shared<FAILOVER_READER_HANDLER>(
        this->topology_service, this->connection_handler,
        dbc->env->failover_thread_pool, ds->opt_FAILOVER_TIMEOUT,
        ds->opt_FAILOVER_READER_CONNECT_TIMEOUT,
        is_failover_mode(FAILOVER_MODE_STRICT_READER, ds), dbc->id,
        ds->opt_LOG_QUERY);
    this->failover_writer_handler = std::make_shared<FAILOVER_WRITER_HANDLER>(
        this->topology_service, this->failover_reader_handler,
        this->connection_handler, dbc->env->failover_thread_pool,
        ds->opt_FAILOVER_TIMEOUT, ds->opt_FAILOVER_TOPOLOGY_REFRESH_RATE,
        ds->opt_FAILOVER_WRITER_RECONNECT_INTERVAL, dbc->id, ds->opt_LOG_QUERY);
    this->metrics_container = metrics_container;
}

FAILOVER_HANDLER::~FAILOVER_HANDLER() {}

SQLRETURN FAILOVER_HANDLER::init_connection() {
    SQLRETURN rc = connection_handler->do_connect(dbc, ds, false);
    if (SQL_SUCCEEDED(rc)) {
        metrics_container->register_invalid_initial_connection(false);
    }
    else {
        metrics_container->register_invalid_initial_connection(true);
        return rc;
    }

    bool reconnect_with_updated_timeouts = false;
    if (ds->opt_ENABLE_CLUSTER_FAILOVER) {
        this->init_cluster_info();

        if (is_failover_enabled()) {
            // Since we can't determine whether failover should be enabled
            // before we connect, there is a possibility we need to reconnect
            // again with the correct connection settings for failover.
            const unsigned int connect_timeout = get_connect_timeout(ds->opt_CONNECT_TIMEOUT);
            const unsigned int network_timeout = get_network_timeout(ds->opt_NETWORK_TIMEOUT);

            reconnect_with_updated_timeouts = (connect_timeout != dbc->login_timeout ||
                                               network_timeout != ds->opt_READTIMEOUT ||
                                               network_timeout != ds->opt_WRITETIMEOUT);
        }

        if (!ds->opt_FAILOVER_MODE) {
            if (RDS_UTILS::is_rds_reader_cluster_dns(this->current_host->get_host())) {
                ds->opt_FAILOVER_MODE.set_remove_brackets((SQLWCHAR*)to_sqlwchar_string(FAILOVER_MODE_READER_OR_WRITER).c_str(), SQL_NTS);
            } else {
                ds->opt_FAILOVER_MODE.set_remove_brackets((SQLWCHAR*)to_sqlwchar_string(FAILOVER_MODE_STRICT_WRITER).c_str(), SQL_NTS);
            }
        }
    }

    if (should_connect_to_new_writer() || reconnect_with_updated_timeouts) {
        rc = reconnect(reconnect_with_updated_timeouts);
    }

    return rc;
}

void FAILOVER_HANDLER::init_cluster_info() {
    if (is_cluster_info_initialized) {
        return;
    }
    
    std::stringstream err;
    // Cluster-aware failover is enabled

    this->current_host = get_host_info_from_ds(ds);
    std::string main_host = this->current_host->get_host();
    unsigned int main_port = this->current_host->get_port();

    const char* hp = (const char*)ds->opt_HOST_PATTERN;
    std::string hp_str(hp ? hp : "");

    const char* clid = (const char*)ds->opt_CLUSTER_ID;
    std::string clid_str(clid ? clid : "");

    if (!hp_str.empty()) {
        unsigned int port = !ds->opt_PORT.is_default() ? ds->opt_PORT : MYSQL_PORT;
        std::vector<Srv_host_detail> host_patterns;

        try {
            host_patterns = parse_host_list(hp_str.c_str(), port);
        } catch (std::string&) {
            err << "Invalid host pattern: '" << hp_str << "' - the value could not be parsed";
            MYLOG_TRACE(dbc->log_file, dbc->id, err.str().c_str());
            throw std::runtime_error(err.str());
        }

        if (host_patterns.empty()) {
            err << "Empty host pattern.";
            MYLOG_DBC_TRACE(dbc, err.str().c_str());
            throw std::runtime_error(err.str());
        }

        std::string host_pattern(host_patterns[0].name);
        unsigned int host_pattern_port = host_patterns[0].port;

        if (!RDS_UTILS::is_dns_pattern_valid(host_pattern)) {
            err << "Invalid host pattern: '" << host_pattern
                << "' - the host pattern must contain a '?' character as a "
                    "placeholder for the DB instance identifiers  of the cluster "
                    "instances";
            MYLOG_DBC_TRACE(dbc, err.str().c_str());
            throw std::runtime_error(err.str());
        }

        auto host_template = std::make_shared<HOST_INFO>(host_pattern, host_pattern_port);
        topology_service->set_cluster_instance_template(host_template);

        m_is_rds = RDS_UTILS::is_rds_dns(host_pattern);
        MYLOG_DBC_TRACE(dbc, "[FAILOVER_HANDLER] m_is_rds=%s", m_is_rds ? "true" : "false");
        m_is_rds_proxy = RDS_UTILS::is_rds_proxy_dns(host_pattern);
        MYLOG_DBC_TRACE(dbc, "[FAILOVER_HANDLER] m_is_rds_proxy=%s", m_is_rds_proxy ? "true" : "false");
        m_is_rds_custom_cluster = RDS_UTILS::is_rds_custom_cluster_dns(host_pattern);

        if (m_is_rds_proxy) {
            err << "RDS Proxy url can't be used as an instance pattern.";
            MYLOG_DBC_TRACE(dbc, err.str().c_str());
            throw std::runtime_error(err.str());
        }

        if (m_is_rds_custom_cluster) {
            err << "RDS Custom Cluster endpoint can't be used as an instance pattern.";
            MYLOG_DBC_TRACE(dbc, err.str().c_str());
            throw std::runtime_error(err.str());
        }

        if (!clid_str.empty()) {
            set_cluster_id(clid_str);

        } else if (m_is_rds) {
            // If it's a cluster endpoint, or a reader cluster endpoint, then
            // let's use as cluster identification
            std::string cluster_rds_host =
                RDS_UTILS::get_rds_cluster_host_url(host_pattern);
            if (!cluster_rds_host.empty()) {
                set_cluster_id(cluster_rds_host, host_pattern_port);
            }
        }

        initialize_topology();
    } else if (RDS_UTILS::is_ipv4(main_host) || RDS_UTILS::is_ipv6(main_host)) {
        // TODO: do we need to setup host template in this case?
        // HOST_INFO* host_template = new HOST_INFO();
        // host_template->host.assign(main_host);
        // host_template->port = main_port;
        // ts->setClusterInstanceTemplate(host_template);

        if (!clid_str.empty()) {
            set_cluster_id(clid_str);
        }

        initialize_topology();

        if (m_is_cluster_topology_available) {
            err << "Host Pattern configuration setting is required when IP "
                    "address is used to connect to a cluster that provides topology "
                    "information. If you would instead like to connect without "
                    "failover functionality, set the 'Disable Cluster Failover' "
                    "configuration property to true.";
            MYLOG_DBC_TRACE(dbc, err.str().c_str());
            throw std::runtime_error(err.str());
        }

        m_is_rds = false;        // actually we don't know
        m_is_rds_proxy = false;  // actually we don't know

    } else {
        m_is_rds = RDS_UTILS::is_rds_dns(main_host);
        MYLOG_DBC_TRACE(dbc, "[FAILOVER_HANDLER] m_is_rds=%s", m_is_rds ? "true" : "false");
        m_is_rds_proxy = RDS_UTILS::is_rds_proxy_dns(main_host);
        MYLOG_DBC_TRACE(dbc, "[FAILOVER_HANDLER] m_is_rds_proxy=%s", m_is_rds_proxy ? "true" : "false");

        if (!m_is_rds) {
            // it's not RDS, maybe custom domain (CNAME)
            auto host_template =
                std::make_shared<HOST_INFO>(main_host, main_port);
            topology_service->set_cluster_instance_template(host_template);

            if (!clid_str.empty()) {
                set_cluster_id(clid_str);
            }

            initialize_topology();

            if (m_is_cluster_topology_available) {
              err << "The provided host appears to be a custom domain. The "
                     "driver requires the Host Pattern configuration setting "
                     "to be set for custom domains. If you would instead like "
                     "to connect without  failover functionality, set the "
                     "'Disable Cluster Failover' configuration property to true.";
                MYLOG_DBC_TRACE(dbc, err.str().c_str());
                throw std::runtime_error(err.str());
            }
        } else {
            // It's RDS

            std::string rds_instance_host = RDS_UTILS::get_rds_instance_host_pattern(main_host);
            if (!rds_instance_host.empty()) {
                topology_service->set_cluster_instance_template(
                    std::make_shared<HOST_INFO>(rds_instance_host, main_port));
            } else {
                err << "The provided host does not appear to match an expected "
                       "Aurora DNS pattern. Please set the Host Pattern "
                       "configuration to specify the host pattern for the "
                       "cluster you are trying to connect to.";
                MYLOG_DBC_TRACE(dbc, err.str().c_str());
                throw std::runtime_error(err.str());
            }

            if (!clid_str.empty()) {
                set_cluster_id(clid_str);
            } else if (m_is_rds_proxy) {
                // Each proxy is associated with a single cluster so it's safe
                // to use RDS Proxy Url as cluster identification
                set_cluster_id(main_host, main_port);
            } else {
                // If it's cluster endpoint or reader cluster endpoint,
                // then let's use as cluster identification

                std::string cluster_rds_host = RDS_UTILS::get_rds_cluster_host_url(main_host);
                if (!cluster_rds_host.empty()) {
                    set_cluster_id(cluster_rds_host, main_port);
                } else {
                    // Main host is an instance endpoint
                    set_cluster_id(main_host, main_port);
                }
            }

            initialize_topology();
        }
    }

    is_cluster_info_initialized = true;
}

bool FAILOVER_HANDLER::should_connect_to_new_writer() {
    auto host = (const char*)ds->opt_SERVER;
    if (host == nullptr || host == "") {
        return false;
    }

    if (!RDS_UTILS::is_rds_writer_cluster_dns(host)) {
        return false;
    }

    std::string host_ip = host_to_IP(host);
    if (host_ip == "") {
        return false;
    }

    this->init_cluster_info();

    // We need to force refresh the topology if we are connected to a read only instance.
    auto topology = topology_service->get_topology(dbc->connection_proxy, is_read_only());

    std::shared_ptr<HOST_INFO> writer;
    try {
        writer = topology->get_writer();
    }
    catch (std::runtime_error) {
        return false;
    }

    std::string writer_host = writer->get_host();
    if (RDS_UTILS::is_rds_cluster_dns(writer_host.c_str())) {
        return false;
    }

    std::string writer_host_ip = host_to_IP(writer_host);
    if (writer_host_ip == "" || writer_host_ip == host_ip) {
        return false;
    }

    // DNS must have resolved the cluster endpoint to a wrong writer
    // so we should reconnect to a proper writer node.
    const sqlwchar_string writer_host_wstr = to_sqlwchar_string(writer_host);
    ds->opt_SERVER.set_remove_brackets((SQLWCHAR*)writer_host_wstr.c_str(), writer_host_wstr.size());

    return true;
}

void FAILOVER_HANDLER::set_cluster_id(std::string host, int port) {
    set_cluster_id(host + ":" + std::to_string(port));
}

void FAILOVER_HANDLER::set_cluster_id(std::string cid) {
    this->cluster_id = cid;
    topology_service->set_cluster_id(this->cluster_id);
    metrics_container->set_cluster_id(this->cluster_id);
}

bool FAILOVER_HANDLER::is_read_only() {
    bool read_only = false;
    if (dbc->connection_proxy->query(MYSQL_READONLY_QUERY) == 0) {
        auto result = dbc->connection_proxy->store_result();
        MYSQL_ROW row;
        if (row = dbc->connection_proxy->fetch_row(result)) {
            read_only = (strcmp(row[0], "1") == 0);
        }
        dbc->connection_proxy->free_result(result);
    }

    return read_only;
}

std::string FAILOVER_HANDLER::host_to_IP(std::string host) {
    int status;
    struct addrinfo hints;
    struct addrinfo* servinfo;
    struct addrinfo* p;
    char ipstr[INET_ADDRSTRLEN];

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; //IPv4
    hints.ai_socktype = SOCK_STREAM;

    if ((status = getaddrinfo(host.c_str(), NULL, &hints, &servinfo)) != 0) {
        return "";
    }

    for (p = servinfo; p != NULL; p = p->ai_next) {
        void* addr;

        struct sockaddr_in* ipv4 = (struct sockaddr_in*)p->ai_addr;
        addr = &(ipv4->sin_addr);
        inet_ntop(p->ai_family, addr, ipstr, sizeof(ipstr));
    }

    freeaddrinfo(servinfo);
    return std::string(ipstr);
}

bool FAILOVER_HANDLER::is_failover_enabled() {
    return (dbc != nullptr && ds != nullptr &&
            ds->opt_ENABLE_CLUSTER_FAILOVER &&
            m_is_cluster_topology_available &&
            !m_is_rds_proxy);
}

bool FAILOVER_HANDLER::is_rds() { return m_is_rds; }

bool FAILOVER_HANDLER::is_rds_proxy() { return m_is_rds_proxy; }

bool FAILOVER_HANDLER::is_cluster_topology_available() {
    return m_is_cluster_topology_available;
}

void FAILOVER_HANDLER::initialize_topology() {
    
    current_topology = topology_service->get_topology(dbc->connection_proxy, false);
    if (current_topology) {
        m_is_cluster_topology_available = current_topology->total_hosts() > 0;
        MYLOG_DBC_TRACE(dbc,
                    "[FAILOVER_HANDLER] m_is_cluster_topology_available=%s",
                    m_is_cluster_topology_available ? "true" : "false");
        MYLOG_DBC_TRACE(dbc, topology_service->log_topology(current_topology).c_str());
        if (is_failover_enabled()) {
            this->dbc->env->failover_thread_pool.resize(current_topology->total_hosts());
        }
    }
}

SQLRETURN FAILOVER_HANDLER::reconnect(bool failover_enabled) {
    if (dbc->connection_proxy != nullptr && dbc->connection_proxy->is_connected()) {
        dbc->close();
    }
    return connection_handler->do_connect(dbc, ds, failover_enabled);
}

// return true if failover is triggered, false if not triggered
bool FAILOVER_HANDLER::trigger_failover_if_needed(const char* error_code,
                                                  const char*& new_error_code,
                                                  const char*& error_msg) {
    new_error_code = error_code;
    std::string ec(error_code ? error_code : "");

    if (!is_failover_enabled() || ec.empty()) {
        return false;
    }

    bool failover_success = false; // If failover happened & succeeded
    bool in_transaction = !autocommit_on(dbc) || dbc->transaction_open;

    if (ec.rfind("08", 0) == 0) {  // start with "08"

        // disable failure detection during failover
        auto failure_detection_old_state = ds->opt_ENABLE_FAILURE_DETECTION;
        ds->opt_ENABLE_FAILURE_DETECTION = false;

        // invalidate current connection
        current_host = nullptr;
        // close transaction if needed
        
        long long elasped_time_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - invoke_start_time_ms).count();
        metrics_container->register_failure_detection_time(elasped_time_ms);

        failover_start_time_ms = std::chrono::steady_clock::now();

        if (current_topology && current_topology->total_hosts() > 1 &&
            // Trigger reader failover if failover mode is not strict writer
            !is_failover_mode(FAILOVER_MODE_STRICT_WRITER, ds)) {

            failover_success = failover_to_reader(new_error_code, error_msg);
            elasped_time_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - failover_start_time_ms).count();
            metrics_container->register_reader_failover_procedure_time(elasped_time_ms);
        } else {
            failover_success = failover_to_writer(new_error_code, error_msg);
            elasped_time_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - failover_start_time_ms).count();
            metrics_container->register_writer_failover_procedure_time(elasped_time_ms);

        }
        metrics_container->register_failover_connects(failover_success);

        if (failover_success && in_transaction) {
            new_error_code = "08007";
            error_msg = "Connection failure during transaction.";
        }

        ds->opt_ENABLE_FAILURE_DETECTION = failure_detection_old_state;

        return true;
    }

    return false;
}

bool FAILOVER_HANDLER::failover_to_reader(const char*& new_error_code, const char*& error_msg) {
    MYLOG_DBC_TRACE(dbc, "[FAILOVER_HANDLER] Starting reader failover procedure with filtered topology: %s", this->topology_service->log_topology(this->topology_service->get_filtered_topology(current_topology)).c_str());
    auto result = failover_reader_handler->failover(this->topology_service->get_filtered_topology(current_topology));

    if (result->connected) {
        current_host = result->new_host;
        connection_handler->update_connection(result->new_connection, current_host->get_host());
        new_error_code = "08S02"; // Failover succeeded error code.
        error_msg = "The active SQL connection has changed.";
        MYLOG_DBC_TRACE(dbc,
                    "[FAILOVER_HANDLER] The active SQL connection has changed "
                    "due to a connection failure. Please re-configure session "
                    "state if required.");
        return true;
    } else {
        MYLOG_DBC_TRACE(dbc, "[FAILOVER_HANDLER] Unable to establish SQL connection to reader node.");
        new_error_code = "08S01"; // Failover failed error code.
        error_msg = "The active SQL connection was lost.";
        return false;
    }
    return false;
}

bool FAILOVER_HANDLER::failover_to_writer(const char*& new_error_code, const char*& error_msg) {
    MYLOG_DBC_TRACE(dbc, "[FAILOVER_HANDLER] Starting writer failover procedure.");
    auto result = failover_writer_handler->failover(current_topology);

    if (!result->connected) {
        MYLOG_DBC_TRACE(dbc, "[FAILOVER_HANDLER] Unable to establish SQL connection to writer node.");
        new_error_code = "08S01";
        error_msg = "The active SQL connection was lost.";
        return false;
    }

    const auto new_topology = result->new_topology;
    const auto new_host = new_topology->get_writer();

    if (result->is_new_host) {
        // connected to a new writer host; take it over
        current_topology = new_topology;
        current_host = new_host;
    }
    const auto filtered_topology = this->topology_service->get_filtered_topology(new_topology);
    const auto allowed_hosts = filtered_topology->get_instances();
    if (std::find(allowed_hosts.begin(), allowed_hosts.end(), new_host) == allowed_hosts.end()) {
        new_error_code = "08S01"; // Failover failed error code.
        error_msg = "The active SQL connection was lost.";
        MYLOG_DBC_TRACE(
            dbc,
            "[FAILOVER_HANDLER] The failover process identified the new writer but the host is not in the list of allowed hosts. "
            "New writer host: '%s'. Allowed hosts: '%s'",
            new_host->get_host().c_str(), 
            this->topology_service->log_topology(filtered_topology).c_str());
        return false;
    }

    connection_handler->update_connection(result->new_connection, new_host->get_host());
    
    new_error_code = "08S02"; // Failover succeeded error code.
    error_msg = "The active SQL connection has changed.";
    MYLOG_DBC_TRACE(
        dbc,
        "[FAILOVER_HANDLER] The active SQL connection has changed due to a "
        "connection failure. Please re-configure session state if required.");
    return true;
}

void FAILOVER_HANDLER::invoke_start_time() {
    invoke_start_time_ms = std::chrono::steady_clock::now();
}

bool FAILOVER_HANDLER::is_failover_mode(const char* expected_mode, DataSource* ds) {
    return myodbc_strcasecmp(expected_mode, (const char*) ds->opt_FAILOVER_MODE) == 0;
}
