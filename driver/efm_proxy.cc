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

#include "efm_proxy.h"

namespace {
    const char* RETRIEVE_HOST_PORT_SQL = "SELECT CONCAT(@@hostname, ':', @@port)";
}

EFM_PROXY::EFM_PROXY(DBC* dbc, DataSource* ds) : EFM_PROXY(
    dbc, ds, nullptr, std::make_shared<MONITOR_SERVICE>(ds && ds->save_queries)) {}

EFM_PROXY::EFM_PROXY(DBC* dbc, DataSource* ds, CONNECTION_PROXY* next_proxy) : EFM_PROXY(
    dbc, ds, next_proxy, std::make_shared<MONITOR_SERVICE>(ds && ds->save_queries)) {}

EFM_PROXY::EFM_PROXY(DBC* dbc, DataSource* ds, CONNECTION_PROXY* next_proxy,
                     std::shared_ptr<MONITOR_SERVICE> monitor_service)
    : CONNECTION_PROXY(dbc, ds),
      monitor_service{std::move(monitor_service)} {

    this->next_proxy = next_proxy;
}

std::shared_ptr<MONITOR_CONNECTION_CONTEXT> EFM_PROXY::start_monitoring() {
    if (!ds || !ds->enable_failure_detection) {
        return nullptr;
    }

    auto failure_detection_timeout = ds->failure_detection_timeout;
    // Use network timeout defined if failure detection timeout is not set
    if (failure_detection_timeout == 0) {
        failure_detection_timeout = ds->network_timeout == 0 ? failure_detection_timeout_default : ds->network_timeout;
    }

    return monitor_service->start_monitoring(
        dbc,
        ds,
        node_keys,
        std::make_shared<HOST_INFO>(get_host(), get_port()),
        std::chrono::milliseconds{ds->failure_detection_time},
        std::chrono::seconds{failure_detection_timeout},
        std::chrono::milliseconds{ds->failure_detection_interval},
        ds->failure_detection_count,
        std::chrono::milliseconds{ds->monitor_disposal_time});
}

void EFM_PROXY::stop_monitoring(std::shared_ptr<MONITOR_CONNECTION_CONTEXT> context) {
    if (!ds || !ds->enable_failure_detection || context == nullptr) {
        return;
    }
    monitor_service->stop_monitoring(context);
    if (context->is_node_unhealthy() && is_connected()) {
        close_socket();
    }
}

void EFM_PROXY::generate_node_keys() {
    node_keys.clear();
    node_keys.insert(std::string(get_host()) + ":" + std::to_string(get_port()));

    if (is_connected()) {
        // Temporarily turn off failure detection if on
        const auto failure_detection_old_state = ds->enable_failure_detection;
        ds->enable_failure_detection = false;

        const auto error = query(RETRIEVE_HOST_PORT_SQL);
        if (error == 0) {
            MYSQL_RES* result = store_result();
            MYSQL_ROW row;
            while ((row = fetch_row(result))) {
                node_keys.insert(std::string(row[0]));
            }
            free_result(result);
        }

        ds->enable_failure_detection = failure_detection_old_state;
    }
}

void EFM_PROXY::set_next_proxy(CONNECTION_PROXY* next_proxy) {
    CONNECTION_PROXY::set_next_proxy(next_proxy);
    generate_node_keys();
}

void EFM_PROXY::set_connection(CONNECTION_PROXY* connection_proxy) {
    CONNECTION_PROXY::set_connection(connection_proxy);

    if (monitor_service != nullptr && !node_keys.empty()) {
        monitor_service->stop_monitoring_for_all_connections(node_keys);
    }

    generate_node_keys();
}

int EFM_PROXY::set_character_set(const char* csname) {
    const auto context = start_monitoring();
    const int ret = next_proxy->set_character_set(csname);
    stop_monitoring(context);
    return ret;
}

bool EFM_PROXY::change_user(const char* user, const char* passwd, const char* db) {
    const auto context = start_monitoring();
    const bool ret = next_proxy->change_user(user, passwd, db);
    stop_monitoring(context);
    return ret;
}

bool EFM_PROXY::real_connect(
    const char* host, const char* user, const char* passwd,
    const char* db, unsigned int port, const char* unix_socket,
    unsigned long clientflag) {

    const bool ret = next_proxy->real_connect(host, user, passwd, db, port, unix_socket, clientflag);

    generate_node_keys();

    return ret;
}

int EFM_PROXY::select_db(const char* db) {
    const auto context = start_monitoring();
    const int ret = next_proxy->select_db(db);
    stop_monitoring(context);
    return ret;
}

int EFM_PROXY::query(const char* q) {
    const auto context = start_monitoring();
    const int ret = next_proxy->query(q);
    stop_monitoring(context);
    return ret;
}

int EFM_PROXY::real_query(const char* q, unsigned long length) {
    const auto context = start_monitoring();
    const int ret = next_proxy->real_query(q, length);
    stop_monitoring(context);
    return ret;
}

MYSQL_RES* EFM_PROXY::store_result() {
    const auto context = start_monitoring();
    MYSQL_RES* ret = next_proxy->store_result();
    stop_monitoring(context);
    return ret;
}

MYSQL_RES* EFM_PROXY::use_result() {
    const auto context = start_monitoring();
    MYSQL_RES* ret = next_proxy->use_result();
    stop_monitoring(context);
    return ret;
}

