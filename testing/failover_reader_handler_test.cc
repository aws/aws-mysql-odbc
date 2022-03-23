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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <thread>

#include "mock_objects.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::Return;

namespace {
    const std::string HOST_SUFFIX = ".xyz.us-east-2.rds.amazonaws.com";
}  // namespace

class FailoverReaderHandlerTest : public testing::Test {
protected:
    static std::shared_ptr<HOST_INFO> reader_a_host;
    static std::shared_ptr<HOST_INFO> reader_b_host;
    static std::shared_ptr<HOST_INFO> reader_c_host;
    static std::shared_ptr<HOST_INFO> writer_host;

    MOCK_CONNECTION* mock_reader_a_connection;
    MOCK_CONNECTION* mock_reader_b_connection;
    MOCK_CONNECTION* mock_reader_c_connection;
    MOCK_CONNECTION* mock_writer_connection;

    static std::shared_ptr<CLUSTER_TOPOLOGY_INFO> topology;
    
    std::shared_ptr<MOCK_TOPOLOGY_SERVICE> mock_ts;
    std::shared_ptr<MOCK_CONNECTION_HANDLER> mock_connection_handler;
    MOCK_FAILOVER_SYNC mock_sync;

    static void SetUpTestSuite() { 
        reader_a_host = std::make_shared<HOST_INFO>("reader-a-host" + HOST_SUFFIX, 1234, UP, false);
        reader_a_host->instance_name = "reader-a-host";
        
        reader_b_host = std::make_shared<HOST_INFO>("reader-b-host" + HOST_SUFFIX, 1234, UP, false);
        reader_b_host->instance_name = "reader-b-host";

        reader_c_host = std::make_shared<HOST_INFO>("reader-c-host" + HOST_SUFFIX, 1234, DOWN, false);
        reader_c_host->instance_name = "reader-c-host";
        
        writer_host = std::make_shared<HOST_INFO>("writer-host" + HOST_SUFFIX, 1234, UP, true);
        writer_host->instance_name = "writer-host";

        topology = std::make_shared<CLUSTER_TOPOLOGY_INFO>();
        topology->add_host(writer_host);
        topology->add_host(reader_a_host);
        topology->add_host(reader_b_host);
        topology->add_host(reader_c_host);
    
    }

    static void TearDownTestSuite() { 
        reader_a_host.reset();
        reader_b_host.reset();
        reader_c_host.reset();
        writer_host.reset();

        topology.reset();
    }

    void SetUp() override {
        reader_a_host->set_host_state(UP);
        reader_b_host->set_host_state(UP);
        reader_c_host->set_host_state(DOWN);
        writer_host->set_host_state(UP);

        mock_ts = std::make_shared<MOCK_TOPOLOGY_SERVICE>();
        mock_connection_handler = std::make_shared<MOCK_CONNECTION_HANDLER>();
        EXPECT_CALL(mock_sync, is_completed()).WillRepeatedly(Return(false));
    }

    void TearDown() override {
        mock_ts.reset();
        mock_connection_handler.reset();
    }
};

std::shared_ptr<HOST_INFO> FailoverReaderHandlerTest::reader_a_host = nullptr;
std::shared_ptr<HOST_INFO> FailoverReaderHandlerTest::reader_b_host = nullptr;
std::shared_ptr<HOST_INFO> FailoverReaderHandlerTest::reader_c_host = nullptr;
std::shared_ptr<HOST_INFO> FailoverReaderHandlerTest::writer_host = nullptr;

std::shared_ptr<CLUSTER_TOPOLOGY_INFO> FailoverReaderHandlerTest::topology = nullptr;

