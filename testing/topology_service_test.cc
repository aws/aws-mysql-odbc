// Copyright (c) 2003, 2018, Oracle and/or its affiliates. All rights reserved.
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

#include "mock_objects.h"

#include "driver/topology_service.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <thread>

using ::testing::Return;

MOCK_CONNECTION* mc;
std::shared_ptr<CONNECTION_INTERFACE> conn;
TOPOLOGY_SERVICE* ts;
std::shared_ptr<HOST_INFO> cluster_instance;

char* writer_id = const_cast<char*>(WRITER_SESSION_ID.c_str());
char* reader1[4] = { "replica-instance-1", "Replica", "2020-09-15 17:51:53.0", "13.5" };
char* reader2[4] = { "replica-instance-2", "Replica", "2020-09-15 17:51:53.0", "13.5" };
char* writer[4] = { "writer-instance", writer_id, "2020-09-15 17:51:53.0", "13.5" };
char* writer1[4] = { "writer-instance-1", writer_id, "2020-09-15 17:51:53.0", "13.5" };
char* writer2[4] = { "writer-instance-2", writer_id, "2020-09-15 17:51:53.0", "13.5" };
char* writer3[4] = { "writer-instance-3", writer_id, "2020-09-15 17:51:53.0", "13.5" };

class TopologyServiceTest : public testing::Test {
protected:
    static void SetUpTestSuite() {
        conn = std::shared_ptr<CONNECTION_INTERFACE>(new MOCK_CONNECTION);
        mc = static_cast<MOCK_CONNECTION*>(conn.get());
		
        ts = new TOPOLOGY_SERVICE();
        cluster_instance = std::make_shared<HOST_INFO>(HOST_INFO("?.XYZ.us-east-2.rds.amazonaws.com", 1234));
        ts->set_cluster_instance_template(cluster_instance);
    }

    static void TearDownTestSuite() {
        conn.reset();
        cluster_instance.reset();
        delete ts;
    }
	
    void SetUp() override {
        ts->set_refresh_rate(DEFAULT_REFRESH_RATE_IN_MILLISECONDS);
    }

    void TearDown() override {
        ts->clear_all();
    }
};

TEST_F(TopologyServiceTest, TopologyQuery) {
    EXPECT_CALL(*mc, try_execute_query(RETRIEVE_TOPOLOGY_SQL))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*mc, fetch_next_row())
        .WillOnce(Return(reader1))
        .WillOnce(Return(writer))
        .WillOnce(Return(reader2))
        .WillRepeatedly(Return(MYSQL_ROW{}));

    std::shared_ptr<CLUSTER_TOPOLOGY_INFO> topology = ts->get_topology(conn);
	
    EXPECT_FALSE(topology->is_multi_writer_cluster());
    EXPECT_EQ(3, topology->total_hosts());
    EXPECT_EQ(2, topology->num_readers());

    std::shared_ptr<HOST_INFO> writer_host = topology->get_writer();
	
    EXPECT_EQ("writer-instance.XYZ.us-east-2.rds.amazonaws.com", writer_host->get_host());
    EXPECT_EQ(1234, writer_host->get_port());
    EXPECT_EQ("writer-instance", writer_host->instance_name);
    EXPECT_EQ(WRITER_SESSION_ID, writer_host->session_id);
    EXPECT_EQ("2020-09-15 17:51:53.0", writer_host->last_updated);
    EXPECT_EQ("13.5", writer_host->replica_lag);
}

