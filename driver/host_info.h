// Copyright (c) 2001, 2018, Oracle and/or its affiliates. All rights reserved.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0, as
// published by the Free Software Foundation.
//
// This program is also distributed with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation. The authors of MySQL hereby grant you an
// additional permission to link the program and your derivative works
// with the separately licensed software that they have included with
// MySQL.
//
// Without limiting anything contained in the foregoing, this file,
// which is part of <MySQL Product>, is also subject to the
// Universal FOSS Exception, version 1.0, a copy of which can be found at
// http://oss.oracle.com/licenses/universal-foss-exception.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

/**
  @file  host_info.h
  @brief Declaration of HOST_INFO
*/

#ifndef __HOSTINFO_H__
#define __HOSTINFO_H__

#include <memory>
#include <string>

enum HOST_STATE { UP, DOWN };

// TODO Think about char types. Using strings for now, but should SQLCHAR *, or CHAR * be employed?
// Most of the strings are for internal failover things
class HOST_INFO {
public:
    HOST_INFO();
    //TODO - probably choose one of the following constructors, or more precisely choose which data type they should take
    HOST_INFO(std::string host, int port);
    HOST_INFO(const char* host, int port);
    HOST_INFO(std::string host, int port, HOST_STATE state, bool is_writer);
    HOST_INFO(const char* host, int port, HOST_STATE state, bool is_writer);
    ~HOST_INFO();


    int get_port();
    std::string get_host();
    std::string get_host_port_pair();
    bool equal_host_port_pair(HOST_INFO& hi);
    HOST_STATE get_host_state();
    void set_host_state(HOST_STATE state);
    bool is_host_up();
    bool is_host_down();
    bool is_host_writer();
    void mark_as_writer(bool writer);
    static bool is_host_same(const std::shared_ptr<HOST_INFO>& h1, const std::shared_ptr<HOST_INFO>& h2);
    static constexpr int NO_PORT = -1;

    // used to be properties - TODO - remove the ones that are not necessary
    std::string session_id;
    std::string last_updated;
    std::string replica_lag;
    std::string instance_name;

private:
    const std::string HOST_PORT_SEPARATOR = ":";
    const std::string host;
    const int port = NO_PORT;

    HOST_STATE host_state;
    bool is_writer;
};

#endif /* __HOSTINFO_H__ */