// Helper function that generates a list of hosts.
// Parameters: Number of reader hosts UP, number of reader hosts DOWN, number of writer hosts.
CLUSTER_TOPOLOGY_INFO generate_topology(int readers_up, int readers_down, int writers) {
  CLUSTER_TOPOLOGY_INFO topology = CLUSTER_TOPOLOGY_INFO();
  int number = 0;
  for (int i = 0; i < readers_up; i++) {
    std::shared_ptr<HOST_INFO> reader_up = std::make_shared<HOST_INFO>("host" + number, 0123, UP, false);
    topology.add_host(reader_up);
    number++;
  }
  for (int i = 0; i < readers_down; i++) {
    std::shared_ptr<HOST_INFO> reader_down = std::make_shared<HOST_INFO>("host" + number, 0123, DOWN, false);
    topology.add_host(reader_down);
    number++;
  }
  for (int i = 0; i < writers; i++) {
    std::shared_ptr<HOST_INFO> writer = std::make_shared<HOST_INFO>("host" + number, 0123, UP, true);
    topology.add_host(writer);
    number++;
  }
  return topology;
}

// Unit tests for the function that generates the list of hosts based on number of hosts.
TEST_F(FailoverReaderHandlerTest, GenerateTopology) {
  CLUSTER_TOPOLOGY_INFO topology_info;

  topology_info = generate_topology(0, 0, 0);
  EXPECT_EQ(0, topology_info.total_hosts());

  topology_info = generate_topology(0, 0, 1);
  EXPECT_EQ(1, topology_info.total_hosts());

  topology_info = generate_topology(1, 0, 1);
  EXPECT_EQ(2, topology_info.total_hosts());

  topology_info = generate_topology(1, 1, 2);
  EXPECT_EQ(4, topology_info.total_hosts());
}

TEST_F(FailoverReaderHandlerTest, BuildHostsList) {
    FAILOVER_READER_HANDLER reader_handler(mock_ts, mock_connection_handler, 60000, 30000, nullptr, 0);
    std::shared_ptr<CLUSTER_TOPOLOGY_INFO> topology;
    std::vector<std::shared_ptr<HOST_INFO>> hosts_list;

    //Case: 0 readers up, 0 readers down, 0 writers, writers included.
    topology = std::make_shared<CLUSTER_TOPOLOGY_INFO>(generate_topology(0, 0, 0));
    hosts_list = reader_handler.build_hosts_list(topology, true);
    EXPECT_EQ(0, hosts_list.size());

    //Case: 0 readers up, 0 readers down, 1 writers, writers included.
    topology = std::make_shared<CLUSTER_TOPOLOGY_INFO>(generate_topology(0, 0, 1));
    hosts_list = reader_handler.build_hosts_list(topology, true);
    EXPECT_EQ(1, hosts_list.size());
    EXPECT_TRUE(hosts_list[0]->is_host_writer());

    //Case: 0 readers up, 0 readers down, 1 writers, writers excluded.
    topology = std::make_shared<CLUSTER_TOPOLOGY_INFO>(generate_topology(0, 0, 1));
    hosts_list = reader_handler.build_hosts_list(topology, false);
    EXPECT_EQ(0, hosts_list.size());

    //Case: 1 readers up, 1 readers down, 0 writers, writers included.
    topology = std::make_shared<CLUSTER_TOPOLOGY_INFO>(generate_topology(0, 1, 0));
    hosts_list = reader_handler.build_hosts_list(topology, true);
    EXPECT_EQ(1, hosts_list.size());
    EXPECT_FALSE(hosts_list[0]->is_host_writer());
    EXPECT_FALSE(hosts_list[0]->is_host_up());

    //Case: 1 readers up, 0 readers down, 0 writers, writers included.
    topology = std::make_shared<CLUSTER_TOPOLOGY_INFO>(generate_topology(1, 0, 0));
    hosts_list = reader_handler.build_hosts_list(topology, true);
    EXPECT_EQ(1, hosts_list.size());
    EXPECT_FALSE(hosts_list[0]->is_host_writer());
    EXPECT_TRUE(hosts_list[0]->is_host_up());

    //Case: 1 readers up, 0 readers down, 1 writers, writers excluded.
    topology = std::make_shared<CLUSTER_TOPOLOGY_INFO>(generate_topology(1, 0, 1));
    hosts_list = reader_handler.build_hosts_list(topology, false);
    EXPECT_EQ(1, hosts_list.size());
    EXPECT_FALSE(hosts_list[0]->is_host_writer());

    //Case: 1 readers up, 1 readers down, 1 writers, writers excluded.
    topology = std::make_shared<CLUSTER_TOPOLOGY_INFO>(generate_topology(1, 1, 2));
    hosts_list = reader_handler.build_hosts_list(topology, false);
    EXPECT_EQ(2, hosts_list.size());
    EXPECT_FALSE(hosts_list[0]->is_host_writer());
    EXPECT_TRUE(hosts_list[0]->is_host_up());
    EXPECT_FALSE(hosts_list[1]->is_host_writer());
    EXPECT_TRUE(hosts_list[1]->is_host_down());

    //Case: 1 readers up, 1 readers down, 1 writers, writers included.
    topology = std::make_shared<CLUSTER_TOPOLOGY_INFO>(generate_topology(1, 1, 1));
    hosts_list = reader_handler.build_hosts_list(topology, true);
    EXPECT_EQ(3, hosts_list.size());
    EXPECT_FALSE(hosts_list[0]->is_host_writer());
    EXPECT_TRUE(hosts_list[0]->is_host_up());
    EXPECT_FALSE(hosts_list[1]->is_host_writer());
    EXPECT_TRUE(hosts_list[1]->is_host_down());
    EXPECT_TRUE(hosts_list[2]->is_host_writer());
}

