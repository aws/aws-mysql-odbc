/*
 * AWS ODBC Driver for MySQL
 * Copyright Amazon.com Inc. or affiliates.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

/**
  @file  failover_handler.c
  @brief Failover functions.
*/

#include <regex>
#include <sstream>

#include "driver.h"

namespace {
const std::regex AURORA_DNS_PATTERN(
    R"#((.+)\.(proxy-|cluster-|cluster-ro-|cluster-custom-)?([a-zA-Z0-9]+\.[a-zA-Z0-9\-]+\.rds\.amazonaws\.com))#",
    std::regex_constants::icase);
const std::regex AURORA_PROXY_DNS_PATTERN(
    R"#((.+)\.(proxy-[a-zA-Z0-9]+\.[a-zA-Z0-9\-]+\.rds\.amazonaws\.com))#",
    std::regex_constants::icase);
const std::regex AURORA_CUSTOM_CLUSTER_PATTERN(
    R"#((.+)\.(cluster-custom-[a-zA-Z0-9]+\.[a-zA-Z0-9\-]+\.rds\.amazonaws\.com))#",
    std::regex_constants::icase);
const std::regex IPV4_PATTERN(
    R"#(^(([1-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])\.){1}(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])\.){2}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])$)#");
const std::regex IPV6_PATTERN(R"#(^[0-9a-fA-F]{1,4}(:[0-9a-fA-F]{1,4}){7}$)#");
const std::regex IPV6_COMPRESSED_PATTERN(
    R"#(^(([0-9A-Fa-f]{1,4}(:[0-9A-Fa-f]{1,4}){0,5})?)::(([0-9A-Fa-f]{1,4}(:[0-9A-Fa-f]{1,4}){0,5})?)$)#");
}  // namespace

FAILOVER_HANDLER::FAILOVER_HANDLER(DBC* dbc, DataSource* ds)
    : FAILOVER_HANDLER(dbc, ds, std::make_shared<FAILOVER_CONNECTION_HANDLER>(dbc),
                       std::make_shared<TOPOLOGY_SERVICE>()) {}

FAILOVER_HANDLER::FAILOVER_HANDLER(DBC* dbc, DataSource* ds,
                                   std::shared_ptr<FAILOVER_CONNECTION_HANDLER> connection_handler,
                                   std::shared_ptr<TOPOLOGY_SERVICE_INTERFACE> topology_service) {
    std::stringstream err;
    if (!dbc || !ds) {
        err << "Internal error.";
        throw err.str();
    }
    this->dbc = dbc;
    this->ds = ds;
    this->topology_service = topology_service;
    this->topology_service->set_refresh_rate(ds->topology_refresh_rate);
    this->connection_handler = connection_handler;

    this->failover_reader_handler = std::make_shared<FAILOVER_READER_HANDLER>(
        this->topology_service, this->connection_handler, ds->failover_timeout,
        ds->failover_reader_connect_timeout);
    this->failover_writer_handler = std::make_shared<FAILOVER_WRITER_HANDLER>(
        this->topology_service, this->failover_reader_handler,
        this->connection_handler, ds->failover_timeout,
        ds->failover_topology_refresh_rate,
        ds->failover_writer_reconnect_interval);
}

FAILOVER_HANDLER::~FAILOVER_HANDLER() {}

