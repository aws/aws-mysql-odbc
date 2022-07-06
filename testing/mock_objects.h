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

#ifndef __MOCKOBJECTS_H__
#define __MOCKOBJECTS_H__

#include <gmock/gmock.h>

#include "driver/connection.h"
#include "driver/failover.h"

#ifdef WIN32
#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#include <stdlib.h>
#define DEBUG_NEW new (_NORMAL_BLOCK, __FILE__, __LINE__)
#define new DEBUG_NEW
#endif
#endif

class MOCK_CONNECTION : public CONNECTION_INTERFACE {
 public:
    MOCK_METHOD(bool, is_null, ());
    MOCK_METHOD(bool, is_connected, ());
    MOCK_METHOD(bool, try_execute_query, (const char*));
    MOCK_METHOD(char**, fetch_next_row, ());
    MOCK_METHOD(void, mock_connection_destructor, ());
    MOCK_METHOD(char*, get_host, ());
    MOCK_METHOD(unsigned int, get_port, ());

    ~MOCK_CONNECTION() override { mock_connection_destructor(); }
};

class MOCK_TOPOLOGY_SERVICE : public TOPOLOGY_SERVICE_INTERFACE {
 public:
    MOCK_METHOD(void, set_cluster_id, (std::string));
    MOCK_METHOD(void, set_cluster_instance_template, (std::shared_ptr<HOST_INFO>));
    MOCK_METHOD(std::shared_ptr<CLUSTER_TOPOLOGY_INFO>, get_topology, (CONNECTION_INTERFACE*, bool));
    MOCK_METHOD(void, mark_host_down, (std::shared_ptr<HOST_INFO>));
    MOCK_METHOD(void, mark_host_up, (std::shared_ptr<HOST_INFO>));
    MOCK_METHOD(void, set_refresh_rate, (int));
    MOCK_METHOD(void, set_gather_metric, (bool can_gather));
};

class MOCK_READER_HANDLER : public FAILOVER_READER_HANDLER {
 public:
    MOCK_READER_HANDLER() : FAILOVER_READER_HANDLER(nullptr, nullptr, 0, 0, nullptr, 0) {}
    MOCK_METHOD(READER_FAILOVER_RESULT, get_reader_connection,
                (std::shared_ptr<CLUSTER_TOPOLOGY_INFO>, FAILOVER_SYNC&));
};

class MOCK_CONNECTION_HANDLER : public FAILOVER_CONNECTION_HANDLER {
 public:
    MOCK_CONNECTION_HANDLER() : FAILOVER_CONNECTION_HANDLER(nullptr) {}
    MOCK_METHOD(CONNECTION_INTERFACE*, connect, (const std::shared_ptr<HOST_INFO>&));
    MOCK_METHOD(SQLRETURN, do_connect, (DBC*, DataSource*, bool));
};

class MOCK_FAILOVER_SYNC : public FAILOVER_SYNC {
public:
    MOCK_FAILOVER_SYNC() : FAILOVER_SYNC(1) {}
    MOCK_METHOD(bool, is_completed, ());
};

class MOCK_CLUSTER_AWARE_METRICS_CONTAINER : public CLUSTER_AWARE_METRICS_CONTAINER {
public:
    MOCK_CLUSTER_AWARE_METRICS_CONTAINER() : CLUSTER_AWARE_METRICS_CONTAINER() {}
    MOCK_CLUSTER_AWARE_METRICS_CONTAINER(DBC* dbc, DataSource* ds) : CLUSTER_AWARE_METRICS_CONTAINER(dbc, ds) {}

    // Expose protected functions
    bool is_enabled() { return CLUSTER_AWARE_METRICS_CONTAINER::is_enabled(); }
    bool is_instance_metrics_enabled() { return CLUSTER_AWARE_METRICS_CONTAINER::is_instance_metrics_enabled(); }

    MOCK_METHOD(std::string, get_curr_conn_url, ());
};

#endif /* __MOCKOBJECTS_H__ */
