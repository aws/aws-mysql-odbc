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

#include "test_utils.h"

void allocate_odbc_handles(SQLHENV& env, DBC*& dbc, DataSource*& ds) {
    SQLHDBC hdbc = nullptr;

    SQLAllocHandle(SQL_HANDLE_ENV, nullptr, &env);
    SQLAllocHandle(SQL_HANDLE_DBC, env, &hdbc);
    dbc = static_cast<DBC*>(hdbc);
    ds = ds_new();
}

void cleanup_odbc_handles(SQLHENV& env, DBC*& dbc, DataSource*& ds, bool call_myodbc_end) {
    SQLHDBC hdbc = static_cast<SQLHDBC>(dbc);
    if (nullptr != hdbc) {
        SQLFreeHandle(SQL_HANDLE_DBC, hdbc);
    }
    if (nullptr != env) {
        SQLFreeHandle(SQL_HANDLE_ENV, env);
#ifndef _UNIX_
        // Needed to free memory on Windows
        if (call_myodbc_end)
            myodbc_end();
#endif
    }
    if (nullptr != dbc) {
        dbc = nullptr;
    }
    if (nullptr != ds) {
        ds_delete(ds);
        ds = nullptr;
    }
}
