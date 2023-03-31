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

CONNECTION_PROXY::CONNECTION_PROXY(DBC* dbc, DataSource* ds) : dbc{dbc}, ds{ds} {
    if (!this->dbc) {
        throw std::runtime_error("DBC cannot be null.");
    }

    if (!this->ds) {
        throw std::runtime_error("DataSource cannot be null.");
    }
}

CONNECTION_PROXY::~CONNECTION_PROXY() {
    if (this->next_proxy) {
        delete next_proxy;
    }
}

bool CONNECTION_PROXY::connect(const char* host, const char* user, const char* password,
    const char* database, unsigned int port, const char* socket, unsigned long flags) {

    if (ds->enable_dns_srv) {
        return this->real_connect_dns_srv(host, user, password, database, flags);
    }

    return this->real_connect(host, user, password, database, port, socket, flags);
}

void CONNECTION_PROXY::delete_ds() {
    next_proxy->delete_ds();
}

uint64_t CONNECTION_PROXY::num_rows(MYSQL_RES* res) {
    return next_proxy->num_rows(res);
}

unsigned int CONNECTION_PROXY::num_fields(MYSQL_RES* res) {
    return next_proxy->num_fields(res);
}

MYSQL_FIELD* CONNECTION_PROXY::fetch_field_direct(MYSQL_RES* res, unsigned int fieldnr) {
    return next_proxy->fetch_field_direct(res, fieldnr);
}

MYSQL_ROW_OFFSET CONNECTION_PROXY::row_tell(MYSQL_RES* res) {
    return next_proxy->row_tell(res);
}

unsigned int CONNECTION_PROXY::field_count() {
    return next_proxy->field_count();
}

uint64_t CONNECTION_PROXY::affected_rows() {
    return next_proxy->affected_rows();
}

unsigned int CONNECTION_PROXY::error_code() {
    return next_proxy->error_code();
}

const char* CONNECTION_PROXY::error() {
    if (has_custom_error_message) {		
        // We disable this flag after fetching the custom message once
        // so it does not obscure future proxy errors.
        has_custom_error_message = false;
		
        return this->custom_error_message.c_str();
    }

    return next_proxy->error();
}

const char* CONNECTION_PROXY::sqlstate() {
    return next_proxy->sqlstate();
}

unsigned long CONNECTION_PROXY::thread_id() {
    return next_proxy->thread_id();
}

int CONNECTION_PROXY::set_character_set(const char* csname) {
    return next_proxy->set_character_set(csname);
}

void CONNECTION_PROXY::init() {
    next_proxy->init();
}

bool CONNECTION_PROXY::ssl_set(const char* key, const char* cert, const char* ca, const char* capath, const char* cipher) {
    return next_proxy->ssl_set(key, cert, ca, capath, cipher);
}

bool CONNECTION_PROXY::change_user(const char* user, const char* passwd, const char* db) {
    return next_proxy->change_user(user, passwd, db);
}

bool CONNECTION_PROXY::real_connect(
    const char* host, const char* user, const char* passwd,
    const char* db, unsigned int port, const char* unix_socket,
    unsigned long clientflag) {

    return next_proxy->real_connect(host, user, passwd, db, port, unix_socket, clientflag);
}

int CONNECTION_PROXY::select_db(const char* db) {
    return next_proxy->select_db(db);
}

int CONNECTION_PROXY::query(const char* q) {
    return next_proxy->query(q);
}

int CONNECTION_PROXY::real_query(const char* q, unsigned long length) {
    return next_proxy->real_query(q, length);
}

MYSQL_RES* CONNECTION_PROXY::store_result() {
    return next_proxy->store_result();
}

MYSQL_RES* CONNECTION_PROXY::use_result() {
    return next_proxy->use_result();
}

struct CHARSET_INFO* CONNECTION_PROXY::get_character_set() const {
    return next_proxy->get_character_set();
}

void CONNECTION_PROXY::get_character_set_info(MY_CHARSET_INFO* charset) {
    next_proxy->get_character_set_info(charset);
}

bool CONNECTION_PROXY::autocommit(bool auto_mode) {
    return next_proxy->autocommit(auto_mode);
}

int CONNECTION_PROXY::next_result() {
    return next_proxy->next_result();
}

int CONNECTION_PROXY::stmt_next_result(MYSQL_STMT* stmt) {
    return next_proxy->stmt_next_result(stmt);
}

void CONNECTION_PROXY::close() {
    next_proxy->close();
}

