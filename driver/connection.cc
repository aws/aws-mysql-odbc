/*
 * AWS ODBC Driver for MySQL
 * Copyright Amazon.com Inc. or affiliates.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation and/or
 * other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "connection.h"

CONNECTION::CONNECTION(MYSQL* conn) {
    this->connection = conn;
    this->query_result = nullptr;
}

CONNECTION::~CONNECTION() {
    if (this->connection) {
        // this->connection is deallocated in mysql_close()
        mysql_close(this->connection);
        this->connection = nullptr;
    }
    if (this->query_result) {
        // this->query_result is deallocated in mysql_free_result()
        mysql_free_result(this->query_result);
        this->query_result = nullptr;
    }
}

bool CONNECTION::is_connected() {
    return this->connection != nullptr && this->connection->net.vio;
}

bool CONNECTION::is_null() {
    return this->connection == nullptr;
}

bool CONNECTION::try_execute_query(const char* query) {
    if (this->query_result != nullptr) {
        mysql_free_result(this->query_result);
        this->query_result = nullptr;
    }
    
    if (this->connection != nullptr && mysql_query(this->connection, query) == 0) {
        this->query_result = mysql_store_result(this->connection);
    }

    return this->query_result != nullptr;
}

MYSQL_ROW CONNECTION::fetch_next_row() {
    if (this->query_result != nullptr) {
        MYSQL_ROW row = mysql_fetch_row(this->query_result);
        if (row) {
            return row;
        }

        mysql_free_result(this->query_result);
        this->query_result = nullptr;
    }
    
    return nullptr;
}

MYSQL* CONNECTION::real_connect(const char* host, const char* user,
    const char* passwd, const char* db,
    unsigned int port, const char* unix_socket,
    unsigned long client_flag) {

    return mysql_real_connect(
        this->connection, host, user, passwd, db, port, unix_socket, client_flag);
}

MYSQL* CONNECTION::real_connect_dns_srv(const char* dns_srv_name, const char* user,
    const char* passwd, const char* db,
    unsigned long client_flag) {

    return mysql_real_connect_dns_srv(
        this->connection, dns_srv_name, user, passwd, db, client_flag);
}

int CONNECTION::query(const char* query) {
    return mysql_query(this->connection, query);
}

int CONNECTION::real_query(const char* query, unsigned long length) {
    return mysql_real_query(this->connection, query, length);
}

uint64_t CONNECTION::call_affected_rows() {
    return mysql_affected_rows(this->connection);
}

uint64_t CONNECTION::get_affected_rows() {
    return this->connection->affected_rows;
}

void CONNECTION::set_affected_rows(uint64_t num_rows) {
    this->connection->affected_rows = num_rows;
}

unsigned int CONNECTION::field_count() {
    return mysql_field_count(this->connection);
}

MYSQL_RES* CONNECTION::list_fields(const char* table, const char* wild) {
    return mysql_list_fields(this->connection, table, wild);
}

int CONNECTION::options(enum mysql_option option, const void* arg) {
    return mysql_options(this->connection, option, arg);
}

int CONNECTION::options4(enum mysql_option option, const void* arg1, const void* arg2) {
    return mysql_options4(this->connection, option, arg1, arg2);
}

int CONNECTION::get_option(enum mysql_option option, const void* arg) {
    return mysql_get_option(this->connection, option, arg);
}

char* CONNECTION::get_host_info() {
    return this->connection->host_info;
}

char* CONNECTION::get_host() {
    return this->connection->host;
}

unsigned int CONNECTION::get_port() {
    return this->connection->port;
}

unsigned long CONNECTION::get_max_packet() {
    return this->connection->net.max_packet;
}

unsigned long CONNECTION::get_server_capabilities() {
    return this->connection->server_capabilities;
}

unsigned int CONNECTION::get_server_status() {
    return this->connection->server_status;
}

char* CONNECTION::get_server_version() {
    return this->connection->server_version;
}

bool CONNECTION::bind_param(unsigned n_params, MYSQL_BIND* binds, const char** names) {
    return mysql_bind_param(this->connection, n_params, binds, names);
}

int CONNECTION::next_result() {
    return mysql_next_result(this->connection);
}

MYSQL_RES* CONNECTION::store_result() {
    return mysql_store_result(this->connection);
}

MYSQL_RES* CONNECTION::use_result() {
    return mysql_use_result(this->connection);
}

bool CONNECTION::change_user(const char* user, const char* passwd, const char* db) {
    return mysql_change_user(this->connection, user, passwd, db);
}

int CONNECTION::select_db(const char* db) {
    return mysql_select_db(this->connection, db);
}

struct CHARSET_INFO* CONNECTION::get_character_set() {
    return this->connection->charset;
}

void CONNECTION::get_character_set_info(MY_CHARSET_INFO* charset) {
    mysql_get_character_set_info(this->connection, charset);
}

int CONNECTION::set_character_set(const char* csname) {
    return mysql_set_character_set(this->connection, csname);
}

unsigned long CONNECTION::real_escape_string(
    char* to,
    const char* from,
    unsigned long length) {

    return mysql_real_escape_string(this->connection, to, from, length);
}

int CONNECTION::ping() {
    return mysql_ping(this->connection);
}

MYSQL_STMT* CONNECTION::stmt_init() {
    return mysql_stmt_init(this->connection);
}

unsigned long CONNECTION::thread_id() {
    return mysql_thread_id(this->connection);
}

bool CONNECTION::autocommit(bool auto_mode) {
    return mysql_autocommit(this->connection, auto_mode);
}

char* CONNECTION::get_sqlstate() {
    return this->connection->net.sqlstate;
}

const char* CONNECTION::sqlstate() {
    return mysql_sqlstate(this->connection);
}

const char* CONNECTION::error() {
    return mysql_error(this->connection);
}

unsigned int CONNECTION::error_code() {
    return mysql_errno(this->connection);
}

char* CONNECTION::get_last_error() {
    return this->connection->net.last_error;
}

unsigned int CONNECTION::get_last_error_code() {
    return this->connection->net.last_errno;
}

void CONNECTION::set_last_error_code(unsigned int error_code) {
    auto net = this->connection->net;
    net.last_errno = error_code;
}

bool CONNECTION::ssl_set(const char* key, const char* cert,
                         const char* ca, const char* capath,
                         const char* cipher) {

    return mysql_ssl_set(this->connection, key, cert, ca, capath, cipher);
}

st_mysql_client_plugin* CONNECTION::client_find_plugin(const char* name, int type) {
    return mysql_client_find_plugin(this->connection, name, type);
}

bool CONNECTION::operator==(const CONNECTION c1) {
    return this->connection == c1.connection;
}

bool CONNECTION::operator!=(const CONNECTION c1) {
    return this->connection != c1.connection;
}