// Verify that reader failover handler fails to connect to any reader node or writer node.
// Expected result: no new connection
TEST_F(FailoverReaderHandlerTest, GetConnectionFromHosts_Failure) {
    EXPECT_CALL(*mock_ts, get_topology(_, true)).WillRepeatedly(Return(topology));
    
    EXPECT_CALL(*mock_connection_handler, connect(_)).WillRepeatedly(Return(nullptr));

    EXPECT_CALL(*mock_ts, mark_host_down(reader_a_host)).Times(1);
    EXPECT_CALL(*mock_ts, mark_host_down(reader_b_host)).Times(1);
    EXPECT_CALL(*mock_ts, mark_host_down(reader_c_host)).Times(1);
    EXPECT_CALL(*mock_ts, mark_host_down(writer_host)).Times(1);

    FAILOVER_READER_HANDLER reader_handler(mock_ts, mock_connection_handler, 60000, 30000, nullptr, 0);
    auto hosts_list = reader_handler.build_hosts_list(topology, true);
    READER_FAILOVER_RESULT result = reader_handler.get_connection_from_hosts(hosts_list, std::ref(mock_sync));

    EXPECT_FALSE(result.connected);
    EXPECT_THAT(result.new_connection, nullptr);
}

// Verify that reader failover handler connects to a reader node that is marked up.
// Expected result: new connection to reader A
TEST_F(FailoverReaderHandlerTest, GetConnectionFromHosts_Success_Reader) {
    mock_reader_a_connection = new MOCK_CONNECTION();

    EXPECT_CALL(*mock_ts, get_topology(_, true)).WillRepeatedly(Return(topology));

    EXPECT_CALL(*mock_reader_a_connection, is_connected()).WillRepeatedly(Return(true));

    EXPECT_CALL(*mock_connection_handler, connect(_)).WillRepeatedly(Return(nullptr));    
    EXPECT_CALL(*mock_connection_handler, connect(reader_a_host)).WillRepeatedly(Return(mock_reader_a_connection));

    // Reader C will not be used as it is put at the end. Will only try to connect to A and B
    EXPECT_CALL(*mock_ts, mark_host_up(reader_a_host)).Times(1);

    FAILOVER_READER_HANDLER reader_handler(mock_ts, mock_connection_handler, 60000, 30000, nullptr, 0);
    auto hosts_list = reader_handler.build_hosts_list(topology, true);
    READER_FAILOVER_RESULT result = reader_handler.get_connection_from_hosts(hosts_list, std::ref(mock_sync));

    EXPECT_TRUE(result.connected);
    EXPECT_THAT(result.new_connection, mock_reader_a_connection);
    EXPECT_FALSE(result.new_host->is_host_writer());

    // Explicit delete on reader A as it is returned as valid connection/result
    delete mock_reader_a_connection;
}

