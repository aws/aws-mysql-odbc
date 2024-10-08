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

#ifndef __MYSQL_PROXY__
#define __MYSQL_PROXY__

#include "connection_proxy.h"
#include "driver.h"
#include "host_info.h"

class MYSQL_PROXY : public CONNECTION_PROXY {
public:
    MYSQL_PROXY(DBC* dbc, DataSource* ds);
    ~MYSQL_PROXY() override;

    uint64_t num_rows(MYSQL_RES* res) override;
    unsigned int num_fields(MYSQL_RES* res) override;
    MYSQL_FIELD* fetch_field_direct(MYSQL_RES* res, unsigned int fieldnr) override;
    MYSQL_ROW_OFFSET row_tell(MYSQL_RES* res) override;

    unsigned int field_count() override;
    uint64_t affected_rows() override;
    unsigned int error_code() override;
    const char* error() override;
    const char* sqlstate() override;
    unsigned long thread_id() override;
    int set_character_set(const char* csname) override;

    void init() override;
    bool change_user(const char* user, const char* passwd,
        const char* db) override;
    bool real_connect(const char* host, const char* user,
        const char* passwd, const char* db, unsigned int port,
        const char* unix_socket, unsigned long clientflag) override;
    int select_db(const char* db) override;
    int query(const char* q) override;
    int real_query(const char* q, unsigned long length) override;
    MYSQL_RES* store_result() override;
    MYSQL_RES* use_result() override;
    struct CHARSET_INFO* get_character_set() const;
    void get_character_set_info(MY_CHARSET_INFO* charset) override;

    int ping() override;
    int options(enum mysql_option option, const void* arg) override;
    int options4(enum mysql_option option, const void* arg1,
        const void* arg2) override;
    int get_option(enum mysql_option option, const void* arg) override;
    void free_result(MYSQL_RES* result) override;
    void data_seek(MYSQL_RES* result, uint64_t offset) override;
    MYSQL_ROW_OFFSET row_seek(MYSQL_RES* result, MYSQL_ROW_OFFSET offset) override;
    MYSQL_FIELD_OFFSET field_seek(MYSQL_RES* result, MYSQL_FIELD_OFFSET offset) override;
    MYSQL_ROW fetch_row(MYSQL_RES* result) override;

    unsigned long* fetch_lengths(MYSQL_RES* result) override;
    MYSQL_FIELD* fetch_field(MYSQL_RES* result) override;
    unsigned long real_escape_string(char* to, const char* from,
        unsigned long length) override;

    bool bind_param(unsigned n_params, MYSQL_BIND* binds,
        const char** names) override;

    MYSQL_STMT* stmt_init() override;
    int stmt_prepare(MYSQL_STMT* stmt, const char* query, unsigned long length) override;
    int stmt_execute(MYSQL_STMT* stmt) override;
    int stmt_fetch(MYSQL_STMT* stmt) override;
    int stmt_fetch_column(MYSQL_STMT* stmt, MYSQL_BIND* bind_arg,
        unsigned int column, unsigned long offset) override;
    int stmt_store_result(MYSQL_STMT* stmt) override;
    unsigned long stmt_param_count(MYSQL_STMT* stmt) override;
    bool stmt_bind_param(MYSQL_STMT* stmt, MYSQL_BIND* bnd) override;
    bool stmt_bind_result(MYSQL_STMT* stmt, MYSQL_BIND* bnd) override;
    bool stmt_bind_named_param(MYSQL_STMT* stmt, MYSQL_BIND* binds,
                               unsigned n_params, const char** names) override;
    bool stmt_close(MYSQL_STMT* stmt) override;
    bool stmt_reset(MYSQL_STMT* stmt) override;
    bool stmt_free_result(MYSQL_STMT* stmt) override;
    bool stmt_send_long_data(MYSQL_STMT* stmt, unsigned int param_number,
        const char* data, unsigned long length) override;
    MYSQL_RES* stmt_result_metadata(MYSQL_STMT* stmt) override;
    unsigned int stmt_errno(MYSQL_STMT* stmt) override;
    const char* stmt_error(MYSQL_STMT* stmt) override;
    MYSQL_ROW_OFFSET stmt_row_seek(MYSQL_STMT* stmt, MYSQL_ROW_OFFSET offset) override;
    MYSQL_ROW_OFFSET stmt_row_tell(MYSQL_STMT* stmt) override;
    void stmt_data_seek(MYSQL_STMT* stmt, uint64_t offset) override;
    uint64_t stmt_num_rows(MYSQL_STMT* stmt) override;
    uint64_t stmt_affected_rows(MYSQL_STMT* stmt) override;
    unsigned int stmt_field_count(MYSQL_STMT* stmt) override;

    bool commit() override;
    bool rollback() override;
    bool autocommit(bool auto_mode) override;
    bool more_results() override;
    int next_result() override;
    int stmt_next_result(MYSQL_STMT* stmt) override;
    void close() override;

    bool real_connect_dns_srv(const char* dns_srv_name,
        const char* user, const char* passwd,
        const char* db, unsigned long client_flag) override;
    struct st_mysql_client_plugin* client_find_plugin(
        const char* name, int type) override;

    bool is_connected() override;

    void set_last_error_code(unsigned int error_code) override;

    char* get_last_error() const;

    unsigned int get_last_error_code() const;

    char* get_sqlstate() const;

    char* get_server_version() const;

    uint64_t get_affected_rows() const;

    void set_affected_rows(uint64_t num_rows) override;

    char* get_host_info() const;

    std::string get_host() override;

    unsigned int get_port() override;

    unsigned long get_max_packet() const;

    unsigned long get_server_capabilities() const;

    unsigned int get_server_status() const;

    void delete_ds() override;

    MYSQL* move_mysql_connection() override;

    void set_connection(CONNECTION_PROXY* connection_proxy) override;

    void close_socket() override;

private:
    MYSQL* mysql = nullptr;
    std::shared_ptr<HOST_INFO> host = nullptr;
};


#endif /* __MYSQL_PROXY__ */
