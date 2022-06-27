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

#include "driver/monitor_service.h"

#include "mock_objects.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::_;
using ::testing::Return;

#define FAILURE_DETECTION_TIME_MS std::chrono::milliseconds(10)
#define FAILURE_DETECTION_INTERVAL_MS std::chrono::milliseconds(100)
#define FAILURE_DETECTION_COUNT 3
#define MONITOR_DISPOSAL_TIME_MS std::chrono::milliseconds(200)

namespace {
    std::set<std::string> node_keys = { "any.node.domain" };
}

class MonitorServiceTest : public testing::Test {
protected:
    std::shared_ptr<HOST_INFO> host;
    std::shared_ptr<MOCK_MONITOR> mock_monitor;
    std::shared_ptr<MOCK_MONITOR_THREAD_CONTAINER> mock_thread_container;
    MONITOR_SERVICE* monitor_service;
    
    static void SetUpTestSuite() {}

    static void TearDownTestSuite() {}

    void SetUp() override {
        host = std::make_shared<HOST_INFO>("host", 1234);
        mock_thread_container = std::make_shared<MOCK_MONITOR_THREAD_CONTAINER>();
        monitor_service = new MONITOR_SERVICE(mock_thread_container);
        mock_monitor = std::make_shared<MOCK_MONITOR>(host, MONITOR_DISPOSAL_TIME_MS, monitor_service);
    }

    void TearDown() override {
        host.reset();
        mock_thread_container.reset();
        mock_monitor.reset();
        delete monitor_service;
    }
};

TEST_F(MonitorServiceTest, StartMonitoring) {
    EXPECT_CALL(*mock_thread_container, create_monitor(_, _, _))
        .WillOnce(Return(mock_monitor));

    EXPECT_CALL(*mock_monitor, start_monitoring(_)).Times(1);

    auto context = monitor_service->start_monitoring(
        nullptr,
        node_keys,
        host,
        FAILURE_DETECTION_TIME_MS,
        FAILURE_DETECTION_INTERVAL_MS,
        FAILURE_DETECTION_COUNT,
        MONITOR_DISPOSAL_TIME_MS);
    EXPECT_NE(nullptr, context);
}

TEST_F(MonitorServiceTest, StartMonitoringCalledMultipleTimes) {
    EXPECT_CALL(*mock_thread_container, create_monitor(_, _, _))
        .WillOnce(Return(mock_monitor));

    const int runs = 5;

    EXPECT_CALL(*mock_monitor, start_monitoring(_)).Times(runs);

    for (int i = 0; i < runs; i++) {
        auto context = monitor_service->start_monitoring(
            nullptr,
            node_keys,
            host,
            FAILURE_DETECTION_TIME_MS,
            FAILURE_DETECTION_INTERVAL_MS,
            FAILURE_DETECTION_COUNT,
            MONITOR_DISPOSAL_TIME_MS);
        EXPECT_NE(nullptr, context);
    }
}

TEST_F(MonitorServiceTest, StopMonitoring) {
    EXPECT_CALL(*mock_thread_container, create_monitor(_, _, _))
        .WillOnce(Return(mock_monitor));

    EXPECT_CALL(*mock_monitor, start_monitoring(_)).Times(1);

    auto context = monitor_service->start_monitoring(
        nullptr,
        node_keys,
        host,
        FAILURE_DETECTION_TIME_MS,
        FAILURE_DETECTION_INTERVAL_MS,
        FAILURE_DETECTION_COUNT,
        MONITOR_DISPOSAL_TIME_MS);
    EXPECT_NE(nullptr, context);

    EXPECT_CALL(*mock_monitor, stop_monitoring(context)).Times(1);

    monitor_service->stop_monitoring(context);
}

TEST_F(MonitorServiceTest, StopMonitoringCalledTwice) {
    EXPECT_CALL(*mock_thread_container, create_monitor(_, _, _))
        .WillOnce(Return(mock_monitor));

    EXPECT_CALL(*mock_monitor, start_monitoring(_)).Times(1);

    auto context = monitor_service->start_monitoring(
        nullptr,
        node_keys,
        host,
        FAILURE_DETECTION_TIME_MS,
        FAILURE_DETECTION_INTERVAL_MS,
        FAILURE_DETECTION_COUNT,
        MONITOR_DISPOSAL_TIME_MS);
    EXPECT_NE(nullptr, context);

    EXPECT_CALL(*mock_monitor, stop_monitoring(context)).Times(2);

    monitor_service->stop_monitoring(context);
    monitor_service->stop_monitoring(context);
}

TEST_F(MonitorServiceTest, EmptyNodeKeys) {
    std::set<std::string> keys = { };

    EXPECT_THROW(
        monitor_service->start_monitoring(
            nullptr,
            keys,
            host,
            FAILURE_DETECTION_TIME_MS,
            FAILURE_DETECTION_INTERVAL_MS,
            FAILURE_DETECTION_COUNT,
            MONITOR_DISPOSAL_TIME_MS),
        std::invalid_argument);
}
