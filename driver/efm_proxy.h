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

#ifndef __EFM_PROXY__
#define __EFM_PROXY__

#include <mysql.h>

#include "monitor_service.h"
#include "mysql_proxy.h"

class EFM_PROXY : public MYSQL_PROXY {
   public:
    int set_character_set(const char* csname) override;
    void init() override;
    bool ssl_set(const char* key, const char* cert, const char* ca,
                 const char* capath, const char* cipher) override;
    bool change_user(const char* user, const char* passwd, const char* db) override;
    bool real_connect(const char* host, const char* user,
                      const char* passwd, const char* db, unsigned int port,
                      const char* unix_socket, unsigned long clientflag) override;
    int select_db(const char* db) override;
    int query(const char* q) override;
    int real_query(const char* q, unsigned long length) override;
    MYSQL_RES* store_result() override;
    MYSQL_RES* use_result() override;
    bool autocommit(bool auto_mode) override;
    int next_result() override;
    int stmt_next_result(MYSQL_STMT* stmt) override;
    bool real_connect_dns_srv(const char* dns_srv_name,
                              const char* user, const char* passwd,
                              const char* db, unsigned long client_flag) override;
    int ping() override;
    int options(enum mysql_option option, const void* arg) override;
    int options4(enum mysql_option option, const void* arg1,
                 const void* arg2) override;
    void free_result(MYSQL_RES* result) override;
    MYSQL_ROW fetch_row(MYSQL_RES* result) override;
    MYSQL_RES* list_fields(const char* table, const char* wild) override;
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
    bool stmt_close(MYSQL_STMT* stmt) override;
    bool stmt_reset(MYSQL_STMT* stmt) override;
    bool stmt_free_result(MYSQL_STMT* stmt) override;
    bool stmt_send_long_data(MYSQL_STMT* stmt, unsigned int param_number,
                             const char* data, unsigned long length) override;
    MYSQL_RES* stmt_result_metadata(MYSQL_STMT* stmt) override;
    struct st_mysql_client_plugin* client_find_plugin(const char* name, int type) override;

   private:
    std::shared_ptr<MONITOR_SERVICE> monitor_service = nullptr;
    std::set<std::string> node_keys;

    std::shared_ptr<MONITOR_CONNECTION_CONTEXT> start_monitoring();
    void stop_monitoring(std::shared_ptr<MONITOR_CONNECTION_CONTEXT> context);
};

#endif /* __EFM_PROXY__ */
