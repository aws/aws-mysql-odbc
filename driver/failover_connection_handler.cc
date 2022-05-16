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

/**
  @file  failover_connection_handler.c
  @brief Failover connection functions.
*/

#include "driver.h"
#include "failover.h"

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

FAILOVER_CONNECTION_HANDLER::FAILOVER_CONNECTION_HANDLER(DBC* dbc) : dbc{dbc} {}

FAILOVER_CONNECTION_HANDLER::~FAILOVER_CONNECTION_HANDLER() {}

SQLRETURN FAILOVER_CONNECTION_HANDLER::do_connect(DBC* dbc_ptr, DataSource* ds, bool failover_enabled) {
    return dbc_ptr->connect(ds, failover_enabled);
}

CONNECTION_INTERFACE* FAILOVER_CONNECTION_HANDLER::connect(const std::shared_ptr<HOST_INFO>& host_info) {

    if (dbc == nullptr || dbc->ds == nullptr || host_info == nullptr) {
        return nullptr;
    }
    // TODO Convert string to wstring. Note: need to revisit if support Linux
    const auto new_host = to_sqlwchar_string(host_info->get_host());

    DBC* dbc_clone = clone_dbc(dbc);
    ds_set_wstrnattr(&dbc_clone->ds->server, (SQLWCHAR*)new_host.c_str(), new_host.size());

    CONNECTION_INTERFACE* new_connection = nullptr;
    CLEAR_DBC_ERROR(dbc_clone);
    const SQLRETURN rc = do_connect(dbc_clone, dbc_clone->ds, true);

    if (rc == SQL_SUCCESS || rc == SQL_SUCCESS_WITH_INFO) {
        new_connection = dbc_clone->mysql;
        dbc_clone->mysql = nullptr;
    }

    // TODO guard these from potential exceptions thrown before, make sure release happens.
    release_dbc(dbc_clone);

    return new_connection;
}

void FAILOVER_CONNECTION_HANDLER::update_connection(
    CONNECTION_INTERFACE* new_connection, const std::string& new_host_name) {

    if (new_connection->is_connected()) {
        dbc->close();

        // CONNECTION is the only implementation of CONNECTION_INTERFACE
        // so dynamic_cast should be safe here.
        // TODO: Is there an alternative to dynamic_cast here?
        dbc->mysql = dynamic_cast<CONNECTION*>(new_connection);
        
        CLEAR_DBC_ERROR(dbc);

        const sqlwchar_string new_host_name_wstr = to_sqlwchar_string(new_host_name);

        // Update original ds to reflect change in host/server.
        ds_set_wstrnattr(&dbc->ds->server, (SQLWCHAR*)new_host_name_wstr.c_str(), new_host_name_wstr.size());
        ds_set_strnattr(&dbc->ds->server8, (SQLCHAR*)new_host_name.c_str(), new_host_name.size());
    }
}

DBC* FAILOVER_CONNECTION_HANDLER::clone_dbc(DBC* source_dbc) {

    DBC* dbc_clone = nullptr;

    SQLRETURN status = SQL_ERROR;
    if (source_dbc && source_dbc->env) {
        SQLHDBC hdbc;
        SQLHENV henv = static_cast<SQLHANDLE>(source_dbc->env);

        status = my_SQLAllocConnect(henv, &hdbc);
        if (status == SQL_SUCCESS || status == SQL_SUCCESS_WITH_INFO) {
            dbc_clone = static_cast<DBC*>(hdbc);
            dbc_clone->ds = ds_new();
            ds_copy(dbc_clone->ds, source_dbc->ds);
        } else {
            const char* err = "Cannot allocate connection handle when cloning DBC in writer failover process";
            MYLOG_DBC_TRACE(dbc, err);
            throw std::runtime_error(err);
        }
    }
    return dbc_clone;
}

void FAILOVER_CONNECTION_HANDLER::release_dbc(DBC* dbc_clone) {
    // dbc->ds is deleted(if not null) in DBC destructor
    // no need to separately clean it.
    my_SQLFreeConnect(static_cast<SQLHANDLE>(dbc_clone));
}
