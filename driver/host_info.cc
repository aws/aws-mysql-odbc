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
  @file  host_info.cc
  @brief HOST_INFO functions.
*/

#include "host_info.h"

// TODO
// the entire HOST_INFO needs to be reviewed based on needed interfaces and other objects like CLUSTER_TOPOLOGY_INFO
// most/all of the HOST_INFO potentially could be internal to CLUSTER_TOPOLOGY_INFO and specfic information may be accessed
// through CLUSTER_TOPOLOGY_INFO
// Move the implementation to it's own file

HOST_INFO::HOST_INFO()
    : HOST_INFO("", -1)
{
}

HOST_INFO::HOST_INFO(std::string host, int port)
    : host{ host }, port{ port }, host_state{ UP }, is_writer{ false }
{
}

// would need some checks for nulls
HOST_INFO::HOST_INFO(const char* host, int port)
    : host{ host }, port{ port }, host_state{ UP }, is_writer{ false }
{
}

/**
 * Returns the host.
 *
 * @return the host
 */
std::string HOST_INFO::get_host() {
    return host;
}

/**
 * Returns the port.
 *
 * @return the port
 */
int  HOST_INFO::get_port() {
    return port;
}

/**
 * Returns a host:port representation of this host.
 *
 * @return the host:port representation of this host
 */
std::string HOST_INFO::get_host_port_pair() {
    return get_host() + HOST_PORT_SEPARATOR + std::to_string(get_port());
}


bool HOST_INFO::equal_host_port_pair(HOST_INFO& hi) {
    return get_host_port_pair() == hi.get_host_port_pair();
}

HOST_STATE HOST_INFO::get_host_state() {
    return host_state;
}

void HOST_INFO::set_host_state(HOST_STATE state) {
    host_state = state;
}

bool HOST_INFO::is_host_up() {
    return host_state == UP;
}

bool HOST_INFO::is_host_down() {
    return host_state == DOWN;
}

bool HOST_INFO::is_host_writer() {
    return is_writer;
}
void HOST_INFO::mark_as_writer(bool writer) {
    is_writer = writer;
}

// Check if two host info have same instance name
bool HOST_INFO::is_host_same(const std::shared_ptr<HOST_INFO>& h1, const std::shared_ptr<HOST_INFO>& h2) {
    return h1->instance_name == h2->instance_name;
}