int EFM_PROXY::next_result() {
    const auto context = start_monitoring();
    const int ret = next_proxy->next_result();
    stop_monitoring(context);
    return ret;
}

int EFM_PROXY::stmt_next_result(MYSQL_STMT* stmt) {
    const auto context = start_monitoring();
    const int ret = next_proxy->stmt_next_result(stmt);
    stop_monitoring(context);
    return ret;
}

bool EFM_PROXY::real_connect_dns_srv(
    const char* dns_srv_name, const char* user,
    const char* passwd, const char* db, unsigned long client_flag) {

    const bool ret = next_proxy->real_connect_dns_srv(dns_srv_name, user, passwd, db, client_flag);

    generate_node_keys();

    return ret;
}

void EFM_PROXY::free_result(MYSQL_RES* result) {
    const auto context = start_monitoring();
    next_proxy->free_result(result);
    stop_monitoring(context);
}

MYSQL_ROW EFM_PROXY::fetch_row(MYSQL_RES* result) {
    const auto context = start_monitoring();
    const MYSQL_ROW ret = next_proxy->fetch_row(result);
    stop_monitoring(context);
    return ret;
}

MYSQL_RES* EFM_PROXY::list_fields(const char* table, const char* wild) {
    const auto context = start_monitoring();
    MYSQL_RES* ret = next_proxy->list_fields(table, wild);
    stop_monitoring(context);
    return ret;
}

unsigned long EFM_PROXY::real_escape_string(char* to, const char* from, unsigned long length) {
    const auto context = start_monitoring();
    const unsigned long ret = next_proxy->real_escape_string(to, from, length);
    stop_monitoring(context);
    return ret;
}

bool EFM_PROXY::bind_param(unsigned n_params, MYSQL_BIND* binds, const char** names) {
    const auto context = start_monitoring();
    const bool ret = next_proxy->bind_param(n_params, binds, names);
    stop_monitoring(context);
    return ret;
}

MYSQL_STMT* EFM_PROXY::stmt_init() {
    const auto context = start_monitoring();
    MYSQL_STMT* ret = next_proxy->stmt_init();
    stop_monitoring(context);
    return ret;
}

int EFM_PROXY::stmt_prepare(MYSQL_STMT* stmt, const char* query, unsigned long length) {
    const auto context = start_monitoring();
    const int ret = next_proxy->stmt_prepare(stmt, query, length);
    stop_monitoring(context);
    return ret;
}

int EFM_PROXY::stmt_execute(MYSQL_STMT* stmt) {
    const auto context = start_monitoring();
    const int ret = next_proxy->stmt_execute(stmt);
    stop_monitoring(context);
    return ret;
}

int EFM_PROXY::stmt_fetch(MYSQL_STMT* stmt) {
    const auto context = start_monitoring();
    const int ret = next_proxy->stmt_fetch(stmt);
    stop_monitoring(context);
    return ret;
}

int EFM_PROXY::stmt_fetch_column(MYSQL_STMT* stmt, MYSQL_BIND* bind_arg, unsigned int column, unsigned long offset) {
    const auto context = start_monitoring();
    const int ret = next_proxy->stmt_fetch_column(stmt, bind_arg, column, offset);
    stop_monitoring(context);
    return ret;
}

int EFM_PROXY::stmt_store_result(MYSQL_STMT* stmt) {
    const auto context = start_monitoring();
    const int ret = next_proxy->stmt_store_result(stmt);
    stop_monitoring(context);
    return ret;
}

bool EFM_PROXY::stmt_bind_param(MYSQL_STMT* stmt, MYSQL_BIND* bnd) {
    const auto context = start_monitoring();
    const bool ret = next_proxy->stmt_bind_param(stmt, bnd);
    stop_monitoring(context);
    return ret;
}

bool EFM_PROXY::stmt_bind_result(MYSQL_STMT* stmt, MYSQL_BIND* bnd) {
    const auto context = start_monitoring();
    const bool ret = next_proxy->stmt_bind_result(stmt, bnd);
    stop_monitoring(context);
    return ret;
}

bool EFM_PROXY::stmt_close(MYSQL_STMT* stmt) {
    const auto context = start_monitoring();
    const bool ret = next_proxy->stmt_close(stmt);
    stop_monitoring(context);
    return ret;
}

bool EFM_PROXY::stmt_reset(MYSQL_STMT* stmt) {
    const auto context = start_monitoring();
    const bool ret = next_proxy->stmt_reset(stmt);
    stop_monitoring(context);
    return ret;
}

bool EFM_PROXY::stmt_free_result(MYSQL_STMT* stmt) {
    const auto context = start_monitoring();
    const bool ret = next_proxy->stmt_free_result(stmt);
    stop_monitoring(context);
    return ret;
}

bool EFM_PROXY::stmt_send_long_data(MYSQL_STMT* stmt, unsigned int param_number, const char* data,
                                    unsigned long length) {
    const auto context = start_monitoring();
    const bool ret = next_proxy->stmt_send_long_data(stmt, param_number, data, length);
    stop_monitoring(context);
    return ret;
}

MYSQL_RES* EFM_PROXY::stmt_result_metadata(MYSQL_STMT* stmt) {
    const auto context = start_monitoring();
    MYSQL_RES* ret = next_proxy->stmt_result_metadata(stmt);
    stop_monitoring(context);
    return ret;
}
