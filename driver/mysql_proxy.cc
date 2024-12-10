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

#include "mysql_proxy.h"

#include <sstream>
#include <thread>

namespace {
    const auto SOCKET_CLOSE_DELAY = std::chrono::milliseconds(100);
}

MYSQL_PROXY::MYSQL_PROXY(DBC* dbc, DataSource* ds) : CONNECTION_PROXY(dbc, ds) {
    this->host = get_host_info_from_ds(ds);
}

MYSQL_PROXY::~MYSQL_PROXY() {
    if (this->mysql) {
        close();
    }
}

uint64_t MYSQL_PROXY::num_rows(MYSQL_RES* res) {
    return mysql_num_rows(res);
}

unsigned int MYSQL_PROXY::num_fields(MYSQL_RES* res) {
    return mysql_num_fields(res);
}

MYSQL_FIELD* MYSQL_PROXY::fetch_field_direct(MYSQL_RES* res, unsigned int fieldnr) {
    return mysql_fetch_field_direct(res, fieldnr);
}

MYSQL_ROW_OFFSET MYSQL_PROXY::row_tell(MYSQL_RES* res) {
    return mysql_row_tell(res);
}

unsigned int MYSQL_PROXY::field_count() {
    return mysql_field_count(mysql);
}

uint64_t MYSQL_PROXY::affected_rows() {
    return mysql_affected_rows(mysql);
}

unsigned int MYSQL_PROXY::error_code() {
    return mysql_errno(mysql);
}

const char* MYSQL_PROXY::error() {
    return mysql_error(mysql);
}

const char* MYSQL_PROXY::sqlstate() {
    return mysql_sqlstate(mysql);
}

unsigned long MYSQL_PROXY::thread_id() {
    return mysql_thread_id(mysql);
}

int MYSQL_PROXY::set_character_set(const char* csname) {
    return mysql_set_character_set(mysql, csname);
}

void MYSQL_PROXY::init() {
    this->mysql = mysql_init(nullptr);
}

bool MYSQL_PROXY::change_user(const char* user, const char* passwd, const char* db) {
    return mysql_change_user(mysql, user, passwd, db);
}

bool MYSQL_PROXY::real_connect(
    const char* host, const char* user, const char* passwd,
    const char* db, unsigned int port, const char* unix_socket,
    unsigned long clientflag) {

    this->explicitly_closed = false;
    
    const MYSQL* new_mysql = mysql_real_connect(mysql, host, user, passwd, db, port, unix_socket, clientflag);
    return new_mysql != nullptr;
}

int MYSQL_PROXY::select_db(const char* db) {
    return mysql_select_db(mysql, db);
}

int MYSQL_PROXY::query(const char* q) {
    return mysql_query(mysql, q);
}

int MYSQL_PROXY::real_query(const char* q, unsigned long length) {
    return mysql_real_query(mysql, q, length);
}

MYSQL_RES* MYSQL_PROXY::store_result() {
    return mysql_store_result(mysql);
}

MYSQL_RES* MYSQL_PROXY::use_result() {
    return mysql_use_result(mysql);
}

struct CHARSET_INFO* MYSQL_PROXY::get_character_set() const {
    return this->mysql->charset;
}

void MYSQL_PROXY::get_character_set_info(MY_CHARSET_INFO* charset) {
    mysql_get_character_set_info(mysql, charset);
}

bool MYSQL_PROXY::autocommit(bool auto_mode) {
    return mysql_autocommit(mysql, auto_mode);
}

bool MYSQL_PROXY::commit() {
    return mysql_commit(mysql);
}

bool MYSQL_PROXY::rollback() {
    return mysql_rollback(mysql);
}

bool MYSQL_PROXY::more_results() {
    return mysql_more_results(mysql);
}

int MYSQL_PROXY::next_result() {
    return mysql_next_result(mysql);
}

int MYSQL_PROXY::stmt_next_result(MYSQL_STMT* stmt) {
    return mysql_stmt_next_result(stmt);
}

void MYSQL_PROXY::close() {
    mysql_close(mysql);
    mysql = nullptr;
    this->explicitly_closed = true;
}

bool MYSQL_PROXY::real_connect_dns_srv(
    const char* dns_srv_name, const char* user,
    const char* passwd, const char* db, unsigned long client_flag) {

    const MYSQL* new_mysql = mysql_real_connect_dns_srv(mysql, dns_srv_name, user, passwd, db, client_flag);
    return new_mysql != nullptr;
}

int MYSQL_PROXY::ping() {
    return mysql_ping(mysql);
}

