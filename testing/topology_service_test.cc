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

#include "mock_objects.h"

#include "driver/topology_service.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <thread>

using ::testing::Return;
using ::testing::StrEq;

namespace {
    MOCK_CONNECTION* mc;
    TOPOLOGY_SERVICE* ts;
    std::shared_ptr<HOST_INFO> cluster_instance;

    std::string cluster_id("eac7829c-bb6d-4d1a-82fa-a8c2470910f2");

    char* reader1[4] = { "replica-instance-1", "Replica", "2020-09-15 17:51:53.0", "13.5" };
    char* reader2[4] = { "replica-instance-2", "Replica", "2020-09-15 17:51:53.0", "13.5" };
    char* writer[4] = { "writer-instance", WRITER_SESSION_ID, "2020-09-15 17:51:53.0", "13.5" };
    char* writer1[4] = { "writer-instance-1", WRITER_SESSION_ID, "2020-09-15 17:51:53.0", "13.5" };
    char* writer2[4] = { "writer-instance-2", WRITER_SESSION_ID, "2020-09-15 17:51:53.0", "13.5" };
    char* writer3[4] = { "writer-instance-3", WRITER_SESSION_ID, "2020-09-15 17:51:53.0", "13.5" };
}  // namespace

class TopologyServiceTest : public testing::Test {
protected:
    static void SetUpTestSuite() {
        mc = new MOCK_CONNECTION();
		
        ts = new TOPOLOGY_SERVICE();
        cluster_instance = std::make_shared<HOST_INFO>(HOST_INFO("?.XYZ.us-east-2.rds.amazonaws.com", 1234));
        ts->set_cluster_instance_template(cluster_instance);
        ts->set_cluster_id(cluster_id);
    }

    static void TearDownTestSuite() {
        cluster_instance.reset();
        delete mc;
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
    EXPECT_CALL(*mc, try_execute_query(StrEq(RETRIEVE_TOPOLOGY_SQL)))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*mc, fetch_next_row())
        .WillOnce(Return(reader1))
        .WillOnce(Return(writer))
        .WillOnce(Return(reader2))
        .WillRepeatedly(Return(MYSQL_ROW{}));

    std::shared_ptr<CLUSTER_TOPOLOGY_INFO> topology = ts->get_topology(mc);
    ASSERT_NE(nullptr, topology);
	
    EXPECT_FALSE(topology->is_multi_writer_cluster());
    EXPECT_EQ(3, topology->total_hosts());
    EXPECT_EQ(2, topology->num_readers());

    std::shared_ptr<HOST_INFO> writer_host = topology->get_writer();
    ASSERT_NE(nullptr, writer_host);
	
    EXPECT_EQ("writer-instance.XYZ.us-east-2.rds.amazonaws.com", writer_host->get_host());
    EXPECT_EQ(1234, writer_host->get_port());
    EXPECT_EQ("writer-instance", writer_host->instance_name);
    EXPECT_EQ(WRITER_SESSION_ID, writer_host->session_id);
    EXPECT_EQ("2020-09-15 17:51:53.0", writer_host->last_updated);
    EXPECT_EQ("13.5", writer_host->replica_lag);
}

TEST_F(TopologyServiceTest, MultiWriter) {
    EXPECT_CALL(*mc, try_execute_query(StrEq(RETRIEVE_TOPOLOGY_SQL)))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*mc, fetch_next_row())
        .WillOnce(Return(writer1))
        .WillOnce(Return(writer2))
        .WillOnce(Return(writer3))
        .WillRepeatedly(Return(MYSQL_ROW{}));

    std::shared_ptr<CLUSTER_TOPOLOGY_INFO> topology = ts->get_topology(mc);
    ASSERT_NE(nullptr, topology);

    EXPECT_TRUE(topology->is_multi_writer_cluster());
    EXPECT_EQ(3, topology->total_hosts());
    EXPECT_EQ(0, topology->num_readers());

    std::shared_ptr<HOST_INFO> writer_host = topology->get_writer();
    ASSERT_NE(nullptr, writer_host);

    EXPECT_EQ("writer-instance-1.XYZ.us-east-2.rds.amazonaws.com", writer_host->get_host());
    EXPECT_EQ(1234, writer_host->get_port());
    EXPECT_EQ("writer-instance-1", writer_host->instance_name);
    EXPECT_EQ(WRITER_SESSION_ID, writer_host->session_id);
    EXPECT_EQ("2020-09-15 17:51:53.0", writer_host->last_updated);
    EXPECT_EQ("13.5", writer_host->replica_lag);
}

