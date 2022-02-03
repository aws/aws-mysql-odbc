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
using ::testing::AtLeast;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::Sequence;
using ::testing::Unused;

namespace {
    const std::string HOST_SUFFIX = ".xyz.us-east-2.rds.amazonaws.com";
}  // namespace

class FailoverWriterHandlerTest : public testing::Test {
 protected:
    std::string writer_instance_name;
    std::string new_writer_instance_name;
    std::shared_ptr<HOST_INFO> writer_host;
    std::shared_ptr<HOST_INFO> new_writer_host;
    std::shared_ptr<HOST_INFO> reader_a_host;
    std::shared_ptr<HOST_INFO> reader_b_host;
    std::shared_ptr<CLUSTER_TOPOLOGY_INFO> current_topology;
    std::shared_ptr<MOCK_TOPOLOGY_SERVICE> mock_ts;
    std::shared_ptr<MOCK_READER_HANDLER> mock_reader_handler;
    std::shared_ptr<MOCK_CONNECTION_HANDLER> mock_connection_handler;

    static void SetUpTestSuite() {}

    static void TearDownTestSuite() {}

    void SetUp() override {
        writer_instance_name = "writer-host";
        new_writer_instance_name = "new-writer-host";

        writer_host = std::make_shared<HOST_INFO>(
            HOST_INFO(writer_instance_name + HOST_SUFFIX, 1234));
        writer_host->mark_as_writer(true);
        writer_host->set_host_state(DOWN);
        writer_host->instance_name = writer_instance_name;

        new_writer_host = std::make_shared<HOST_INFO>(
            HOST_INFO(new_writer_instance_name + HOST_SUFFIX, 1234));
        new_writer_host->mark_as_writer(true);
        new_writer_host->set_host_state(UP);
        new_writer_host->instance_name = new_writer_instance_name;

        reader_a_host = std::make_shared<HOST_INFO>(
            HOST_INFO("reader-a-host" + HOST_SUFFIX, 1234));
        reader_a_host->instance_name = "reader-a-host";

        reader_b_host = std::make_shared<HOST_INFO>(
            HOST_INFO("reader-b-host" + HOST_SUFFIX, 1234));
        reader_b_host->instance_name = "reader-b-host";

        current_topology = std::make_shared<CLUSTER_TOPOLOGY_INFO>();
        current_topology->add_host(writer_host);
        current_topology->add_host(reader_a_host);
        current_topology->add_host(reader_b_host);

        mock_ts = std::make_shared<MOCK_TOPOLOGY_SERVICE>();
        mock_reader_handler = std::make_shared<MOCK_READER_HANDLER>();
        mock_connection_handler = std::make_shared<MOCK_CONNECTION_HANDLER>();
    }

    void TearDown() override {}
};

// Verify that writer failover handler can re-connect to a current writer node.
// topology: no changes
// taskA: successfully re-connect to writer; return new connection
// taskB: fail to connect to any reader due to exception
// expected test result: new connection by taskA
TEST_F(FailoverWriterHandlerTest, DISABLED_ReconnectToWriter_TaskBEmptyReaderResult) {
    auto mock_connection = std::make_shared<MOCK_CONNECTION>();

    EXPECT_CALL(*mock_connection, is_connected()).WillRepeatedly(Return(true));

    EXPECT_CALL(*mock_ts, get_topology(_, true))
        .WillRepeatedly(Return(current_topology));

    Sequence s;
    EXPECT_CALL(*mock_ts, mark_host_down(writer_host)).InSequence(s);
    EXPECT_CALL(*mock_ts, mark_host_up(writer_host)).InSequence(s);

    EXPECT_CALL(*mock_reader_handler, get_reader_connection(_, _))
        .WillRepeatedly(Return(READER_FAILOVER_RESULT(false, nullptr, nullptr)));

    EXPECT_CALL(*mock_connection_handler, connect(writer_host))
        .WillRepeatedly(Return(mock_connection));
    EXPECT_CALL(*mock_connection_handler, connect(reader_a_host))
        .WillRepeatedly(Return(nullptr));
    EXPECT_CALL(*mock_connection_handler, connect(reader_b_host))
        .WillRepeatedly(Return(nullptr));

    FAILOVER_WRITER_HANDLER writer_handler(
        mock_ts, mock_reader_handler, mock_connection_handler, 5000, 2000, 2000);
    auto result = writer_handler.failover(current_topology);

    EXPECT_TRUE(result.connected);
    EXPECT_FALSE(result.is_new_host);
    EXPECT_THAT(result.new_connection, mock_connection);
}

