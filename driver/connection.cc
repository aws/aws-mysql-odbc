// Copyright (c) 2001, 2018, Oracle and/or its affiliates. All rights reserved.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0, as
// published by the Free Software Foundation.
//
// This program is also distributed with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation. The authors of MySQL hereby grant you an
// additional permission to link the program and your derivative works
// with the separately licensed software that they have included with
// MySQL.
//
// Without limiting anything contained in the foregoing, this file,
// which is part of MySQL Connector/ODBC, is also subject to the
// Universal FOSS Exception, version 1.0, a copy of which can be found at
// http://oss.oracle.com/licenses/universal-foss-exception.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

/**
  @file connection.cc
  @brief Implementation of CONNECTION class implementing CONNECTION_INTERFACE
*/

#include "connection.h"

CONNECTION::CONNECTION(MYSQL* conn) {
    this->connection = std::shared_ptr<MYSQL>(conn, [](MYSQL* ptr) {ptr = nullptr;});
    this->query_result = nullptr;
}

CONNECTION::CONNECTION(std::shared_ptr<MYSQL> conn) {
    this->connection = conn;
    this->query_result = nullptr;
}

bool CONNECTION::is_connected() {
    return this->connection != nullptr && this->connection.get()->net.vio;
}

bool CONNECTION::is_null() {
    return this->connection == nullptr;
}

bool CONNECTION::try_execute_query(const char* query) {
    if (this->query_result != nullptr) {
        mysql_free_result(this->query_result.get());
        this->query_result = nullptr;
    }
    
    MYSQL* conn = this->connection.get();
    if (conn != NULL && mysql_query(conn, query) == 0) {
        MYSQL_RES* result = mysql_store_result(conn);
        this->query_result = std::make_shared<MYSQL_RES>(*result);
    }

    return this->query_result != nullptr;
}

MYSQL_ROW CONNECTION::fetch_next_row() {
    if (this->query_result != nullptr) {
        MYSQL_ROW row = mysql_fetch_row(this->query_result.get());
        if (row) {
            return row;
        }

        mysql_free_result(this->query_result.get());
        this->query_result = nullptr;
    }
    
    return nullptr;
}

void CONNECTION::close_connection() {
    mysql_close(this->connection.get());
}

MYSQL* CONNECTION::real_connect(const char* host, const char* user,
    const char* passwd, const char* db,
    unsigned int port, const char* unix_socket,
    unsigned long client_flag) {

    return mysql_real_connect(
        this->connection.get(), host, user, passwd, db, port, unix_socket, client_flag);
}

MYSQL* CONNECTION::real_connect_dns_srv(const char* dns_srv_name, const char* user,
    const char* passwd, const char* db,
    unsigned long client_flag) {

    return mysql_real_connect_dns_srv(
        this->connection.get(), dns_srv_name, user, passwd, db, client_flag);
}

int CONNECTION::query(const char* query) {
    return mysql_query(this->connection.get(), query);
}

int CONNECTION::real_query(const char* query, unsigned long length) {
    return mysql_real_query(this->connection.get(), query, length);
}

uint64_t CONNECTION::call_affected_rows() {
    return mysql_affected_rows(this->connection.get());
}

uint64_t CONNECTION::get_affected_rows() {
    return this->connection.get()->affected_rows;
}

void CONNECTION::set_affected_rows(uint64_t num_rows) {
    this->connection.get()->affected_rows = num_rows;
}

unsigned int CONNECTION::field_count() {
    return mysql_field_count(this->connection.get());
}

MYSQL_RES* CONNECTION::list_fields(const char* table, const char* wild) {
    return mysql_list_fields(this->connection.get(), table, wild);
}

int CONNECTION::options(enum mysql_option option, const void* arg) {
    return mysql_options(this->connection.get(), option, arg);
}

int CONNECTION::options4(enum mysql_option option, const void* arg1, const void* arg2) {
    return mysql_options4(this->connection.get(), option, arg1, arg2);
}

int CONNECTION::get_option(enum mysql_option option, const void* arg) {
    return mysql_get_option(this->connection.get(), option, arg);
}

char* CONNECTION::get_host_info() {
    return this->connection.get()->host_info;
}

unsigned long CONNECTION::get_max_packet() {
    return this->connection.get()->net.max_packet;
}

unsigned long CONNECTION::get_server_capabilities() {
    return this->connection.get()->server_capabilities;
}

unsigned int CONNECTION::get_server_status() {
    return this->connection.get()->server_status;
}

char* CONNECTION::get_server_version() {
    return this->connection.get()->server_version;
}

bool CONNECTION::bind_param(unsigned n_params, MYSQL_BIND* binds, const char** names) {
    return mysql_bind_param(this->connection.get(), n_params, binds, names);
}

int CONNECTION::next_result() {
    return mysql_next_result(this->connection.get());
}

MYSQL_RES* CONNECTION::store_result() {
    return mysql_store_result(this->connection.get());
}

MYSQL_RES* CONNECTION::use_result() {
    return mysql_use_result(this->connection.get());
}

bool CONNECTION::change_user(const char* user, const char* passwd, const char* db) {
    return mysql_change_user(this->connection.get(), user, passwd, db);
}

int CONNECTION::select_db(const char* db) {
    return mysql_select_db(this->connection.get(), db);
}

struct CHARSET_INFO* CONNECTION::get_character_set() {
    return this->connection.get()->charset;
}

void CONNECTION::get_character_set_info(MY_CHARSET_INFO* charset) {
    mysql_get_character_set_info(this->connection.get(), charset);
}

int CONNECTION::set_character_set(const char* csname) {
    return mysql_set_character_set(this->connection.get(), csname);
}

unsigned long CONNECTION::real_escape_string(
    char* to,
    const char* from,
    unsigned long length) {

    return mysql_real_escape_string(this->connection.get(), to, from, length);
}

int CONNECTION::ping() {
    return mysql_ping(this->connection.get());
}

MYSQL_STMT* CONNECTION::stmt_init() {
    return mysql_stmt_init(this->connection.get());
}

unsigned long CONNECTION::thread_id() {
    return mysql_thread_id(this->connection.get());
}

bool CONNECTION::autocommit(bool auto_mode) {
    return mysql_autocommit(this->connection.get(), auto_mode);
}

char* CONNECTION::get_sqlstate() {
    return this->connection.get()->net.sqlstate;
}

const char* CONNECTION::sqlstate() {
    return mysql_sqlstate(this->connection.get());
}

const char* CONNECTION::error() {
    return mysql_error(this->connection.get());
}

unsigned int CONNECTION::error_code() {
    return mysql_errno(this->connection.get());
}

char* CONNECTION::get_last_error() {
    return this->connection.get()->net.last_error;
}

unsigned int CONNECTION::get_last_error_code() {
    return this->connection.get()->net.last_errno;
}

void CONNECTION::set_last_error_code(unsigned int error_code) {
    auto net = this->connection.get()->net;
    net.last_errno = error_code;
}

bool CONNECTION::ssl_set(const char* key, const char* cert,
                         const char* ca, const char* capath,
                         const char* cipher) {

    return mysql_ssl_set(this->connection.get(), key, cert, ca, capath, cipher);
}

st_mysql_client_plugin* CONNECTION::client_find_plugin(const char* name, int type) {
    return mysql_client_find_plugin(this->connection.get(), name, type);
}

bool CONNECTION::operator==(const CONNECTION c1) {
    return this->connection.get() == c1.connection.get();
}

bool CONNECTION::operator!=(const CONNECTION c1) {
    return this->connection.get() != c1.connection.get();
}