TEST_F(TopologyServiceTest, MultiWriter) {
    EXPECT_CALL(*mc, try_execute_query(RETRIEVE_TOPOLOGY_SQL))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*mc, fetch_next_row())
        .WillOnce(Return(writer1))
        .WillOnce(Return(writer2))
        .WillOnce(Return(writer3))
        .WillRepeatedly(Return(MYSQL_ROW{}));

    std::shared_ptr<CLUSTER_TOPOLOGY_INFO> topology = ts->get_topology(conn);

    EXPECT_TRUE(topology->is_multi_writer_cluster());
    EXPECT_EQ(3, topology->total_hosts());
    EXPECT_EQ(0, topology->num_readers());

    std::shared_ptr<HOST_INFO> writer_host = topology->get_writer();

    EXPECT_EQ("writer-instance-1.XYZ.us-east-2.rds.amazonaws.com", writer_host->get_host());
    EXPECT_EQ(1234, writer_host->get_port());
    EXPECT_EQ("writer-instance-1", writer_host->instance_name);
    EXPECT_EQ(WRITER_SESSION_ID, writer_host->session_id);
    EXPECT_EQ("2020-09-15 17:51:53.0", writer_host->last_updated);
    EXPECT_EQ("13.5", writer_host->replica_lag);
}

TEST_F(TopologyServiceTest, CachedTopology) {
    EXPECT_CALL(*mc, try_execute_query(RETRIEVE_TOPOLOGY_SQL))
        .Times(1)
        .WillOnce(Return(true));
    EXPECT_CALL(*mc, fetch_next_row())
        .Times(4)
        .WillOnce(Return(reader1))
        .WillOnce(Return(writer))
        .WillOnce(Return(reader2))
        .WillOnce(Return(MYSQL_ROW{}));

    ts->get_topology(conn);

    // 2nd call to get_topology() should retrieve from cache instead of executing another query
    // which is why we expect try_execute_query to be called only once
    ts->get_topology(conn);
}

TEST_F(TopologyServiceTest, QueryFailure) {
    EXPECT_CALL(*mc, try_execute_query(RETRIEVE_TOPOLOGY_SQL))
        .WillOnce(Return(false));

    std::shared_ptr<CLUSTER_TOPOLOGY_INFO> topology = ts->get_topology(conn);

    EXPECT_EQ(nullptr, topology);
}

TEST_F(TopologyServiceTest, StaleTopology) {
    EXPECT_CALL(*mc, try_execute_query(RETRIEVE_TOPOLOGY_SQL))
        .Times(2)
        .WillOnce(Return(true))
        .WillOnce(Return(false));
    EXPECT_CALL(*mc, fetch_next_row())
        .WillOnce(Return(reader1))
        .WillOnce(Return(writer))
        .WillOnce(Return(reader2))
        .WillRepeatedly(Return(MYSQL_ROW{}));

    ts->set_refresh_rate(1);

    std::shared_ptr<CLUSTER_TOPOLOGY_INFO> hosts = ts->get_topology(conn);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    std::shared_ptr<CLUSTER_TOPOLOGY_INFO> stale_hosts = ts->get_topology(conn);
	
    EXPECT_EQ(3, stale_hosts->total_hosts());
    EXPECT_EQ(hosts, stale_hosts);
}

TEST_F(TopologyServiceTest, RefreshTopology) {
    EXPECT_CALL(*mc, try_execute_query(RETRIEVE_TOPOLOGY_SQL))
        .Times(2)
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*mc, fetch_next_row())
        .WillOnce(Return(reader1))
        .WillOnce(Return(writer))
        .WillOnce(Return(reader2))
        .WillRepeatedly(Return(MYSQL_ROW{}));

    ts->set_refresh_rate(1);

    ts->get_topology(conn);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    ts->get_topology(conn);
}

TEST_F(TopologyServiceTest, ClearCache) {
    EXPECT_CALL(*mc, try_execute_query(RETRIEVE_TOPOLOGY_SQL))
        .WillOnce(Return(true))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*mc, fetch_next_row())
        .WillOnce(Return(reader1))
        .WillOnce(Return(writer))
        .WillOnce(Return(reader2))
        .WillRepeatedly(Return(MYSQL_ROW{}));

    auto topology = ts->get_topology(conn);
    EXPECT_NE(nullptr, topology);
	
    ts->clear_all();

    // topology should now be null after above clear_all()
    topology = ts->get_topology(conn);
    EXPECT_EQ(nullptr, topology);
}