// Verify that writer failover handler can re-connect to a current writer node.
// topology: no changes seen by task A, changes to [new-writer, reader-a,
// reader-b] for taskB
// taskA: successfully re-connect to initial writer; return new connection
// taskB: successfully connect to reader-a and then new writer but it takes more
// time than taskA
// expected test result: new connection by taskA
TEST_F(FailoverWriterHandlerTest, DISABLED_ReconnectToWriter_SlowReaderA) {
    std::shared_ptr<CONNECTION_INTERFACE> mock_writer_connection =
        std::make_shared<MOCK_CONNECTION>();
    std::shared_ptr<CONNECTION_INTERFACE> mock_reader_a_connection =
        std::make_shared<MOCK_CONNECTION>();

    auto new_topology = std::make_shared<CLUSTER_TOPOLOGY_INFO>();
    new_topology->add_host(new_writer_host);
    new_topology->add_host(reader_a_host);
    new_topology->add_host(reader_b_host);

    EXPECT_CALL(*static_cast<MOCK_CONNECTION*>(mock_writer_connection.get()),
                is_connected())
        .WillRepeatedly(Return(true));

    EXPECT_CALL(*static_cast<MOCK_CONNECTION*>(mock_reader_a_connection.get()),
                is_connected())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*static_cast<MOCK_CONNECTION*>(mock_reader_a_connection.get()),
                close_connection())
        .Times(AtLeast(1));

    EXPECT_CALL(*mock_connection_handler, connect(writer_host))
        .WillRepeatedly(Return(mock_writer_connection));
    EXPECT_CALL(*mock_connection_handler, connect(reader_b_host))
        .WillRepeatedly(Return(nullptr));

    EXPECT_CALL(*mock_ts, get_topology(mock_writer_connection.get(), true))
        .WillRepeatedly(Return(current_topology));
    EXPECT_CALL(*mock_ts, get_topology(mock_reader_a_connection.get(), true))
        .WillRepeatedly(Return(new_topology));

    Sequence s;
    EXPECT_CALL(*mock_ts, mark_host_down(writer_host)).InSequence(s);
    EXPECT_CALL(*mock_ts, mark_host_up(writer_host)).InSequence(s);

    EXPECT_CALL(*mock_reader_handler, get_reader_connection(_, _))
        .WillRepeatedly(DoAll(
            Invoke([](Unused, const std::function<bool()> is_canceled) {
                for (int i = 0; i <= 50 && !is_canceled(); i++) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            }),
            Return(READER_FAILOVER_RESULT(true, reader_a_host,
                                          mock_reader_a_connection))));

    FAILOVER_WRITER_HANDLER writer_handler(
        mock_ts, mock_reader_handler, mock_connection_handler, 60000, 5000, 5000);
    auto result = writer_handler.failover(current_topology);

    EXPECT_TRUE(result.connected);
    EXPECT_FALSE(result.is_new_host);
    EXPECT_THAT(result.new_connection, mock_writer_connection);
}

