/**
  @file connection.h
  @brief Declaration of CONNECTION and CONNECTION_INTERFACE
*/

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
