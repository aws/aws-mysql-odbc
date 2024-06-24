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

/**
  @file  connection_handler.c
  @brief connection functions.
*/

#include "connection_handler.h"
#include "connection_proxy.h"
#include "driver.h"

#include <codecvt>
#include <locale>

#ifdef __linux__
    sqlwchar_string to_sqlwchar_string(const std::string& src) {
        return std::wstring_convert< std::codecvt_utf8_utf16< char16_t >,
                                     char16_t >{}
            .from_bytes(src);
    }
#else
    sqlwchar_string to_sqlwchar_string(const std::string& src) {
        return std::wstring_convert< std::codecvt_utf8< wchar_t >, wchar_t >{}
            .from_bytes(src);
    }
#endif

CONNECTION_HANDLER::CONNECTION_HANDLER(DBC* dbc) : dbc{dbc} {}

CONNECTION_HANDLER::~CONNECTION_HANDLER() = default;

SQLRETURN CONNECTION_HANDLER::do_connect(DBC* dbc_ptr, DataSource* ds, bool failover_enabled, bool is_monitor_connection) {
    return dbc_ptr->connect(ds, failover_enabled, is_monitor_connection);
}

CONNECTION_PROXY* CONNECTION_HANDLER::connect(std::shared_ptr<HOST_INFO> host_info, DataSource* ds, bool is_monitor_connection) {

    if (dbc == nullptr || host_info == nullptr) {
        return nullptr;
    }

    DataSource* ds_to_use = new DataSource();
    ds_to_use->copy(ds ? ds : dbc->ds);
    const auto new_host = to_sqlwchar_string(host_info->get_host());

    DBC* dbc_clone = clone_dbc(dbc, ds_to_use);
    ds_to_use->opt_SERVER.set_remove_brackets((SQLWCHAR*) new_host.c_str(), new_host.size());

    CONNECTION_PROXY* new_connection = nullptr;
    CLEAR_DBC_ERROR(dbc_clone);
    const SQLRETURN rc = do_connect(dbc_clone, ds_to_use, ds_to_use->opt_ENABLE_CLUSTER_FAILOVER, is_monitor_connection);

    if (rc == SQL_SUCCESS || rc == SQL_SUCCESS_WITH_INFO) {
        new_connection = dbc_clone->connection_proxy;
        dbc_clone->connection_proxy = nullptr;
    }

    my_SQLFreeConnect(dbc_clone);

    return new_connection;
}

void CONNECTION_HANDLER::update_connection(
    CONNECTION_PROXY* new_connection, const std::string& new_host_name) {

    if (new_connection->is_connected()) {
        dbc->close();
        dbc->connection_proxy->set_connection(new_connection);
        
        CLEAR_DBC_ERROR(dbc);

        const sqlwchar_string new_host_name_wstr = to_sqlwchar_string(new_host_name);

        // Update original ds to reflect change in host/server.

        dbc->ds->opt_SERVER.set_remove_brackets((SQLWCHAR*) new_host_name_wstr.c_str(), new_host_name_wstr.size());
    }
}

DBC* CONNECTION_HANDLER::clone_dbc(DBC* source_dbc, DataSource* ds) {

    DBC* dbc_clone = nullptr;

    SQLRETURN status = SQL_ERROR;
    if (source_dbc && source_dbc->env) {
        SQLHDBC hdbc;
        SQLHENV henv = static_cast<SQLHANDLE>(source_dbc->env);

        status = my_SQLAllocConnect(henv, &hdbc);
        if (status == SQL_SUCCESS || status == SQL_SUCCESS_WITH_INFO) {
            dbc_clone = static_cast<DBC*>(hdbc);
            dbc_clone->init_proxy_chain(ds);
        } else {
            const char* err = "Cannot allocate connection handle when cloning DBC";
            MYLOG_DBC_TRACE(dbc, err);
            throw std::runtime_error(err);
        }
    }
    return dbc_clone;
}
