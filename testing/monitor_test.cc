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

#include "driver/monitor.h"
#include "driver/driver.h"

#include "mock_objects.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::_;
using ::testing::Return;

#define FAILURE_DETECTION_TIME_MS std::chrono::milliseconds(10)
#define FAILURE_DETECTION_SHORT_INTERVAL_MS std::chrono::milliseconds(30)
#define FAILURE_DETECTION_LONG_INTERVAL_MS std::chrono::milliseconds(300)
#define FAILURE_DETECTION_COUNT 3
#define VALIDATION_INTERVAL_MS std::chrono::milliseconds(50)
#define MONITOR_DISPOSAL_TIME_MS std::chrono::milliseconds(200)

namespace {
    std::set<std::string> node_keys = { "any.node.domain" };
}


class MonitorTest : public testing::Test {
protected:
    std::shared_ptr<HOST_INFO> host;
    MONITOR* monitor;
    std::shared_ptr<MOCK_MONITOR_CONNECTION_CONTEXT> mock_context_short_interval;
    std::shared_ptr<MOCK_MONITOR_CONNECTION_CONTEXT> mock_context_long_interval;

    static void SetUpTestSuite() {}

    static void TearDownTestSuite() {}

    void SetUp() override {
        host = std::make_shared<HOST_INFO>("host", 1234);
        monitor = new MONITOR(host, MONITOR_DISPOSAL_TIME_MS, nullptr);
        
        mock_context_short_interval = std::make_shared<MOCK_MONITOR_CONNECTION_CONTEXT>(
            node_keys,
            FAILURE_DETECTION_TIME_MS,
            FAILURE_DETECTION_SHORT_INTERVAL_MS,
            FAILURE_DETECTION_COUNT);

        mock_context_long_interval = std::make_shared<MOCK_MONITOR_CONNECTION_CONTEXT>(
            node_keys,
            FAILURE_DETECTION_TIME_MS,
            FAILURE_DETECTION_LONG_INTERVAL_MS,
            FAILURE_DETECTION_COUNT);
    }

    void TearDown() override {
        mock_context_short_interval.reset();
        mock_context_long_interval.reset();
        delete monitor;
        host.reset();
    }

    // Defined so we can use private method get_connection_check_interval() in tests.
    std::chrono::milliseconds get_connection_check_interval() {
        return monitor->get_connection_check_interval();
    }

    // Defined so we can use private method check_connection_status() in tests.
    CONNECTION_STATUS check_connection_status(
        MONITOR* monitor, std::chrono::milliseconds shortest_detection_interval) {

        return monitor->check_connection_status(shortest_detection_interval);
    }
};

TEST_F(MonitorTest, StartMonitoringWithDifferentContexts) {
    EXPECT_CALL(*mock_context_short_interval, set_start_monitor_time(_));
    EXPECT_CALL(*mock_context_long_interval, set_start_monitor_time(_));
    
    monitor->start_monitoring(mock_context_short_interval);
    monitor->start_monitoring(mock_context_long_interval);

    EXPECT_EQ(FAILURE_DETECTION_SHORT_INTERVAL_MS, get_connection_check_interval());
}

TEST_F(MonitorTest, StopMonitoringWithContextRemaining) {
    EXPECT_CALL(*mock_context_short_interval, set_start_monitor_time(_));
    EXPECT_CALL(*mock_context_long_interval, set_start_monitor_time(_));
    
    monitor->start_monitoring(mock_context_short_interval);
    monitor->start_monitoring(mock_context_long_interval);

    monitor->stop_monitoring(mock_context_short_interval);

    EXPECT_EQ(FAILURE_DETECTION_LONG_INTERVAL_MS, get_connection_check_interval());
}

TEST_F(MonitorTest, StopMonitoringWithNoMatchingContexts) {
    monitor->stop_monitoring(mock_context_long_interval);

    EXPECT_EQ(std::chrono::milliseconds(0), get_connection_check_interval());

    EXPECT_CALL(*mock_context_short_interval, set_start_monitor_time(_));
    
    monitor->start_monitoring(mock_context_short_interval);
    monitor->stop_monitoring(mock_context_long_interval);

    EXPECT_EQ(FAILURE_DETECTION_SHORT_INTERVAL_MS, get_connection_check_interval());
}

TEST_F(MonitorTest, StopMonitoringTwiceWithSameContext) {
    EXPECT_CALL(*mock_context_long_interval, set_start_monitor_time(_));

    monitor->start_monitoring(mock_context_long_interval);

    monitor->stop_monitoring(mock_context_long_interval);
    monitor->stop_monitoring(mock_context_long_interval);

    EXPECT_EQ(std::chrono::milliseconds(0), get_connection_check_interval());
}

TEST_F(MonitorTest, IsConnectionHealthyWithNoExistingConnection) {
    auto mock_monitor = new MOCK_MONITOR(host, MONITOR_DISPOSAL_TIME_MS, nullptr);

    EXPECT_CALL(*mock_monitor, connect(_))
        .WillOnce(Return(true));

    EXPECT_CALL(*mock_monitor, ping_server())
        .WillOnce(Return(true));

    CONNECTION_STATUS status = check_connection_status(mock_monitor, FAILURE_DETECTION_SHORT_INTERVAL_MS);
    EXPECT_TRUE(status.is_valid);
    EXPECT_TRUE(status.elapsed_time >= std::chrono::milliseconds(0));

    delete mock_monitor;
}

TEST_F(MonitorTest, IsConnectionHealthyOrUnhealthy) {
    auto mock_monitor = new MOCK_MONITOR(host, MONITOR_DISPOSAL_TIME_MS, nullptr);

    EXPECT_CALL(*mock_monitor, connect(_))
        .WillRepeatedly(Return(true));

    EXPECT_CALL(*mock_monitor, ping_server())
        .WillOnce(Return(true))
        .WillOnce(Return(false));

    CONNECTION_STATUS status1 = check_connection_status(mock_monitor, FAILURE_DETECTION_SHORT_INTERVAL_MS);
    EXPECT_TRUE(status1.is_valid);

    CONNECTION_STATUS status2 = check_connection_status(mock_monitor, FAILURE_DETECTION_SHORT_INTERVAL_MS);
    EXPECT_FALSE(status2.is_valid);

    delete mock_monitor;
}

TEST_F(MonitorTest, IsConnectionHealthyAfterFailedConnection) {
    auto mock_monitor = new MOCK_MONITOR(host, MONITOR_DISPOSAL_TIME_MS, nullptr);

    EXPECT_CALL(*mock_monitor, connect(_))
        .WillRepeatedly(Return(false));

    CONNECTION_STATUS status = check_connection_status(mock_monitor, FAILURE_DETECTION_SHORT_INTERVAL_MS);
    EXPECT_FALSE(status.is_valid);
    EXPECT_TRUE(status.elapsed_time >= std::chrono::milliseconds(0));

    delete mock_monitor;
}