// Verify that writer failover handler can re-connect to a current writer node.
// topology: no changes
// taskA: successfully re-connect to writer; return new connection
// taskB: successfully connect to readerA and retrieve topology, but latest
// writer is not new (defer to taskA)
// expected test result: new connection by taskA
TEST_F(FailoverWriterHandlerTest, DISABLED_ReconnectToWriter_TaskBDefers) {
    std::shared_ptr<CONNECTION_INTERFACE> mock_writer_connection =
        std::make_shared<MOCK_CONNECTION>();
    std::shared_ptr<CONNECTION_INTERFACE> mock_reader_a_connection =
        std::make_shared<MOCK_CONNECTION>();

    EXPECT_CALL(*static_cast<MOCK_CONNECTION*>(mock_writer_connection.get()),
                is_connected())
        .WillRepeatedly(Return(true));

    EXPECT_CALL(*static_cast<MOCK_CONNECTION*>(mock_reader_a_connection.get()),
                is_connected())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*static_cast<MOCK_CONNECTION*>(mock_reader_a_connection.get()),
                close_connection())
        .Times(AtLeast(1));

    EXPECT_CALL(*mock_connection_handler, connect(writer_host))
        .WillRepeatedly(Invoke([&]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(5000));
            return mock_writer_connection;
        }));
    EXPECT_CALL(*mock_connection_handler, connect(reader_b_host))
        .WillRepeatedly(Return(nullptr));

    EXPECT_CALL(*mock_ts, get_topology(_, true))
        .WillRepeatedly(Return(current_topology));

    Sequence s;
    EXPECT_CALL(*mock_ts, mark_host_down(writer_host)).InSequence(s);
    EXPECT_CALL(*mock_ts, mark_host_up(writer_host)).InSequence(s);

    EXPECT_CALL(*mock_reader_handler, get_reader_connection(_, _))
        .WillRepeatedly(Return(READER_FAILOVER_RESULT(true, reader_a_host,
                                                    mock_reader_a_connection)));

    FAILOVER_WRITER_HANDLER writer_handler(
        mock_ts, mock_reader_handler, mock_connection_handler, 60000, 2000, 2000);
    auto result = writer_handler.failover(current_topology);

    EXPECT_TRUE(result.connected);
    EXPECT_FALSE(result.is_new_host);
    EXPECT_THAT(result.new_connection, mock_writer_connection);
}

// Verify that writer failover handler can re-connect to a new writer node.
// topology: changes to [new-writer, reader-A, reader-B] for taskB, taskA sees
// no changes
// taskA: successfully re-connect to writer; return connection to initial writer
// but it takes more time than taskB
// taskB: successfully connect to readerA and then to new-writer
// expected test result: new connection to writer by taskB
TEST_F(FailoverWriterHandlerTest, DISABLED_ConnectToReaderA_SlowWriter) {
    std::shared_ptr<CONNECTION_INTERFACE> mock_writer_connection =
        std::make_shared<MOCK_CONNECTION>();
    std::shared_ptr<CONNECTION_INTERFACE> mock_new_writer_connection =
        std::make_shared<MOCK_CONNECTION>();
    std::shared_ptr<CONNECTION_INTERFACE> mock_reader_a_connection =
        std::make_shared<MOCK_CONNECTION>();
    std::shared_ptr<CONNECTION_INTERFACE> mock_reader_b_connection =
        std::make_shared<MOCK_CONNECTION>();

    auto new_topology = std::make_shared<CLUSTER_TOPOLOGY_INFO>();
    new_topology->add_host(new_writer_host);
    new_topology->add_host(reader_a_host);
    new_topology->add_host(reader_b_host);

    EXPECT_CALL(*static_cast<MOCK_CONNECTION*>(mock_writer_connection.get()),
                is_connected())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*static_cast<MOCK_CONNECTION*>(mock_writer_connection.get()),
                close_connection())
        .Times(1);

    EXPECT_CALL(*static_cast<MOCK_CONNECTION*>(mock_new_writer_connection.get()),
                is_connected())
        .WillRepeatedly(Return(true));

    EXPECT_CALL(*static_cast<MOCK_CONNECTION*>(mock_reader_a_connection.get()),
                is_connected())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*static_cast<MOCK_CONNECTION*>(mock_reader_a_connection.get()),
                close_connection())
        .Times(1);

    EXPECT_CALL(*mock_connection_handler, connect(writer_host))
        .WillRepeatedly(Invoke([&]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(5000));
            return mock_writer_connection;
        }));
    EXPECT_CALL(*mock_connection_handler, connect(reader_a_host))
        .WillRepeatedly(Return(mock_reader_a_connection));
    EXPECT_CALL(*mock_connection_handler, connect(reader_b_host))
        .WillRepeatedly(Return(mock_reader_b_connection));
    EXPECT_CALL(*mock_connection_handler, connect(new_writer_host))
        .WillRepeatedly(Return(mock_new_writer_connection));

    EXPECT_CALL(*mock_ts, get_topology(mock_writer_connection.get(), true))
        .WillRepeatedly(Return(current_topology));
    EXPECT_CALL(*mock_ts, get_topology(mock_reader_a_connection.get(), true))
        .WillRepeatedly(Return(new_topology));
    EXPECT_CALL(*mock_ts, mark_host_down(writer_host)).Times(1);
    EXPECT_CALL(*mock_ts, mark_host_up(_)).Times(AnyNumber());
    EXPECT_CALL(*mock_ts, mark_host_up(new_writer_host)).Times(1);

    EXPECT_CALL(*mock_reader_handler, get_reader_connection(_, _))
        .WillRepeatedly(Return(READER_FAILOVER_RESULT(true, reader_a_host,
                                                    mock_reader_a_connection)));

    FAILOVER_WRITER_HANDLER writer_handler(
        mock_ts, mock_reader_handler, mock_connection_handler, 60000, 5000, 5000);
    auto result = writer_handler.failover(current_topology);

    EXPECT_TRUE(result.connected);
    EXPECT_TRUE(result.is_new_host);
    EXPECT_THAT(result.new_connection, mock_new_writer_connection);
    EXPECT_EQ(3, result.new_topology->total_hosts());
    EXPECT_EQ(new_writer_instance_name,
            result.new_topology->get_writer()->instance_name);
}