bool CONNECTION_PROXY::real_connect_dns_srv(
    const char* dns_srv_name, const char* user,
    const char* passwd, const char* db, unsigned long client_flag) {

    return next_proxy->real_connect_dns_srv(dns_srv_name, user, passwd, db, client_flag);
}

int CONNECTION_PROXY::ping() {
    return next_proxy->ping();
}

unsigned long CONNECTION_PROXY::get_client_version(void) {
    return mysql_get_client_version();
}

int CONNECTION_PROXY::get_option(mysql_option option, const void* arg) {
    return next_proxy->get_option(option, arg);
}

int CONNECTION_PROXY::options4(mysql_option option, const void* arg1, const void* arg2) {
    return next_proxy->options4(option, arg1, arg2);
}

int CONNECTION_PROXY::options(mysql_option option, const void* arg) {
    return next_proxy->options(option, arg);
}

void CONNECTION_PROXY::free_result(MYSQL_RES* result) {
    next_proxy->free_result(result);
}

void CONNECTION_PROXY::data_seek(MYSQL_RES* result, uint64_t offset) {
    next_proxy->data_seek(result, offset);
}

MYSQL_ROW_OFFSET CONNECTION_PROXY::row_seek(MYSQL_RES* result, MYSQL_ROW_OFFSET offset) {
    return next_proxy->row_seek(result, offset);
}

MYSQL_FIELD_OFFSET CONNECTION_PROXY::field_seek(MYSQL_RES* result, MYSQL_FIELD_OFFSET offset) {
    return next_proxy->field_seek(result, offset);
}

MYSQL_ROW CONNECTION_PROXY::fetch_row(MYSQL_RES* result) {
    return next_proxy->fetch_row(result);
}

unsigned long* CONNECTION_PROXY::fetch_lengths(MYSQL_RES* result) {
    return next_proxy->fetch_lengths(result);
}

MYSQL_FIELD* CONNECTION_PROXY::fetch_field(MYSQL_RES* result) {
    return next_proxy->fetch_field(result);
}

MYSQL_RES* CONNECTION_PROXY::list_fields(const char* table, const char* wild) {
    return next_proxy->list_fields(table, wild);
}

unsigned long CONNECTION_PROXY::real_escape_string(char* to, const char* from, unsigned long length) {
    return next_proxy->real_escape_string(to, from, length);
}

bool CONNECTION_PROXY::bind_param(unsigned n_params, MYSQL_BIND* binds, const char** names) {
    return next_proxy->bind_param(n_params, binds, names);
}

MYSQL_STMT* CONNECTION_PROXY::stmt_init() {
    return next_proxy->stmt_init();
}

int CONNECTION_PROXY::stmt_prepare(MYSQL_STMT* stmt, const char* query, unsigned long length) {
    return next_proxy->stmt_prepare(stmt, query, length);
}

int CONNECTION_PROXY::stmt_execute(MYSQL_STMT* stmt) {
    return next_proxy->stmt_execute(stmt);
}

int CONNECTION_PROXY::stmt_fetch(MYSQL_STMT* stmt) {
    return next_proxy->stmt_fetch(stmt);
}

int CONNECTION_PROXY::stmt_fetch_column(MYSQL_STMT* stmt, MYSQL_BIND* bind_arg, unsigned int column, unsigned long offset) {
    return next_proxy->stmt_fetch_column(stmt, bind_arg, column, offset);
}

int CONNECTION_PROXY::stmt_store_result(MYSQL_STMT* stmt) {
    return next_proxy->stmt_store_result(stmt);
}

unsigned long CONNECTION_PROXY::stmt_param_count(MYSQL_STMT* stmt) {
    return next_proxy->stmt_param_count(stmt);
}

bool CONNECTION_PROXY::stmt_bind_param(MYSQL_STMT* stmt, MYSQL_BIND* bnd) {
    return next_proxy->stmt_bind_param(stmt, bnd);
}

bool CONNECTION_PROXY::stmt_bind_result(MYSQL_STMT* stmt, MYSQL_BIND* bnd) {
    return next_proxy->stmt_bind_result(stmt, bnd);
}

bool CONNECTION_PROXY::stmt_close(MYSQL_STMT* stmt) {
    return next_proxy->stmt_close(stmt);
}

bool CONNECTION_PROXY::stmt_reset(MYSQL_STMT* stmt) {
    return next_proxy->stmt_reset(stmt);
}