SQLRETURN FAILOVER_HANDLER::init_cluster_info() {
    SQLRETURN rc = SQL_ERROR;
    if (initialized) {
        return rc;
    }
    
    if (ds->disable_cluster_failover) {
        // Use a standard default connection - no further initialization required
        rc = connection_handler->do_connect(dbc, ds, false);
        initialized = true;
        return rc;
    }
    std::stringstream err;
    // Cluster-aware failover is enabled

    std::vector<Srv_host_detail> hosts;
    try {
        hosts = parse_host_list(
            ds_get_utf8attr(ds->server, &ds->server8), ds->port);
    } catch (std::string& e) {
        err << "Invalid server '" << ds->server8 << "'.";
        throw err.str();
    }

    if (hosts.size() == 0) {
        err << "Empty server host.";
        throw err.str();
    }

    std::string main_host(hosts[0].name);
    unsigned int main_port = hosts[0].port;

    this->current_host = std::make_shared<HOST_INFO>(main_host, main_port);

    const char* hp =
        ds_get_utf8attr(ds->host_pattern, &ds->host_pattern8);
    std::string hp_str(hp ? hp : "");

    const char* clid =
        ds_get_utf8attr(ds->cluster_id, &ds->cluster_id8);
    std::string clid_str(clid ? clid : "");

    if (!hp_str.empty()) {
        unsigned int port = ds->port ? ds->port : MYSQL_PORT;
        std::vector<Srv_host_detail> host_patterns;

        try {
            host_patterns = parse_host_list(hp_str.c_str(), port);
        } catch (std::string& e) {
            err << "Invalid host pattern '" << hp_str << "'.";
            throw err.str();
        }

        if (host_patterns.size() == 0) {
            err << "Empty host pattern.";
            throw err.str();
        }

        std::string host_pattern(host_patterns[0].name);
        unsigned int host_pattern_port = host_patterns[0].port;

        if (!is_dns_pattern_valid(host_pattern)) {
            err << "Invalid host pattern '" << hp_str << "'.";
            throw err.str();
        }

        auto host_template = std::make_shared<HOST_INFO>(host_pattern, host_pattern_port);
        topology_service->set_cluster_instance_template(host_template);

        m_is_rds = is_rds_dns(host_pattern);
        m_is_rds_proxy = is_rds_proxy_dns(host_pattern);
        m_is_rds_custom_cluster = is_rds_custom_cluster_dns(host_pattern);

        if (m_is_rds_proxy) {
            err << "RDS Proxy url can't be used as an instance pattern.";
            throw err.str();
        }

        if (m_is_rds_custom_cluster) {
            err << "RDS Custom Cluster endpoint can't be used as an instance "
                   "pattern.";
            throw err.str();
        }

        if (!clid_str.empty()) {
            topology_service->set_cluster_id(clid_str);

        } else if (m_is_rds) {
            // If it's a cluster endpoint, or a reader cluster endpoint, then
            // let's use as cluster identification
            std::string cluster_rds_host =
                get_rds_cluster_host_url(host_pattern);
            if (!cluster_rds_host.empty()) {
                clid_str.assign(cluster_rds_host);
                clid_str.append(":");
                clid_str.append(std::to_string(host_pattern_port));
                topology_service->set_cluster_id(clid_str);
            }
        }

        rc = create_connection_and_initialize_topology();
    } else if (is_ipv4(main_host) || is_ipv6(main_host)) {
        // TODO: do we need to setup host template in this case?
        // HOST_INFO* host_template = new HOST_INFO();
        // host_template->host.assign(main_host);
        // host_template->port = main_port;
        // ts->setClusterInstanceTemplate(host_template);

        if (!clid_str.empty()) {
            topology_service->set_cluster_id(clid_str);
        }

        rc = create_connection_and_initialize_topology();

        if (m_is_cluster_topology_available) {
            err << "Host Pattern configuration setting is required when IP "
                   "address is used to connect to cluster.";
            throw err.str();
        }

        m_is_rds = false;        // actually we don't know
        m_is_rds_proxy = false;  // actually we don't know

    } else {
        m_is_rds = is_rds_dns(main_host);
        m_is_rds_proxy = is_rds_proxy_dns(main_host);

        if (!m_is_rds) {
            // it's not RDS, maybe custom domain (CNAME)
            auto host_template =
                std::make_shared<HOST_INFO>(main_host, main_port);
            topology_service->set_cluster_instance_template(host_template);

            if (!clid_str.empty()) {
                topology_service->set_cluster_id(clid_str);
            }

            rc = create_connection_and_initialize_topology();

            if (m_is_cluster_topology_available) {
                err << "The provided host appears to be a custom domain. The "
                       "driver requires the Host Pattern configuration setting "
                       "to be set for custom domains.";
                throw err.str();
            }
        } else {
            // It's RDS

            std::string rds_instance_host =
                get_rds_instance_host_pattern(main_host);
            if (!rds_instance_host.empty()) {
                topology_service->set_cluster_instance_template(
                    std::make_shared<HOST_INFO>(rds_instance_host, main_port));
            } else {
                err << "The provided host does not appear to match an expected "
                       "Aurora DNS pattern. Please set the Host Pattern "
                       "configuration to specify the host pattern for the "
                       "cluster you are trying to connect to.";
                throw err.str();
            }

            if (!clid_str.empty()) {
                topology_service->set_cluster_id(clid_str);
            } else if (m_is_rds_proxy) {
                // Each proxy is associated with a single cluster so it's safe
                // to use RDS Proxy Url as cluster identification
                clid_str.assign(main_host);
                clid_str.append(":");
                clid_str.append(std::to_string(main_port));
                topology_service->set_cluster_id(clid_str);
            } else {
                // If it's cluster endpoint or reader cluster endpoint,
                // then let's use as cluster identification

                std::string cluster_rds_host =
                    get_rds_cluster_host_url(main_host);
                if (!cluster_rds_host.empty()) {
                    clid_str.assign(cluster_rds_host);
                    clid_str.append(":");
                    clid_str.append(std::to_string(main_port));
                    topology_service->set_cluster_id(clid_str);
                }
            }

            rc = create_connection_and_initialize_topology();
        }
    }

    initialized = true;
    return rc;
}

bool FAILOVER_HANDLER::is_dns_pattern_valid(std::string host) {
    return (host.find("?") != std::string::npos);
}

bool FAILOVER_HANDLER::is_rds_dns(std::string host) {
    return std::regex_match(host, AURORA_DNS_PATTERN);
}

bool FAILOVER_HANDLER::is_rds_proxy_dns(std::string host) {
    return std::regex_match(host, AURORA_PROXY_DNS_PATTERN);
}

bool FAILOVER_HANDLER::is_rds_custom_cluster_dns(std::string host) {
    return std::regex_match(host, AURORA_CUSTOM_CLUSTER_PATTERN);
}

#if defined(__APPLE__) || defined(__linux__)
    #define strcmp_case_insensitive(str1, str2) strcasecmp(str1, str2)