// Verify that reader failover handler connects to a writer node.
// Expected result: new connection to writer
TEST_F(FailoverReaderHandlerTest, GetConnectionFromHosts_Success_Writer) {
    mock_writer_connection = new MOCK_CONNECTION();

    EXPECT_CALL(*mock_ts, get_topology(_, true)).WillRepeatedly(Return(topology));

    EXPECT_CALL(*mock_writer_connection, is_connected()).WillRepeatedly(Return(true));

    EXPECT_CALL(*mock_connection_handler, connect(_)).WillRepeatedly(Return(nullptr));
    EXPECT_CALL(*mock_connection_handler, connect(writer_host)).WillRepeatedly(Return(mock_writer_connection));

    EXPECT_CALL(*mock_ts, mark_host_up(writer_host)).Times(1);

    FAILOVER_READER_HANDLER reader_handler(mock_ts, mock_connection_handler, 60000, 30000, nullptr, 0);
    auto hosts_list = reader_handler.build_hosts_list(topology, true);
    READER_FAILOVER_RESULT result = reader_handler.get_connection_from_hosts(hosts_list, std::ref(mock_sync));

    EXPECT_TRUE(result.connected);
    EXPECT_THAT(result.new_connection, mock_writer_connection);
    EXPECT_TRUE(result.new_host->is_host_writer());

    // Explicit delete as it is returned as result & is not deconstructed during failover
    delete mock_writer_connection; 
}

// Verify that reader failover handler connects to the fastest reader node available that is marked up.
// Expected result: new connection to reader A
TEST_F(FailoverReaderHandlerTest, GetConnectionFromHosts_FastestHost) {
    mock_reader_a_connection = new MOCK_CONNECTION();
    mock_reader_b_connection = new MOCK_CONNECTION(); // Will be free'd during failover as it is slower

    // May not have actually connected during failover
    // Cannot delete at the end as it may cause double delete
    Mock::AllowLeak(mock_reader_b_connection);

    EXPECT_CALL(*mock_ts, get_topology(_, true)).WillRepeatedly(Return(topology));

    EXPECT_CALL(*mock_ts, get_topology(_, true)).WillRepeatedly(Return(topology));
    // Reader C will not be used as it is put at the end. Will only try to connect to A and B
    EXPECT_CALL(*mock_reader_a_connection, is_connected()).WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_reader_b_connection, is_connected()).WillRepeatedly(Return(true));

    EXPECT_CALL(*mock_connection_handler, connect(_)).WillRepeatedly(Return(nullptr));
    EXPECT_CALL(*mock_connection_handler, connect(reader_a_host)).WillRepeatedly(Return(mock_reader_a_connection));
    EXPECT_CALL(*mock_connection_handler, connect(reader_b_host)).WillRepeatedly(Invoke([&]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(5000));
            return mock_reader_b_connection;
        }));

    FAILOVER_READER_HANDLER reader_handler(mock_ts, mock_connection_handler, 60000, 30000, nullptr, 0);
    auto hosts_list = reader_handler.build_hosts_list(topology, true);
    READER_FAILOVER_RESULT result = reader_handler.get_connection_from_hosts(hosts_list, std::ref(mock_sync));

    EXPECT_TRUE(result.connected);
    EXPECT_THAT(result.new_connection, mock_reader_a_connection);
    EXPECT_FALSE(result.new_host->is_host_writer());

    // Explicit delete on reader A as it is returned as a valid result
    delete mock_reader_a_connection;
}