int MYSQL_PROXY::get_option(mysql_option option, const void* arg) {
    return mysql_get_option(mysql, option, arg);
}

int MYSQL_PROXY::options4(mysql_option option, const void* arg1, const void* arg2) {
    return mysql_options4(mysql, option, arg1, arg2);
}

int MYSQL_PROXY::options(mysql_option option, const void* arg) {
    return mysql_options(mysql, option, arg);
}

void MYSQL_PROXY::free_result(MYSQL_RES* result) {
    mysql_free_result(result);
}

void MYSQL_PROXY::data_seek(MYSQL_RES* result, uint64_t offset) {
    mysql_data_seek(result, offset);
}

MYSQL_ROW_OFFSET MYSQL_PROXY::row_seek(MYSQL_RES* result, MYSQL_ROW_OFFSET offset) {
    return mysql_row_seek(result, offset);
}

MYSQL_FIELD_OFFSET MYSQL_PROXY::field_seek(MYSQL_RES* result, MYSQL_FIELD_OFFSET offset) {
    return mysql_field_seek(result, offset);
}

MYSQL_ROW MYSQL_PROXY::fetch_row(MYSQL_RES* result) {
    return mysql_fetch_row(result);
}

unsigned long* MYSQL_PROXY::fetch_lengths(MYSQL_RES* result) {
    return mysql_fetch_lengths(result);
}

MYSQL_FIELD* MYSQL_PROXY::fetch_field(MYSQL_RES* result) {
    return mysql_fetch_field(result);
}

unsigned long MYSQL_PROXY::real_escape_string(char* to, const char* from, unsigned long length) {
    return mysql_real_escape_string(mysql, to, from, length);
}

bool MYSQL_PROXY::bind_param(unsigned n_params, MYSQL_BIND* binds, const char** names) {
    return mysql_bind_param(mysql, n_params, binds, names);
}

MYSQL_STMT* MYSQL_PROXY::stmt_init() {
    return mysql_stmt_init(mysql);
}

int MYSQL_PROXY::stmt_prepare(MYSQL_STMT* stmt, const char* query, unsigned long length) {
    return mysql_stmt_prepare(stmt, query, length);
}

int MYSQL_PROXY::stmt_execute(MYSQL_STMT* stmt) {
    return mysql_stmt_execute(stmt);
}

int MYSQL_PROXY::stmt_fetch(MYSQL_STMT* stmt) {
    return mysql_stmt_fetch(stmt);
}

int MYSQL_PROXY::stmt_fetch_column(MYSQL_STMT* stmt, MYSQL_BIND* bind_arg, unsigned int column, unsigned long offset) {
    return mysql_stmt_fetch_column(stmt, bind_arg, column, offset);
}

int MYSQL_PROXY::stmt_store_result(MYSQL_STMT* stmt) {
    return mysql_stmt_store_result(stmt);
}

unsigned long MYSQL_PROXY::stmt_param_count(MYSQL_STMT* stmt) {
    return mysql_stmt_param_count(stmt);
}

bool MYSQL_PROXY::stmt_bind_param(MYSQL_STMT* stmt, MYSQL_BIND* bnd) {
  return mysql_stmt_bind_param(stmt, bnd);
}

bool MYSQL_PROXY::stmt_bind_named_param(MYSQL_STMT *stmt, MYSQL_BIND *binds,
                                        unsigned n_params, const char **names) {
  return mysql_stmt_bind_named_param(stmt, binds, n_params, names);
}

bool MYSQL_PROXY::stmt_bind_result(MYSQL_STMT* stmt, MYSQL_BIND* bnd) {
    return mysql_stmt_bind_result(stmt, bnd);
}

bool MYSQL_PROXY::stmt_close(MYSQL_STMT* stmt) {
    return mysql_stmt_close(stmt);
}

bool MYSQL_PROXY::stmt_reset(MYSQL_STMT* stmt) {
    return mysql_stmt_reset(stmt);
}

bool MYSQL_PROXY::stmt_free_result(MYSQL_STMT* stmt) {
    return mysql_stmt_free_result(stmt);
}

bool MYSQL_PROXY::stmt_send_long_data(MYSQL_STMT* stmt, unsigned int param_number, const char* data,
    unsigned long length) {

    return mysql_stmt_send_long_data(stmt, param_number, data, length);
}

MYSQL_RES* MYSQL_PROXY::stmt_result_metadata(MYSQL_STMT* stmt) {
    return mysql_stmt_result_metadata(stmt);
}

