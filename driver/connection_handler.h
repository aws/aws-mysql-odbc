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

#ifndef __CONNECTION_HANDLER_H__
#define __CONNECTION_HANDLER_H__

#include "host_info.h"
#include <memory>

#ifdef __linux__
typedef std::u16string sqlwchar_string;
#else
typedef std::wstring sqlwchar_string;
#endif

sqlwchar_string to_sqlwchar_string(const std::string& src);

struct DBC;
struct DataSource;
class CONNECTION_PROXY;
typedef short SQLRETURN;

class CONNECTION_HANDLER {
    public:
        CONNECTION_HANDLER(DBC* dbc);
        virtual ~CONNECTION_HANDLER();

        virtual SQLRETURN do_connect(DBC* dbc_ptr, DataSource* ds, bool failover_enabled);
        virtual CONNECTION_PROXY* connect(std::shared_ptr<HOST_INFO> host_info, DataSource* ds);
        void update_connection(CONNECTION_PROXY* new_connection, const std::string& new_host_name);

    private:
        DBC* dbc;
        DBC* clone_dbc(DBC* source_dbc, DataSource* ds);
};

#endif /* __CONNECTION_HANDLER_H__ */