// Verify that writer failover handler can re-connect to a new writer node.
// topology: changes to [new-writer, initial-writer, reader-A, reader-B] 
// taskA: successfully reconnect, but initial-writer is now a reader (defer to taskB) 
// taskB: successfully connect to readerA and then to new-writer 
// expected test result: new connection to writer by taskB
TEST_F(FailoverWriterHandlerTest, DISABLED_ConnectToReaderA_TaskADefers) {
    std::shared_ptr<CONNECTION_INTERFACE> mock_writer_connection =
        std::make_shared<MOCK_CONNECTION>();
    std::shared_ptr<CONNECTION_INTERFACE> mock_new_writer_connection =
        std::make_shared<MOCK_CONNECTION>();
    std::shared_ptr<CONNECTION_INTERFACE> mock_reader_a_connection =
        std::make_shared<MOCK_CONNECTION>();
    std::shared_ptr<CONNECTION_INTERFACE> mock_reader_b_connection =
        std::make_shared<MOCK_CONNECTION>();

    auto new_topology = std::make_shared<CLUSTER_TOPOLOGY_INFO>();
    new_topology->add_host(new_writer_host);
    new_topology->add_host(writer_host);
    new_topology->add_host(reader_a_host);
    new_topology->add_host(reader_b_host);

    EXPECT_CALL(*static_cast<MOCK_CONNECTION*>(mock_writer_connection.get()),
                is_connected())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*static_cast<MOCK_CONNECTION*>(mock_writer_connection.get()),
                close_connection())
        .Times(AtLeast(1));

    EXPECT_CALL(*static_cast<MOCK_CONNECTION*>(mock_new_writer_connection.get()),
                is_connected())
        .WillRepeatedly(Return(true));

    EXPECT_CALL(*static_cast<MOCK_CONNECTION*>(mock_reader_a_connection.get()),
                is_connected())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*static_cast<MOCK_CONNECTION*>(mock_reader_a_connection.get()),
                close_connection())
        .Times(1);

    EXPECT_CALL(*static_cast<MOCK_CONNECTION*>(mock_reader_b_connection.get()),
                is_connected())
        .WillRepeatedly(Return(true));

    EXPECT_CALL(*mock_connection_handler, connect(writer_host))
        .WillRepeatedly(Return(mock_writer_connection));
    EXPECT_CALL(*mock_connection_handler, connect(reader_a_host))
        .WillRepeatedly(Return(mock_reader_a_connection));
    EXPECT_CALL(*mock_connection_handler, connect(reader_b_host))
        .WillRepeatedly(Return(mock_reader_b_connection));
    EXPECT_CALL(*mock_connection_handler, connect(new_writer_host))
        .WillRepeatedly(Invoke([&]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(5000));
            return mock_new_writer_connection;
        }));

    EXPECT_CALL(*mock_ts, get_topology(_, true))
        .WillRepeatedly(Return(new_topology));
    EXPECT_CALL(*mock_ts, mark_host_down(writer_host)).Times(1);
    EXPECT_CALL(*mock_ts, mark_host_up(_)).Times(AnyNumber());
    EXPECT_CALL(*mock_ts, mark_host_up(new_writer_host)).Times(1);

    EXPECT_CALL(*mock_reader_handler, get_reader_connection(_, _))
        .WillRepeatedly(Return(READER_FAILOVER_RESULT(true, reader_a_host,
                                                    mock_reader_a_connection)));

    FAILOVER_WRITER_HANDLER writer_handler(
        mock_ts, mock_reader_handler, mock_connection_handler, 60000, 5000, 5000);
    auto result = writer_handler.failover(current_topology);

    EXPECT_TRUE(result.connected);
    EXPECT_TRUE(result.is_new_host);
    EXPECT_THAT(result.new_connection, mock_new_writer_connection);
    EXPECT_EQ(4, result.new_topology->total_hosts());
    EXPECT_EQ(new_writer_instance_name,
            result.new_topology->get_writer()->instance_name);
}

