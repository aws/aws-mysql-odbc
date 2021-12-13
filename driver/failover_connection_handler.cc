/**
  @file  failover_connection_handler.c
  @brief Failover connection functions.
*/

#include "driver.h"
#include "failover.h"

#include <codecvt>
#include <locale>

FAILOVER_CONNECTION_HANDLER::FAILOVER_CONNECTION_HANDLER(
    std::shared_ptr<DBC> dbc)
    : dbc{dbc} {}

FAILOVER_CONNECTION_HANDLER::~FAILOVER_CONNECTION_HANDLER() {}

std::shared_ptr<MYSQL> FAILOVER_CONNECTION_HANDLER::connect(
    std::shared_ptr<HOST_INFO> host_info) {

    if (dbc == nullptr || dbc->ds == nullptr || host_info == nullptr) {
        return nullptr;
    }
    // TODO Convert string to wstring. Note: need to revist if support Linux
    auto host = host_info->get_host();
    auto new_host =
        std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t>{}.from_bytes(host);

    auto dbc_clone = clone_dbc(dbc);
    ds_set_strnattr(&dbc_clone->ds->server, new_host.c_str(), new_host.size());

    std::shared_ptr<MYSQL> new_connection;
    CLEAR_DBC_ERROR(dbc_clone.get());
    SQLRETURN rc = dbc_clone->connect(dbc_clone->ds);

    if (rc == SQL_SUCCESS || rc == SQL_SUCCESS_WITH_INFO) {
        new_connection.reset(dbc_clone->mysql);
        dbc_clone->mysql = nullptr;

        // TODO ponder whether or not we should update ds - the data source in
        // original dbc while updating connection. currently as we don't do
        // that, the original ds holds the initial, user supplied connection
        // information, e.g. host, etc. the release_dbc method deletes the
        // cloned dbc and cloned ds.
    }

    // TODO guard these from potential exceptions thrown before, make sure
    // release happens.
    release_dbc(dbc_clone);

    return new_connection;
}

void FAILOVER_CONNECTION_HANDLER::update_connection(
    std::shared_ptr<MYSQL> new_connection) {

    if (new_connection) {
        dbc->close();
        dbc->mysql = new_connection.get();
        CLEAR_DBC_ERROR(dbc.get());

        // TODO: should we also update dbc->ds when updating connection? How
        // this would affect the user?  Same mentioned in TODO above
        //  The only difference would be ds->server and ds->server8. Currently
        //  we're presering the initial server information.
    }
}

void FAILOVER_CONNECTION_HANDLER::release_connection(
    std::shared_ptr<MYSQL> mysql) {

    mysql_close(mysql.get());
}

std::shared_ptr<DBC> FAILOVER_CONNECTION_HANDLER::clone_dbc(
    std::shared_ptr<DBC> source_dbc) {

    std::shared_ptr<DBC> dbc_clone;

    SQLRETURN status = SQL_ERROR;
    if (source_dbc && source_dbc->env) {
        SQLHANDLE hdbc = nullptr;
        status = SQLAllocHandle(SQL_HANDLE_DBC,
                                static_cast<SQLHANDLE>(source_dbc->env), &hdbc);
        if (status == SQL_SUCCESS || status == SQL_SUCCESS_WITH_INFO) {
            dbc_clone.reset(static_cast<DBC*>(hdbc));
            dbc_clone->ds = ds_new();
            ds_copy(dbc_clone->ds, source_dbc->ds);
        } else {
            throw std::runtime_error(
                "Cannot allocate connection handle when cloning DBC in writer "
                "failover process");
        }
    }
    return dbc_clone;
}

void FAILOVER_CONNECTION_HANDLER::release_dbc(std::shared_ptr<DBC> dbc_clone) {
    // dbc->ds is deleted(if not null) in DBC destructor
    // no need to separately clean it.
    SQLFreeHandle(SQL_HANDLE_DBC, static_cast<SQLHANDLE>(dbc_clone.get()));
}