#else
    #define strcmp_case_insensitive(str1, str2) strcmpi(str1, str2)
#endif

std::string FAILOVER_HANDLER::get_rds_cluster_host_url(std::string host) {
    std::smatch m;
    if (std::regex_search(host, m, AURORA_DNS_PATTERN) && m.size() > 1) {
        std::string gr1 = m.size() > 1 ? m.str(1) : std::string("");
        std::string gr2 = m.size() > 2 ? m.str(2) : std::string("");
        std::string gr3 = m.size() > 3 ? m.str(3) : std::string("");
        if (!gr1.empty() && !gr3.empty() &&
            (strcmp_case_insensitive(gr2.c_str(), "cluster-") == 0 || strcmp_case_insensitive(gr2.c_str(), "cluster-ro-") == 0)) {
            std::string result;
            result.assign(gr1);
            result.append(".cluster-");
            result.append(gr3);

            return result;
        }
    }
    return "";
}

std::string FAILOVER_HANDLER::get_rds_instance_host_pattern(std::string host) {
    std::smatch m;
    if (std::regex_search(host, m, AURORA_DNS_PATTERN) && m.size() > 3) {
        if (!m.str(3).empty()) {
            std::string result("?.");
            result.append(m.str(3));

            return result;
        }
    }
    return "";
}

bool FAILOVER_HANDLER::is_failover_enabled() {
    return (dbc != nullptr && ds != nullptr &&
            !ds->disable_cluster_failover &&
            m_is_cluster_topology_available &&
            !m_is_rds_proxy &&
            !m_is_multi_writer_cluster);
}

bool FAILOVER_HANDLER::is_rds() { return m_is_rds; }

bool FAILOVER_HANDLER::is_rds_proxy() { return m_is_rds_proxy; }

bool FAILOVER_HANDLER::is_cluster_topology_available() {
    return m_is_cluster_topology_available;
}

SQLRETURN FAILOVER_HANDLER::create_connection_and_initialize_topology() {
    SQLRETURN rc = connection_handler->do_connect(dbc, ds, false);
    if (!SQL_SUCCEEDED(rc)) {
        return rc;
    }

    current_topology = topology_service->get_topology(dbc->mysql, false);
    if (current_topology) {
        m_is_multi_writer_cluster = current_topology->is_multi_writer_cluster;
        m_is_cluster_topology_available = current_topology->total_hosts() > 0;

        // Since we can't determine whether failover should be enabled
        // before we connect, there is a possibility we need to reconnect
        // again with the correct connection settings for failover.
        if (is_failover_enabled() && (ds->connect_timeout != dbc->login_timeout ||
                                      ds->network_timeout != ds->read_timeout ||
                                      ds->network_timeout != ds->write_timeout)) {

            rc = reconnect(true);
        }
    }

    return rc;
}

SQLRETURN FAILOVER_HANDLER::reconnect(bool failover_enabled) {
    if (dbc->mysql != nullptr && dbc->mysql->is_connected()) {
        dbc->close();
    }
    return connection_handler->do_connect(dbc, ds, failover_enabled);
}

bool FAILOVER_HANDLER::is_ipv4(std::string host) {
    return std::regex_match(host, IPV4_PATTERN);
}

bool FAILOVER_HANDLER::is_ipv6(std::string host) {
    return std::regex_match(host, IPV6_PATTERN) ||
           std::regex_match(host, IPV6_COMPRESSED_PATTERN);
}

bool FAILOVER_HANDLER::trigger_failover_if_needed(const char* error_code, const char*& new_error_code) {
    new_error_code = error_code;
    std::string ec(error_code ? error_code : "");

    if (!is_failover_enabled() || ec.empty()) {
        return false;
    }

    if (ec.rfind("08", 0) == 0) {  // start with "08"

        // invalidate current connection
        current_host = nullptr;
        // close transaction if needed

        if (current_topology && current_topology->total_hosts() > 1 &&
            ds->allow_reader_connections) {  // there're readers in topology
            return failover_to_reader(new_error_code);
        } else {
            return failover_to_writer(new_error_code);
        }
    }

    return false;
}

bool FAILOVER_HANDLER::failover_to_reader(const char*& new_error_code) {
    auto result = failover_reader_handler->failover(current_topology);

    if (result.connected) {
        current_host = result.new_host;
        connection_handler->update_connection(result.new_connection);
        new_error_code = "08S02";
        return true;
    } else {
        // "Unable to establish SQL connection to reader node"
        new_error_code = "08S01";
        return false;
    }
    return false;
}

bool FAILOVER_HANDLER::failover_to_writer(const char*& new_error_code) {
    auto result = failover_writer_handler->failover(current_topology);

    if (!result.connected) {
        // "Unable to establish SQL connection to writer node"
        new_error_code = "08S01";
        return false;
    }
    if (result.is_new_host) {
        // connected to a new writer host; take it over
        current_topology = result.new_topology;
        current_host = current_topology->get_writer();
    }
    connection_handler->update_connection(result.new_connection);
    new_error_code = "08S02";
    return true;
}
