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

#include "MYSQLLIB_PROXY.h"

MYSQLLIB_PROXY::MYSQLLIB_PROXY(DBC* dbc, DataSource* ds) : dbc{dbc}, ds{ds} {}

MYSQLLIB_PROXY::~MYSQLLIB_PROXY() {}

uint64_t MYSQLLIB_PROXY::num_rows(MYSQL_RES* res) {
    return mysql_num_rows(res);
}

unsigned int MYSQLLIB_PROXY::num_fields(MYSQL_RES* res) {
    return mysql_num_fields(res);
}

MYSQL_FIELD* MYSQLLIB_PROXY::fetch_field_direct(
    MYSQL_RES* res, unsigned int fieldnr) {
    return mysql_fetch_field_direct(res, fieldnr);
}

MYSQL_ROW_OFFSET MYSQLLIB_PROXY::row_tell(
    MYSQL_RES* res) {
    return mysql_row_tell(res);
}

unsigned int MYSQLLIB_PROXY::field_count(MYSQL* mysql) {
    return mysql_field_count(mysql);
}

uint64_t MYSQLLIB_PROXY::affected_rows(MYSQL* mysql) {
    return mysql_affected_rows(mysql);
}

unsigned int MYSQLLIB_PROXY::errorno(MYSQL* mysql) { return mysql_errno(mysql); }

const char* MYSQLLIB_PROXY::error(MYSQL* mysql) { return mysql_error(mysql); }

const char* MYSQLLIB_PROXY::sqlstate(MYSQL* mysql) { return mysql_sqlstate(mysql); }

unsigned long MYSQLLIB_PROXY::thread_id(MYSQL* mysql) {
    return mysql_thread_id(mysql);
}

int MYSQLLIB_PROXY::set_character_set(
    MYSQL* mysql, const char* csname) {
    return mysql_set_character_set(mysql, csname);
}

MYSQL* MYSQLLIB_PROXY::init(MYSQL* mysql) { return mysql_init(mysql); }

bool MYSQLLIB_PROXY::ssl_set(
    MYSQL* mysql, const char* key, const char* cert, const char* ca,
    const char* capath, const char* cipher) {
    return mysql_ssl_set(mysql, key, cert, ca, capath, cipher);
}

bool MYSQLLIB_PROXY::change_user(MYSQL* mysql,
                                 const char* user,
                                 const char* passwd,
                                 const char* db) {
    return mysql_change_user(mysql, user, passwd, db);
}

MYSQL* MYSQLLIB_PROXY::real_connect(
    MYSQL* mysql, const char* host, const char* user, const char* passwd,
    const char* db, unsigned int port, const char* unix_socket,
    unsigned long clientflag) {
    return mysql_real_connect(mysql, host, user, passwd, db, port, unix_socket, clientflag);
}

int MYSQLLIB_PROXY::select_db(MYSQL* mysql,
                              const char* db) {
    return mysql_select_db(mysql, db);
}

int MYSQLLIB_PROXY::query(MYSQL* mysql, const char* q) { return mysql_query(mysql, q); }

int MYSQLLIB_PROXY::real_query(MYSQL* mysql,
                               const char* q,
                               unsigned long length) {
    return mysql_real_query(mysql, q, length);
}

MYSQL_RES* MYSQLLIB_PROXY::store_result(MYSQL* mysql) {
    return mysql_store_result(mysql);
}

MYSQL_RES* MYSQLLIB_PROXY::use_result(MYSQL* mysql) { return mysql_use_result(mysql); }

void MYSQLLIB_PROXY::get_character_set_info(MYSQL* mysql,
                                            MY_CHARSET_INFO* charset) {
    mysql_get_character_set_info(mysql, charset);
}

bool MYSQLLIB_PROXY::autocommit(MYSQL* mysql,
                                bool auto_mode) {
    return mysql_autocommit(mysql, auto_mode);
}

int MYSQLLIB_PROXY::next_result(MYSQL* mysql) { return mysql_next_result(mysql); }

int MYSQLLIB_PROXY::stmt_next_result(MYSQL_STMT* stmt) {
    return mysql_stmt_next_result(stmt);
}

void MYSQLLIB_PROXY::close(MYSQL* sock) { mysql_close(sock); }

MYSQL* MYSQLLIB_PROXY::real_connect_dns_srv(
    MYSQL* mysql, const char* dns_srv_name, const char* user,
    const char* passwd, const char* db, unsigned long client_flag) {
    return mysql_real_connect_dns_srv(mysql, dns_srv_name, user, passwd, db, client_flag);
}

int MYSQLLIB_PROXY::ping(MYSQL* mysql) { return mysql_ping(mysql); }

unsigned long MYSQLLIB_PROXY::get_client_version(void) {
    return mysql_get_client_version();
}

int MYSQLLIB_PROXY::get_option(MYSQL* mysql,
                               mysql_option option,
                               const void* arg) {
    return mysql_get_option(mysql, option, arg);
}

int MYSQLLIB_PROXY::options4(MYSQL* mysql,
                             mysql_option option,
                             const void* arg1,
                             const void* arg2) {
    return mysql_options4(mysql, option, arg1, arg2);
}

int MYSQLLIB_PROXY::options(MYSQL* mysql,
                            mysql_option option,
                            const void* arg) {
    return mysql_options(mysql, option, arg);
}

void MYSQLLIB_PROXY::free_result(MYSQL_RES* result) { mysql_free_result(result); }

void MYSQLLIB_PROXY::data_seek(MYSQL_RES* result,
                               uint64_t offset) { mysql_data_seek(result, offset); }