unsigned int MYSQL_PROXY::stmt_errno(MYSQL_STMT* stmt) {
    return mysql_stmt_errno(stmt);
}

const char* MYSQL_PROXY::stmt_error(MYSQL_STMT* stmt) {
    return mysql_stmt_error(stmt);
}

MYSQL_ROW_OFFSET MYSQL_PROXY::stmt_row_seek(MYSQL_STMT* stmt, MYSQL_ROW_OFFSET offset) {
    return mysql_stmt_row_seek(stmt, offset);
}

MYSQL_ROW_OFFSET MYSQL_PROXY::stmt_row_tell(MYSQL_STMT* stmt) {
    return mysql_stmt_row_tell(stmt);
}

void MYSQL_PROXY::stmt_data_seek(MYSQL_STMT* stmt, uint64_t offset) {
    mysql_stmt_data_seek(stmt, offset);
}

uint64_t MYSQL_PROXY::stmt_num_rows(MYSQL_STMT* stmt) {
    return mysql_stmt_num_rows(stmt);
}

uint64_t MYSQL_PROXY::stmt_affected_rows(MYSQL_STMT* stmt) {
    return mysql_stmt_affected_rows(stmt);
}

unsigned int MYSQL_PROXY::stmt_field_count(MYSQL_STMT* stmt) {
    return mysql_stmt_field_count(stmt);
}

st_mysql_client_plugin* MYSQL_PROXY::client_find_plugin(const char* name, int type) {
    return mysql_client_find_plugin(mysql, name, type);
}

bool MYSQL_PROXY::is_connected() {
    return this->mysql != nullptr && this->mysql->net.vio;
}

void MYSQL_PROXY::set_last_error_code(unsigned int error_code) {
    this->mysql->net.last_errno = error_code;
}

char* MYSQL_PROXY::get_last_error() const {
    return this->mysql->net.last_error;
}

unsigned int MYSQL_PROXY::get_last_error_code() const {
    return this->mysql->net.last_errno;
}

char* MYSQL_PROXY::get_sqlstate() const {
    return this->mysql->net.sqlstate;
}

char* MYSQL_PROXY::get_server_version() const {
    return this->mysql->server_version;
}

uint64_t MYSQL_PROXY::get_affected_rows() const {
    return this->mysql->affected_rows;
}

void MYSQL_PROXY::set_affected_rows(uint64_t num_rows) {
    this->mysql->affected_rows = num_rows;
}

char* MYSQL_PROXY::get_host_info() const {
    return this->mysql->host_info;
}

std::string MYSQL_PROXY::get_host() {
    return (this->mysql && this->mysql->host) ? this->mysql->host : this->host->get_host();
}

unsigned int MYSQL_PROXY::get_port() {
    return (this->mysql) ? this->mysql->port : this->host->get_port();
}

unsigned long MYSQL_PROXY::get_max_packet() const {
    return this->mysql->net.max_packet;
}

unsigned long MYSQL_PROXY::get_server_capabilities() const {
    return this->mysql->server_capabilities;
}

unsigned int MYSQL_PROXY::get_server_status() const {
    return this->mysql->server_status;
}

void MYSQL_PROXY::delete_ds() {
    if (ds) {
        delete ds;
        ds = nullptr;
    }
}

MYSQL* MYSQL_PROXY::move_mysql_connection() {
    MYSQL* ret = this->mysql;
    this->mysql = nullptr;
    return ret;
}

void MYSQL_PROXY::set_connection(CONNECTION_PROXY* connection_proxy) {
    close();
    this->mysql = connection_proxy->move_mysql_connection();
    // delete the ds initialized in CONNECTION_HANDLER::clone_dbc()
    delete connection_proxy;
}

void MYSQL_PROXY::close_socket() {
    MYLOG_DBC_TRACE(dbc, "Closing socket");
    int ret = 0;
    if (mysql->net.fd != INVALID_SOCKET && (ret = shutdown(mysql->net.fd, SHUT_RDWR))) {
        MYLOG_DBC_TRACE(dbc, "shutdown() with return code: %d, error message: %s,", ret, strerror(socket_errno));
    }
    // Yield to main thread to handle socket shutdown
    std::this_thread::sleep_for(SOCKET_CLOSE_DELAY);
    if (mysql->net.fd != INVALID_SOCKET && (ret = ::closesocket(mysql->net.fd))) {
        MYLOG_DBC_TRACE(dbc, "closesocket() with return code: %d, error message: %s,", ret, strerror(socket_errno));
    }
}

bool MYSQL_PROXY::is_explicitly_closed() { return this->explicitly_closed; }
