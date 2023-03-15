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

#include "driver.h"

namespace {
    const char* RETRIEVE_HOST_PORT_SQL = "SELECT CONCAT(@@hostname, ':', @@port)";
}

EFM_PROXY::EFM_PROXY(DBC* dbc, DataSource* ds) : EFM_PROXY(
    dbc, ds, nullptr, std::make_shared<MONITOR_SERVICE>(ds && ds->save_queries)) {}

EFM_PROXY::EFM_PROXY(DBC* dbc, DataSource* ds, MYSQL_PROXY* next_proxy) : EFM_PROXY(
    dbc, ds, next_proxy, std::make_shared<MONITOR_SERVICE>(ds && ds->save_queries)) {}

EFM_PROXY::EFM_PROXY(DBC* dbc, DataSource* ds, MYSQL_PROXY* next_proxy,
                     std::shared_ptr<MONITOR_SERVICE> monitor_service)
    : MYSQL_PROXY(dbc, ds),
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

void EFM_PROXY::set_next_proxy(MYSQL_PROXY* next_proxy) {
    MYSQL_PROXY::set_next_proxy(next_proxy);
    generate_node_keys();
}

void EFM_PROXY::set_connection(MYSQL_PROXY* mysql_proxy) {
    next_proxy->set_connection(mysql_proxy);

    if (monitor_service != nullptr && !node_keys.empty()) {
        monitor_service->stop_monitoring_for_all_connections(node_keys);
    }

    generate_node_keys();
}

void EFM_PROXY::close_socket() {
    next_proxy->close_socket();
}

void EFM_PROXY::delete_ds() {
    next_proxy->delete_ds();
}

uint64_t EFM_PROXY::num_rows(MYSQL_RES* res) {
    return next_proxy->num_rows(res);
}

unsigned EFM_PROXY::num_fields(MYSQL_RES* res) {
    return next_proxy->num_fields(res);
}

MYSQL_FIELD* EFM_PROXY::fetch_field_direct(MYSQL_RES* res, unsigned fieldnr) {
    return next_proxy->fetch_field_direct(res, fieldnr);
}

MYSQL_ROW_OFFSET EFM_PROXY::row_tell(MYSQL_RES* res) {
    return next_proxy->row_tell(res);
}

unsigned EFM_PROXY::field_count() {
    return next_proxy->field_count();
}

uint64_t EFM_PROXY::affected_rows() {
    return next_proxy->affected_rows();
}

unsigned EFM_PROXY::error_code() {
    return next_proxy->error_code();
}

const char* EFM_PROXY::error() {
    return next_proxy->error();
}

const char* EFM_PROXY::sqlstate() {
    return next_proxy->sqlstate();
}

unsigned long EFM_PROXY::thread_id() {
    return next_proxy->thread_id();
}

int EFM_PROXY::set_character_set(const char* csname) {
    const auto context = start_monitoring();
    const int ret = next_proxy->set_character_set(csname);
    stop_monitoring(context);
    return ret;
}

void EFM_PROXY::init() {
    next_proxy->init();
}