// Verify that writer failover handler fails to re-connect to any writer node.
// topology: no changes seen by task A, changes to [new-writer, reader-A, reader-B] for taskB
// taskA: fail to re-connect to writer due to failover timeout
// taskB: successfully connect to readerA and then fail to connect to writer due to failover timeout
// expected test result: no connection
TEST_F(FailoverWriterHandlerTest, FailedToConnect_FailoverTimeout) {
    std::shared_ptr<CONNECTION_INTERFACE> mock_writer_connection =
        std::make_shared<MOCK_CONNECTION>();
    std::shared_ptr<CONNECTION_INTERFACE> mock_new_writer_connection =
        std::make_shared<MOCK_CONNECTION>();
    std::shared_ptr<CONNECTION_INTERFACE> mock_reader_a_connection =
        std::make_shared<MOCK_CONNECTION>();
    std::shared_ptr<CONNECTION_INTERFACE> mock_reader_b_connection =
        std::make_shared<MOCK_CONNECTION>();

    auto new_topology = std::make_shared<CLUSTER_TOPOLOGY_INFO>();
    new_topology->add_host(new_writer_host);
    new_topology->add_host(reader_a_host);
    new_topology->add_host(reader_b_host);

    EXPECT_CALL(*static_cast<MOCK_CONNECTION*>(mock_writer_connection.get()),
                is_connected())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*static_cast<MOCK_CONNECTION*>(mock_writer_connection.get()),
                close_connection())
        .Times(1);

    EXPECT_CALL(*static_cast<MOCK_CONNECTION*>(mock_new_writer_connection.get()),
                is_connected())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*static_cast<MOCK_CONNECTION*>(mock_new_writer_connection.get()),
                close_connection())
        .Times(1);

    EXPECT_CALL(*static_cast<MOCK_CONNECTION*>(mock_reader_a_connection.get()),
                is_connected())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*static_cast<MOCK_CONNECTION*>(mock_reader_a_connection.get()),
                close_connection())
        .Times(AtLeast(1));

    EXPECT_CALL(*static_cast<MOCK_CONNECTION*>(mock_reader_b_connection.get()),
                is_connected())
        .WillRepeatedly(Return(true));

    EXPECT_CALL(*mock_connection_handler, connect(writer_host))
        .WillRepeatedly(Invoke([&]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(5000));
            return mock_writer_connection;
        }));
    EXPECT_CALL(*mock_connection_handler, connect(reader_a_host))
        .WillRepeatedly(Return(mock_reader_a_connection));
    EXPECT_CALL(*mock_connection_handler, connect(reader_b_host))
        .WillRepeatedly(Return(mock_reader_b_connection));
    EXPECT_CALL(*mock_connection_handler, connect(new_writer_host))
        .WillRepeatedly(Invoke([&]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(5000));
            return mock_new_writer_connection;
        }));

    EXPECT_CALL(*mock_ts, get_topology(mock_writer_connection.get(), _))
        .WillRepeatedly(Return(current_topology));
    EXPECT_CALL(*mock_ts, get_topology(mock_reader_a_connection.get(), _))
        .WillRepeatedly(Return(new_topology));
    EXPECT_CALL(*mock_ts, mark_host_down(writer_host)).Times(1);
    EXPECT_CALL(*mock_ts, mark_host_up(writer_host)).Times(1);
    EXPECT_CALL(*mock_ts, mark_host_up(new_writer_host)).Times(1);

    EXPECT_CALL(*mock_reader_handler, get_reader_connection(_, _))
        .WillRepeatedly(Return(READER_FAILOVER_RESULT(true, reader_a_host,
                                                    mock_reader_a_connection)));

    FAILOVER_WRITER_HANDLER writer_handler(
        mock_ts, mock_reader_handler, mock_connection_handler, 1000, 2000, 2000);
    auto result = writer_handler.failover(current_topology);

    EXPECT_FALSE(result.connected);
    EXPECT_FALSE(result.is_new_host);
    EXPECT_THAT(result.new_connection, nullptr);
}