bool CONNECTION_PROXY::stmt_free_result(MYSQL_STMT* stmt) {
    return next_proxy->stmt_free_result(stmt);
}

bool CONNECTION_PROXY::stmt_send_long_data(MYSQL_STMT* stmt, unsigned int param_number, const char* data,
                                      unsigned long length) {

    return next_proxy->stmt_send_long_data(stmt, param_number, data, length);
}

MYSQL_RES* CONNECTION_PROXY::stmt_result_metadata(MYSQL_STMT* stmt) {
    return next_proxy->stmt_result_metadata(stmt);
}

unsigned int CONNECTION_PROXY::stmt_errno(MYSQL_STMT* stmt) {
    return next_proxy->stmt_errno(stmt);
}

const char* CONNECTION_PROXY::stmt_error(MYSQL_STMT* stmt) {
    return next_proxy->stmt_error(stmt);
}

MYSQL_ROW_OFFSET CONNECTION_PROXY::stmt_row_seek(MYSQL_STMT* stmt, MYSQL_ROW_OFFSET offset) {
    return next_proxy->stmt_row_seek(stmt, offset);
}

MYSQL_ROW_OFFSET CONNECTION_PROXY::stmt_row_tell(MYSQL_STMT* stmt) {
    return next_proxy->stmt_row_tell(stmt);
}

void CONNECTION_PROXY::stmt_data_seek(MYSQL_STMT* stmt, uint64_t offset) {
    next_proxy->stmt_data_seek(stmt, offset);
}

uint64_t CONNECTION_PROXY::stmt_num_rows(MYSQL_STMT* stmt) {
    return next_proxy->stmt_num_rows(stmt);
}

uint64_t CONNECTION_PROXY::stmt_affected_rows(MYSQL_STMT* stmt) {
    return next_proxy->stmt_affected_rows(stmt);
}

unsigned int CONNECTION_PROXY::stmt_field_count(MYSQL_STMT* stmt) {
    return next_proxy->stmt_field_count(stmt);
}

st_mysql_client_plugin* CONNECTION_PROXY::client_find_plugin(const char* name, int type) {
    return next_proxy->client_find_plugin(name, type);
}

bool CONNECTION_PROXY::is_connected() {
    return next_proxy->is_connected();
}

void CONNECTION_PROXY::set_last_error_code(unsigned int error_code) {
    next_proxy->set_last_error_code(error_code);
}

char* CONNECTION_PROXY::get_last_error() const {
    return next_proxy->get_last_error();
}

unsigned int CONNECTION_PROXY::get_last_error_code() const {
    return next_proxy->get_last_error_code();
}

char* CONNECTION_PROXY::get_sqlstate() const {
    return next_proxy->get_sqlstate();
}

char* CONNECTION_PROXY::get_server_version() const {
    return next_proxy->get_server_version();
}

uint64_t CONNECTION_PROXY::get_affected_rows() const {
    return next_proxy->get_affected_rows();
}

void CONNECTION_PROXY::set_affected_rows(uint64_t num_rows) {
    next_proxy->set_affected_rows(num_rows);
}

char* CONNECTION_PROXY::get_host_info() const {
    return next_proxy->get_host_info();
}

std::string CONNECTION_PROXY::get_host() {
    return next_proxy->get_host();
}

unsigned int CONNECTION_PROXY::get_port() {
    return next_proxy->get_port();
}

unsigned long CONNECTION_PROXY::get_max_packet() const {
    return next_proxy->get_max_packet();
}

unsigned long CONNECTION_PROXY::get_server_capabilities() const {
    return next_proxy->get_server_capabilities();
}

unsigned int CONNECTION_PROXY::get_server_status() const {
    return next_proxy->get_server_status();
}

void CONNECTION_PROXY::set_connection(CONNECTION_PROXY* connection_proxy) {
    next_proxy->set_connection(connection_proxy);
}

void CONNECTION_PROXY::close_socket() {
    next_proxy->close_socket();
}

void CONNECTION_PROXY::set_next_proxy(CONNECTION_PROXY* next_proxy) {
    if (this->next_proxy) {
        throw std::runtime_error("There is already a next proxy present!");
    }

    this->next_proxy = next_proxy;
}

MYSQL* CONNECTION_PROXY::move_mysql_connection() {
    return next_proxy ? next_proxy->move_mysql_connection() : nullptr;
}

void CONNECTION_PROXY::set_custom_error_message(const char* error_message) {
    this->custom_error_message = error_message;
    has_custom_error_message = true;
}
