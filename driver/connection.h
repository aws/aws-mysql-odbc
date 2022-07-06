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

#ifndef __CONNECTION_H__
#define __CONNECTION_H__

#include <memory>
#include <mysql.h>

class CONNECTION_INTERFACE {
public:
    virtual ~CONNECTION_INTERFACE() {};

    virtual bool is_null() = 0;
    virtual bool is_connected() = 0;
    virtual bool try_execute_query(const char* query) = 0;
    virtual char** fetch_next_row() = 0;
    virtual char* get_host() = 0;
    virtual unsigned int get_port() = 0;
};

class CONNECTION : virtual public CONNECTION_INTERFACE {
public:
    CONNECTION(MYSQL* conn);
    virtual ~CONNECTION();
    bool is_connected() override;

    bool is_null() override;
    bool try_execute_query(const char* query) override;
    MYSQL_ROW fetch_next_row() override;

    MYSQL* real_connect(const char* host, const char* user,
                        const char* passwd, const char* db,
                        unsigned int port, const char* unix_socket,
                        unsigned long client_flag);

    MYSQL* real_connect_dns_srv(const char* dns_srv_name, const char* user,
                                const char* passwd, const char* db,
                                unsigned long client_flag);

    int query(const char* query);
    int real_query(const char* query, unsigned long length);

    uint64_t call_affected_rows();
    uint64_t get_affected_rows();
    void set_affected_rows(uint64_t num_rows);

    unsigned int field_count();
    MYSQL_RES* list_fields(const char* table, const char* wild);

    int options(enum mysql_option option, const void* arg);
    int options4(enum mysql_option option, const void* arg1, const void* arg2);
    int get_option(enum mysql_option option, const void* arg);

    char* get_host_info();
    char* get_host() override;
    unsigned int get_port() override;
    unsigned long get_max_packet();

    unsigned long get_server_capabilities();
    unsigned int get_server_status();
    char* get_server_version();

    bool bind_param(unsigned n_params, MYSQL_BIND* binds, const char** names);

    int next_result();
    MYSQL_RES* store_result();
    MYSQL_RES* use_result();

    bool change_user(const char* user, const char* passwd, const char* db);
    int select_db(const char* db);

    struct CHARSET_INFO* get_character_set();
    void get_character_set_info(MY_CHARSET_INFO* charset);
    int set_character_set(const char* csname);

    unsigned long real_escape_string(
        char* to, const char* from, unsigned long length);

    int ping();
    MYSQL_STMT* stmt_init();
    unsigned long thread_id();

    bool autocommit(bool auto_mode);

    char* get_sqlstate();
    const char* sqlstate();
    const char* error();
    unsigned int error_code();
    char* get_last_error();
    unsigned int get_last_error_code();
    void set_last_error_code(unsigned int error_code);

    bool ssl_set(const char* key, const char* cert,
                 const char* ca, const char* capath,
                 const char* cipher);

    st_mysql_client_plugin* client_find_plugin(
        const char* name,
        int type);

    bool operator==(const CONNECTION c1);
    bool operator!=(const CONNECTION c1);

private:
    MYSQL* connection;
    MYSQL_RES* query_result;
};

#endif /* __CONNECTION_H__ */
