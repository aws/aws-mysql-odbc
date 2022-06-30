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

#ifndef __MYSQL_PROXY__
#define __MYSQL_PROXY__

#include <mysql.h>

#include "monitor_service.h"

struct DBC;
struct DataSource;

class CONNECTION_INTERFACE {
public:
    virtual ~CONNECTION_INTERFACE() {};

    virtual bool is_null() = 0;
    virtual bool is_connected() = 0;
    virtual char* get_host() = 0;
    virtual unsigned int get_port() = 0;
    virtual int query(const char* q) = 0;
    virtual MYSQL_RES* store_result() = 0;
    virtual MYSQL_ROW fetch_row(MYSQL_RES* result) = 0;
    virtual void free_result(MYSQL_RES* result) = 0;
};

class MYSQL_PROXY : virtual public CONNECTION_INTERFACE {
public:
    MYSQL_PROXY(DBC* dbc, DataSource* ds);
    ~MYSQL_PROXY() override;

    uint64_t num_rows(MYSQL_RES* res);
    unsigned int num_fields(MYSQL_RES* res);
    MYSQL_FIELD* fetch_field_direct(MYSQL_RES* res, unsigned int fieldnr);
    MYSQL_ROW_OFFSET row_tell(MYSQL_RES* res);

    unsigned int field_count();
    uint64_t affected_rows();
    unsigned int error_code();
    const char* error();
    const char* sqlstate();
    unsigned long thread_id();
    int set_character_set(const char* csname);

    void init();
    bool ssl_set(const char* key, const char* cert, const char* ca,
                 const char* capath, const char* cipher);
    bool change_user(const char* user, const char* passwd,
                     const char* db);
    bool real_connect(const char* host, const char* user,
                      const char* passwd, const char* db, unsigned int port,
                      const char* unix_socket, unsigned long clientflag);
    int select_db(const char* db);
    int query(const char* q) override;
    int real_query(const char* q, unsigned long length);
    MYSQL_RES* store_result() override;
    MYSQL_RES* use_result();
    struct CHARSET_INFO* get_character_set() const;
    void get_character_set_info(MY_CHARSET_INFO* charset);

    virtual int ping();
    static unsigned long get_client_version(void);
    virtual int options(enum mysql_option option, const void* arg);
    int options4(enum mysql_option option, const void* arg1,
                 const void* arg2);
    int get_option(enum mysql_option option, const void* arg);
    void free_result(MYSQL_RES* result) override;
    void data_seek(MYSQL_RES* result, uint64_t offset);
    MYSQL_ROW_OFFSET row_seek(MYSQL_RES* result, MYSQL_ROW_OFFSET offset);
    MYSQL_FIELD_OFFSET field_seek(MYSQL_RES* result, MYSQL_FIELD_OFFSET offset);
    MYSQL_ROW fetch_row(MYSQL_RES* result) override;

    unsigned long* fetch_lengths(MYSQL_RES* result);
    MYSQL_FIELD* fetch_field(MYSQL_RES* result);
    MYSQL_RES* list_fields(const char* table, const char* wild);
    unsigned long real_escape_string(char* to, const char* from,
                                     unsigned long length);

    bool bind_param(unsigned n_params, MYSQL_BIND* binds,
                    const char** names);

    MYSQL_STMT* stmt_init();
    int stmt_prepare(MYSQL_STMT* stmt, const char* query, unsigned long length);
    int stmt_execute(MYSQL_STMT* stmt);
    int stmt_fetch(MYSQL_STMT* stmt);
    int stmt_fetch_column(MYSQL_STMT* stmt, MYSQL_BIND* bind_arg,
                          unsigned int column, unsigned long offset);
    int stmt_store_result(MYSQL_STMT* stmt);
    unsigned long stmt_param_count(MYSQL_STMT* stmt);
    bool stmt_bind_param(MYSQL_STMT* stmt, MYSQL_BIND* bnd);
    bool stmt_bind_result(MYSQL_STMT* stmt, MYSQL_BIND* bnd);
    bool stmt_close(MYSQL_STMT* stmt);
    bool stmt_reset(MYSQL_STMT* stmt);
    bool stmt_free_result(MYSQL_STMT* stmt);
    bool stmt_send_long_data(MYSQL_STMT* stmt, unsigned int param_number,
                             const char* data, unsigned long length);
    MYSQL_RES* stmt_result_metadata(MYSQL_STMT* stmt);
    unsigned int stmt_errno(MYSQL_STMT* stmt);
    const char* stmt_error(MYSQL_STMT* stmt);
    MYSQL_ROW_OFFSET stmt_row_seek(MYSQL_STMT* stmt, MYSQL_ROW_OFFSET offset);
    MYSQL_ROW_OFFSET stmt_row_tell(MYSQL_STMT* stmt);
    void stmt_data_seek(MYSQL_STMT* stmt, uint64_t offset);
    uint64_t stmt_num_rows(MYSQL_STMT* stmt);
    uint64_t stmt_affected_rows(MYSQL_STMT* stmt);
    unsigned int stmt_field_count(MYSQL_STMT* stmt);

    bool autocommit(bool auto_mode);
    int next_result();
    int stmt_next_result(MYSQL_STMT* stmt);
    void close();

    bool real_connect_dns_srv(const char* dns_srv_name,
                              const char* user, const char* passwd,
                              const char* db, unsigned long client_flag);
    struct st_mysql_client_plugin* client_find_plugin(
        const char* name, int type);

    bool is_connected() override;

    bool is_null() override;

    void set_last_error_code(unsigned int error_code);

    char* get_last_error() const;

    unsigned int get_last_error_code() const;

    char* get_sqlstate() const;

    char* get_server_version() const;

    uint64_t get_affected_rows() const;

    void set_affected_rows(uint64_t num_rows);

    char* get_host_info() const;

    char* get_host() override;

    unsigned int get_port() override;

    unsigned long get_max_packet() const;

    unsigned long get_server_capabilities() const;

    unsigned int get_server_status() const;

    void set_connection(MYSQL_PROXY* mysql_proxy);

protected:
    DBC* dbc = nullptr;
    DataSource* ds = nullptr;
    MYSQL* mysql = nullptr;
    std::shared_ptr<MONITOR_SERVICE> monitor_service = nullptr;
    std::set<std::string> node_keys;

    std::shared_ptr<MONITOR_CONNECTION_CONTEXT> start_monitoring();
    void stop_monitoring(std::shared_ptr<MONITOR_CONNECTION_CONTEXT> context);
};

class MYSQL_MONITOR_PROXY : public MYSQL_PROXY {
public:
    MYSQL_MONITOR_PROXY(DBC* dbc, DataSource* ds) : MYSQL_PROXY(dbc, ds) {};

    virtual void init();
    int ping();
    int options(enum mysql_option option, const void* arg);
    virtual bool connect();
};

#endif /* __MYSQL_PROXY__ */