// Verify that reader failover handler fails to connect when a host fails to connect before timeout.
// Expected result: no connection
TEST_F(FailoverReaderHandlerTest, GetConnectionFromHosts_Timeout) {
    // Connections should automatically free inside failover
    mock_reader_a_connection = new MOCK_CONNECTION(); 
    mock_reader_b_connection = new MOCK_CONNECTION();

    EXPECT_CALL(*mock_ts, get_topology(_, true)).WillRepeatedly(Return(topology));

    EXPECT_CALL(*mock_reader_a_connection, is_connected()).WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_reader_a_connection, mock_connection_destructor());
    EXPECT_CALL(*mock_reader_b_connection, is_connected()).WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_reader_b_connection, mock_connection_destructor());
    
    EXPECT_CALL(*mock_connection_handler, connect(_)).WillRepeatedly(Return(nullptr));
    EXPECT_CALL(*mock_connection_handler, connect(reader_a_host)).WillRepeatedly(Invoke([&]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(5000));
            return mock_reader_a_connection;
        }));
    EXPECT_CALL(*mock_connection_handler, connect(reader_b_host)).WillRepeatedly(Invoke([&]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(5000));
            return mock_reader_b_connection;
        }));

    EXPECT_CALL(*mock_ts, mark_host_down(_)).Times(AnyNumber());
    EXPECT_CALL(*mock_ts, mark_host_down(writer_host)).Times(1);

    FAILOVER_READER_HANDLER reader_handler(mock_ts, mock_connection_handler, 60000, 1000, nullptr, 0);
    auto hosts_list = reader_handler.build_hosts_list(topology, true);
    READER_FAILOVER_RESULT result = reader_handler.get_connection_from_hosts(hosts_list, std::ref(mock_sync));

    EXPECT_FALSE(result.connected);
    EXPECT_THAT(result.new_connection, nullptr);
}

// Verify that reader failover handler fails to connect to any reader node or
// writer node. Expected result: no new connection
TEST_F(FailoverReaderHandlerTest, Failover_Failure) {
    EXPECT_CALL(*mock_ts, get_topology(_, true)).WillRepeatedly(Return(topology));

    EXPECT_CALL(*mock_connection_handler, connect(_)).WillRepeatedly(Return(nullptr));

    EXPECT_CALL(*mock_ts, mark_host_down(reader_a_host)).Times(1);
    EXPECT_CALL(*mock_ts, mark_host_down(reader_b_host)).Times(1);
    EXPECT_CALL(*mock_ts, mark_host_down(reader_c_host)).Times(1);
    EXPECT_CALL(*mock_ts, mark_host_down(writer_host)).Times(1);

    FAILOVER_READER_HANDLER reader_handler(mock_ts, mock_connection_handler, 3000, 1000, nullptr, 0);
    READER_FAILOVER_RESULT result = reader_handler.failover(topology);

    EXPECT_FALSE(result.connected);
    EXPECT_THAT(result.new_connection, nullptr);
}

// Verify that reader failover handler connects to a faster reader node.
// Expected result: new connection to reader A
TEST_F(FailoverReaderHandlerTest, Failover_Success_Reader) {
    mock_reader_a_connection = new MOCK_CONNECTION();
    mock_reader_b_connection = new MOCK_CONNECTION(); // Will be free'd during failover to timeout / too slow

    // May not have actually connected during failover
    // Cannot delete at the end as it may cause double delete
    Mock::AllowLeak(mock_reader_b_connection);

    auto current_topology = std::make_shared<CLUSTER_TOPOLOGY_INFO>();
    current_topology->add_host(writer_host);
    current_topology->add_host(reader_a_host);
    current_topology->add_host(reader_b_host);
    EXPECT_CALL(*mock_ts, get_topology(_, true)).WillRepeatedly(Return(current_topology));

    EXPECT_CALL(*mock_reader_a_connection, is_connected()).WillRepeatedly(Return(true));

    EXPECT_CALL(*mock_reader_b_connection, is_connected()).WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_reader_b_connection, mock_connection_destructor());

    EXPECT_CALL(*mock_connection_handler, connect(_)).WillRepeatedly(Return(nullptr));
    EXPECT_CALL(*mock_connection_handler, connect(reader_a_host)).WillRepeatedly(
        Return(mock_reader_a_connection));
    EXPECT_CALL(*mock_connection_handler, connect(reader_b_host)).WillRepeatedly(Invoke([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(5000));
        return mock_reader_b_connection;
    }));

    FAILOVER_READER_HANDLER reader_handler(mock_ts, mock_connection_handler, 60000, 30000, nullptr, 0);
    READER_FAILOVER_RESULT result = reader_handler.failover(current_topology);

    EXPECT_TRUE(result.connected);
    EXPECT_THAT(result.new_connection, mock_reader_a_connection);
    EXPECT_FALSE(result.new_host->is_host_writer());
    EXPECT_EQ("reader-a-host", result.new_host->instance_name);

    // Explicit delete on reader A as it's returned as a valid result
    delete mock_reader_a_connection;
}