// Verify that writer failover handler fails to re-connect to any writer node.
// topology: changes to [new-writer, reader-A, reader-B] for taskB
// taskA: fail to re-connect to writer
// taskB: successfully connect to readerA and then fail to connect to writer
// expected test result: no connection
TEST_F(FailoverWriterHandlerTest, FailedToConnect_TaskAFailed_TaskBWriterFailed) {
    std::shared_ptr<CONNECTION_INTERFACE> mock_reader_a_connection =
        std::make_shared<MOCK_CONNECTION>();
    std::shared_ptr<CONNECTION_INTERFACE> mock_reader_b_connection =
        std::make_shared<MOCK_CONNECTION>();

    auto new_topology = std::make_shared<CLUSTER_TOPOLOGY_INFO>();
    new_topology->add_host(new_writer_host);
    new_topology->add_host(reader_a_host);
    new_topology->add_host(reader_b_host);

    EXPECT_CALL(*static_cast<MOCK_CONNECTION*>(mock_reader_a_connection.get()),
                is_connected())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*static_cast<MOCK_CONNECTION*>(mock_reader_a_connection.get()),
                close_connection())
        .Times(AtLeast(1));

    EXPECT_CALL(*static_cast<MOCK_CONNECTION*>(mock_reader_b_connection.get()),
                is_connected())
        .WillRepeatedly(Return(true));

    EXPECT_CALL(*mock_connection_handler, connect(writer_host))
        .WillRepeatedly(Return(nullptr));
    EXPECT_CALL(*mock_connection_handler, connect(reader_a_host))
        .WillRepeatedly(Return(mock_reader_a_connection));
    EXPECT_CALL(*mock_connection_handler, connect(reader_b_host))
        .WillRepeatedly(Return(mock_reader_b_connection));
    EXPECT_CALL(*mock_connection_handler, connect(new_writer_host))
        .WillRepeatedly(Return(nullptr));

    EXPECT_CALL(*mock_ts, get_topology(_, _))
        .WillRepeatedly(Return(new_topology));
    EXPECT_CALL(*mock_ts, mark_host_down(writer_host)).Times(1);
    EXPECT_CALL(*mock_ts, mark_host_down(new_writer_host)).Times(AtLeast(1));

    EXPECT_CALL(*mock_reader_handler, get_reader_connection(_, _))
        .WillRepeatedly(Return(READER_FAILOVER_RESULT(true, reader_a_host,
                                                    mock_reader_a_connection)));

    FAILOVER_WRITER_HANDLER writer_handler(
        mock_ts, mock_reader_handler, mock_connection_handler, 5000, 2000, 2000);
    auto result = writer_handler.failover(current_topology);

    EXPECT_FALSE(result.connected);
    EXPECT_FALSE(result.is_new_host);
    EXPECT_THAT(result.new_connection, nullptr);
}