MYSQL_ROW_OFFSET MYSQLLIB_PROXY::row_seek(
    MYSQL_RES* result, MYSQL_ROW_OFFSET offset) {
    return mysql_row_seek(result, offset);
}

MYSQL_FIELD_OFFSET MYSQLLIB_PROXY::field_seek(
    MYSQL_RES* result, MYSQL_FIELD_OFFSET offset) {
    return mysql_field_seek(result, offset);
}

MYSQL_ROW MYSQLLIB_PROXY::fetch_row(MYSQL_RES* result) {
    return mysql_fetch_row(result);
}

unsigned long* MYSQLLIB_PROXY::fetch_lengths(
    MYSQL_RES* result) {
    return mysql_fetch_lengths(result);
}

MYSQL_FIELD* MYSQLLIB_PROXY::fetch_field(
    MYSQL_RES* result) {
    return mysql_fetch_field(result);
}

MYSQL_RES* MYSQLLIB_PROXY::list_fields(
    MYSQL* mysql, const char* table, const char* wild) {
    return mysql_list_fields(mysql, table, wild);
}

unsigned long MYSQLLIB_PROXY::real_escape_string(
    MYSQL* mysql, char* to, const char* from, unsigned long length) {
    return mysql_real_escape_string(mysql, to, from, length);
}

bool MYSQLLIB_PROXY::bind_param(MYSQL* mysql,
                                unsigned n_params,
                                MYSQL_BIND* binds,
                                const char** names) {
    return mysql_bind_param(mysql, n_params, binds, names);
}

MYSQL_STMT* MYSQLLIB_PROXY::stmt_init(MYSQL* mysql) { return mysql_stmt_init(mysql); }

int MYSQLLIB_PROXY::stmt_prepare(MYSQL_STMT* stmt,
                                 const char* query,
                                 unsigned long length) {
    return mysql_stmt_prepare(stmt, query, length);
}

int MYSQLLIB_PROXY::stmt_execute(MYSQL_STMT* stmt) {
    return mysql_stmt_execute(stmt);
}

int MYSQLLIB_PROXY::stmt_fetch(MYSQL_STMT* stmt) { return mysql_stmt_fetch(stmt); }

int MYSQLLIB_PROXY::stmt_fetch_column(
    MYSQL_STMT* stmt, MYSQL_BIND* bind_arg, unsigned int column,
    unsigned long offset) {
    return mysql_stmt_fetch_column(stmt, bind_arg, column, offset);
}

int MYSQLLIB_PROXY::stmt_store_result(MYSQL_STMT* stmt) {
    return mysql_stmt_store_result(stmt);
}

bool MYSQLLIB_PROXY::stmt_bind_param(MYSQL_STMT* stmt,
                                     MYSQL_BIND* bnd) {
    return mysql_stmt_bind_param(stmt, bnd);
}

bool MYSQLLIB_PROXY::stmt_bind_result(MYSQL_STMT* stmt,
                                      MYSQL_BIND* bnd) {
    return mysql_stmt_bind_result(stmt, bnd);
}

bool MYSQLLIB_PROXY::stmt_close(MYSQL_STMT* stmt) { return mysql_stmt_close(stmt); }

bool MYSQLLIB_PROXY::stmt_reset(MYSQL_STMT* stmt) { return mysql_stmt_reset(stmt); }

bool MYSQLLIB_PROXY::stmt_free_result(MYSQL_STMT* stmt) {
    return mysql_stmt_free_result(stmt);
}

bool MYSQLLIB_PROXY::stmt_send_long_data(
    MYSQL_STMT* stmt, unsigned int param_number, const char* data,
    unsigned long length) {
    return mysql_stmt_send_long_data(stmt, param_number, data, length);
}

MYSQL_RES* MYSQLLIB_PROXY::stmt_result_metadata(
    MYSQL_STMT* stmt) {
    return mysql_stmt_result_metadata(stmt);
}

unsigned int MYSQLLIB_PROXY::stmt_errno(
    MYSQL_STMT* stmt) {
    return mysql_stmt_errno(stmt);
}

const char* MYSQLLIB_PROXY::stmt_error(
    MYSQL_STMT* stmt) {
    return mysql_stmt_error(stmt);
}

MYSQL_ROW_OFFSET MYSQLLIB_PROXY::stmt_row_seek(
    MYSQL_STMT* stmt, MYSQL_ROW_OFFSET offset) {
    return mysql_stmt_row_seek(stmt, offset);
}

MYSQL_ROW_OFFSET MYSQLLIB_PROXY::stmt_row_tell(
    MYSQL_STMT* stmt) {
    return mysql_stmt_row_tell(stmt);
}

void MYSQLLIB_PROXY::stmt_data_seek(MYSQL_STMT* stmt,
                                    uint64_t offset) { mysql_stmt_data_seek(stmt, offset); }

uint64_t MYSQLLIB_PROXY::stmt_num_rows(
    MYSQL_STMT* stmt) {
    return mysql_stmt_num_rows(stmt);
}

uint64_t MYSQLLIB_PROXY::stmt_affected_rows(
    MYSQL_STMT* stmt) {
    return mysql_stmt_affected_rows(stmt);
}

unsigned int MYSQLLIB_PROXY::stmt_field_count(
    MYSQL_STMT* stmt) {
    return mysql_stmt_field_count(stmt);
}

st_mysql_client_plugin* MYSQLLIB_PROXY::client_find_plugin(
    MYSQL* mysql, const char* name, int type) {
    return mysql_client_find_plugin(mysql, name, type);
}