bool EFM_PROXY::ssl_set(const char* key, const char* cert, const char* ca, const char* capath, const char* cipher) {
    return next_proxy->ssl_set(key, cert, ca, capath, cipher);
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

CHARSET_INFO* EFM_PROXY::get_character_set() const {
    return next_proxy->get_character_set();
}

void EFM_PROXY::get_character_set_info(MY_CHARSET_INFO* charset) {
    next_proxy->get_character_set_info(charset);
}

bool EFM_PROXY::autocommit(bool auto_mode) {
    return next_proxy->autocommit(auto_mode);
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

void EFM_PROXY::close() {
    next_proxy->close();
}

bool EFM_PROXY::real_connect_dns_srv(
    const char* dns_srv_name, const char* user,
    const char* passwd, const char* db, unsigned long client_flag) {

    const bool ret = next_proxy->real_connect_dns_srv(dns_srv_name, user, passwd, db, client_flag);

    generate_node_keys();

    return ret;
}

int EFM_PROXY::ping() {
    return next_proxy->ping();
}

int EFM_PROXY::options4(mysql_option option, const void* arg1, const void* arg2) {
    return next_proxy->options4(option, arg1, arg2);
}

int EFM_PROXY::get_option(mysql_option option, const void* arg) {
    return next_proxy->get_option(option, arg);
}

int EFM_PROXY::options(mysql_option option, const void* arg) {
    return next_proxy->options(option, arg);
}

void EFM_PROXY::free_result(MYSQL_RES* result) {
    const auto context = start_monitoring();
    next_proxy->free_result(result);
    stop_monitoring(context);
}

void EFM_PROXY::data_seek(MYSQL_RES* result, uint64_t offset) {
    next_proxy->data_seek(result, offset);
}

MYSQL_ROW_OFFSET EFM_PROXY::row_seek(MYSQL_RES* result, MYSQL_ROW_OFFSET offset) {
    return next_proxy->row_seek(result, offset);
}

MYSQL_FIELD_OFFSET EFM_PROXY::field_seek(MYSQL_RES* result, MYSQL_FIELD_OFFSET offset) {
    return next_proxy->field_seek(result, offset);
}

MYSQL_ROW EFM_PROXY::fetch_row(MYSQL_RES* result) {
    const auto context = start_monitoring();
    const MYSQL_ROW ret = next_proxy->fetch_row(result);
    stop_monitoring(context);
    return ret;
}

unsigned long* EFM_PROXY::fetch_lengths(MYSQL_RES* result) {
    return next_proxy->fetch_lengths(result);
}

MYSQL_FIELD* EFM_PROXY::fetch_field(MYSQL_RES* result) {
    return next_proxy->fetch_field(result);
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

unsigned long EFM_PROXY::stmt_param_count(MYSQL_STMT* stmt) {
    return next_proxy->stmt_param_count(stmt);
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

unsigned EFM_PROXY::stmt_errno(MYSQL_STMT* stmt) {
    return next_proxy->stmt_errno(stmt);
}

const char* EFM_PROXY::stmt_error(MYSQL_STMT* stmt) {
    return next_proxy->stmt_error(stmt);
}

MYSQL_ROW_OFFSET EFM_PROXY::stmt_row_seek(MYSQL_STMT* stmt, MYSQL_ROW_OFFSET offset) {
    return next_proxy->stmt_row_seek(stmt, offset);
}

MYSQL_ROW_OFFSET EFM_PROXY::stmt_row_tell(MYSQL_STMT* stmt) {
    return next_proxy->stmt_row_tell(stmt);
}

void EFM_PROXY::stmt_data_seek(MYSQL_STMT* stmt, uint64_t offset) {
    next_proxy->stmt_data_seek(stmt, offset);
}

uint64_t EFM_PROXY::stmt_num_rows(MYSQL_STMT* stmt) {
    return next_proxy->stmt_num_rows(stmt);
}

uint64_t EFM_PROXY::stmt_affected_rows(MYSQL_STMT* stmt) {
    return next_proxy->stmt_affected_rows(stmt);
}

unsigned EFM_PROXY::stmt_field_count(MYSQL_STMT* stmt) {
    return next_proxy->stmt_field_count(stmt);
}

st_mysql_client_plugin* EFM_PROXY::client_find_plugin(const char* name, int type) {
    return next_proxy->client_find_plugin(name, type);
}

bool EFM_PROXY::is_connected() {
    return next_proxy->is_connected();
}

void EFM_PROXY::set_last_error_code(unsigned error_code) {
    next_proxy->set_last_error_code(error_code);
}

char* EFM_PROXY::get_last_error() const {
    return next_proxy->get_last_error();
}

unsigned EFM_PROXY::get_last_error_code() const {
    return next_proxy->get_last_error_code();
}

char* EFM_PROXY::get_sqlstate() const {
    return next_proxy->get_sqlstate();
}

char* EFM_PROXY::get_server_version() const {
    return next_proxy->get_server_version();
}

uint64_t EFM_PROXY::get_affected_rows() const {
    return next_proxy->get_affected_rows();
}

void EFM_PROXY::set_affected_rows(uint64_t num_rows) {
    next_proxy->set_affected_rows(num_rows);
}

char* EFM_PROXY::get_host_info() const {
    return next_proxy->get_host_info();
}

std::string EFM_PROXY::get_host() {
    return next_proxy->get_host();
}

unsigned EFM_PROXY::get_port() {
    return next_proxy->get_port();
}

unsigned long EFM_PROXY::get_max_packet() const {
    return next_proxy->get_max_packet();
}

unsigned long EFM_PROXY::get_server_capabilities() const {
    return next_proxy->get_server_capabilities();
}

unsigned EFM_PROXY::get_server_status() const {
    return next_proxy->get_server_status();
}