TEST_F(TopologyServiceTest, CachedTopology) {
    EXPECT_CALL(*mc, try_execute_query(StrEq(RETRIEVE_TOPOLOGY_SQL)))
        .Times(1)
        .WillOnce(Return(true));
    EXPECT_CALL(*mc, fetch_next_row())
        .Times(4)
        .WillOnce(Return(reader1))
        .WillOnce(Return(writer))
        .WillOnce(Return(reader2))
        .WillOnce(Return(MYSQL_ROW{}));

    ts->get_topology(mc);

    // 2nd call to get_topology() should retrieve from cache instead of executing another query
    // which is why we expect try_execute_query to be called only once
    ts->get_topology(mc);
}

TEST_F(TopologyServiceTest, QueryFailure) {
    EXPECT_CALL(*mc, try_execute_query(StrEq(RETRIEVE_TOPOLOGY_SQL)))
        .WillOnce(Return(false));

    std::shared_ptr<CLUSTER_TOPOLOGY_INFO> topology = ts->get_topology(mc);

    EXPECT_EQ(nullptr, topology);
}

TEST_F(TopologyServiceTest, StaleTopology) {
    EXPECT_CALL(*mc, try_execute_query(StrEq(RETRIEVE_TOPOLOGY_SQL)))
        .Times(2)
        .WillOnce(Return(true))
        .WillOnce(Return(false));
    EXPECT_CALL(*mc, fetch_next_row())
        .WillOnce(Return(reader1))
        .WillOnce(Return(writer))
        .WillOnce(Return(reader2))
        .WillRepeatedly(Return(MYSQL_ROW{}));

    ts->set_refresh_rate(1);

    std::shared_ptr<CLUSTER_TOPOLOGY_INFO> hosts = ts->get_topology(mc);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    std::shared_ptr<CLUSTER_TOPOLOGY_INFO> stale_hosts = ts->get_topology(mc);

    EXPECT_EQ(3, stale_hosts->total_hosts());
    EXPECT_EQ(hosts, stale_hosts);
}

TEST_F(TopologyServiceTest, RefreshTopology) {
    EXPECT_CALL(*mc, try_execute_query(StrEq(RETRIEVE_TOPOLOGY_SQL)))
        .Times(2)
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*mc, fetch_next_row())
        .WillOnce(Return(reader1))
        .WillOnce(Return(writer))
        .WillOnce(Return(reader2))
        .WillRepeatedly(Return(MYSQL_ROW{}));

    ts->set_refresh_rate(1);

    ts->get_topology(mc);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    ts->get_topology(mc);
}

TEST_F(TopologyServiceTest, SharedTopology) {
    EXPECT_CALL(*mc, try_execute_query(StrEq(RETRIEVE_TOPOLOGY_SQL)))
        .WillOnce(Return(true))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*mc, fetch_next_row())
        .WillOnce(Return(reader1))
        .WillOnce(Return(writer))
        .WillOnce(Return(reader2))
        .WillRepeatedly(Return(MYSQL_ROW{}));

    TOPOLOGY_SERVICE* ts2 = new TOPOLOGY_SERVICE();
    ts2->set_cluster_id(cluster_id);

    auto topology1 = ts->get_topology(mc);
    auto topology2 = ts2->get_cached_topology();
    
    // Both topologies should come from 
    // the same underlying shared topology.
    EXPECT_NE(nullptr, topology1);
    EXPECT_NE(nullptr, topology2);
    EXPECT_EQ(topology1, topology2);

    ts->clear();

    topology1 = ts->get_cached_topology();
    topology2 = ts2->get_cached_topology();

    // Both topologies should be null because
    // the underlying shared topology got cleared.
    EXPECT_EQ(nullptr, topology1);
    EXPECT_EQ(nullptr, topology2);
}

TEST_F(TopologyServiceTest, ClearCache) {
    EXPECT_CALL(*mc, try_execute_query(StrEq(RETRIEVE_TOPOLOGY_SQL)))
        .WillOnce(Return(true))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*mc, fetch_next_row())
        .WillOnce(Return(reader1))
        .WillOnce(Return(writer))
        .WillOnce(Return(reader2))
        .WillRepeatedly(Return(MYSQL_ROW{}));

    auto topology = ts->get_topology(mc);
    EXPECT_NE(nullptr, topology);

    ts->clear_all();

    // topology should now be null after above clear_all()
    topology = ts->get_topology(mc);
    EXPECT_EQ(nullptr, topology);
}
