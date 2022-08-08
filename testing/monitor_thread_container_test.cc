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

#include "driver/monitor_thread_container.h"

#include "test_utils.h"
#include "mock_objects.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::_;
using ::testing::Return;

namespace {
    std::string node_key = "any.node.domain";
    std::set<std::string> node_keys = { node_key };
    const std::chrono::milliseconds failure_detection_time(10);
    const std::chrono::milliseconds failure_detection_interval(30);
    const int failure_detection_count = 3;
    const std::chrono::milliseconds monitor_disposal_time(200);
}

static std::shared_ptr<HOST_INFO> host;
static std::shared_ptr<MONITOR_THREAD_CONTAINER> thread_container;
static std::shared_ptr<MONITOR_SERVICE> service;

class MonitorThreadContainerTest : public testing::Test {
protected:
    static void SetUpTestSuite() {
        host = std::make_shared<HOST_INFO>("host", 1234);
        thread_container = MONITOR_THREAD_CONTAINER::get_instance();
        service = std::make_shared<MONITOR_SERVICE>(thread_container);
    }

    static void TearDownTestSuite() {
        service->release_resources();
    }

    void SetUp() override {}

    void TearDown() override {}
};

TEST_F(MonitorThreadContainerTest, GetInstance) {
    const auto thread_container_same = MONITOR_THREAD_CONTAINER::get_instance();
    EXPECT_TRUE(thread_container_same == thread_container);
}

TEST_F(MonitorThreadContainerTest, MultipleNodeKeys) {
    std::set<std::string> node_keys1 = { "nodeOne.domain", "nodeTwo.domain" };
    std::set<std::string> node_keys2 = { "nodeTwo.domain" };

    auto monitor1 = thread_container->get_or_create_monitor(
        node_keys1, host, monitor_disposal_time, nullptr);
    EXPECT_NE(nullptr, monitor1);

    // Should return the same monitor again because the first call to get_or_create_monitor()
    // mapped the monitor to both "nodeOne.domain" and "nodeTwo.domain".
    auto monitor2 = thread_container->get_or_create_monitor(
        node_keys2, host, monitor_disposal_time, nullptr);
    EXPECT_NE(nullptr, monitor2);

    EXPECT_TRUE(monitor1 == monitor2);
}

TEST_F(MonitorThreadContainerTest, DifferentNodeKeys) {
    std::set<std::string> keys = { "nodeNEW.domain" };

    auto monitor1 = thread_container->get_or_create_monitor(
        keys, host, monitor_disposal_time, nullptr);
    EXPECT_NE(nullptr, monitor1);

    auto monitor2 = thread_container->get_or_create_monitor(
        keys, host, monitor_disposal_time, nullptr);
    EXPECT_NE(nullptr, monitor2);

    // Monitors should be the same because both calls to get_or_create_monitor()
    // used the same node keys.
    EXPECT_TRUE(monitor1 == monitor2);

    auto monitor3 = thread_container->get_or_create_monitor(
        node_keys, host, monitor_disposal_time, nullptr);
    EXPECT_NE(nullptr, monitor3);

    // Last monitor should be different because it has a different node key.
    EXPECT_TRUE(monitor1 != monitor3);
    EXPECT_TRUE(monitor2 != monitor3);
}

TEST_F(MonitorThreadContainerTest, SameKeysInDifferentNodeKeys) {
    std::set<std::string> keys1 = { "nodeA" };
    std::set<std::string> keys2 = { "nodeA", "nodeB" };
    std::set<std::string> keys3 = { "nodeB" };

    auto monitor1 = thread_container->get_or_create_monitor(
        keys1, host, monitor_disposal_time, nullptr);
    EXPECT_NE(nullptr, monitor1);

    auto monitor2 = thread_container->get_or_create_monitor(
        keys2, host, monitor_disposal_time, nullptr);
    EXPECT_NE(nullptr, monitor2);

    // Monitors should be the same because both sets of keys have "nodeA".
    EXPECT_TRUE(monitor1 == monitor2);

    auto monitor3 = thread_container->get_or_create_monitor(
        keys3, host, monitor_disposal_time, nullptr);
    EXPECT_NE(nullptr, monitor3);

    // Last monitor should be also be the same because the 2nd call to get_or_create_monitor()
    // mapped the monitor to "nodeB" and the 3rd call used "nodeB".
    EXPECT_TRUE(monitor1 == monitor3);
    EXPECT_TRUE(monitor2 == monitor3);
}

TEST_F(MonitorThreadContainerTest, PopulateMonitorMap) {
    std::set<std::string> keys = { "nodeA", "nodeB", "nodeC", "nodeD" };
    auto monitor = thread_container->get_or_create_monitor(
        keys, host, monitor_disposal_time, nullptr);

    // Check that we now have mappings for all the keys.
    for (auto it = keys.begin(); it != keys.end(); ++it) {
        std::string node = *it;
        auto get_monitor = thread_container->get_monitor(node);
        EXPECT_NE(nullptr, get_monitor);

        EXPECT_TRUE(get_monitor == monitor);
    }
}

TEST_F(MonitorThreadContainerTest, RemoveMonitorMapping) {
    std::set<std::string> keys1 = { "nodeA", "nodeB", "nodeC", "nodeD" };
    auto monitor1 = thread_container->get_or_create_monitor(
        keys1, host, monitor_disposal_time, nullptr);

    std::set<std::string> keys2 = { "nodeE", "nodeF", "nodeG", "nodeH" };
    auto monitor2 = thread_container->get_or_create_monitor(
        keys2, host, std::chrono::milliseconds(100), nullptr);

    // This should remove the mappings for keys1 but not keys2.
    thread_container->reset_resource(monitor1);

    // Check that we no longer have any mappings for keys1.
    for (auto it = keys1.begin(); it != keys1.end(); ++it) {
        std::string node = *it;
        auto get_monitor = thread_container->get_monitor(node);
        EXPECT_THAT(get_monitor, nullptr);
    }

    // Check that we still have all the mapping for keys2.
    for (auto it = keys2.begin(); it != keys2.end(); ++it) {
        std::string node = *it;
        auto get_monitor = thread_container->get_monitor(node);
        EXPECT_NE(nullptr, get_monitor);

        EXPECT_TRUE(get_monitor == monitor2);
    }
}

TEST_F(MonitorThreadContainerTest, AvailableMonitorsQueue) {
    std::set<std::string> keys = { "nodeA", "nodeB", "nodeC", "nodeD" };
    auto mock_thread_container = std::make_shared<MOCK_MONITOR_THREAD_CONTAINER>();
    auto monitor_service = std::make_shared<MONITOR_SERVICE>(mock_thread_container);
    auto mock_monitor1 = std::make_shared<MOCK_MONITOR>(host, monitor_disposal_time, nullptr);
    auto mock_monitor2 = std::make_shared<MOCK_MONITOR>(host, monitor_disposal_time, nullptr);

    // While we have three get_or_create_monitor() calls, we only call create_monitor() twice.
    EXPECT_CALL(*mock_thread_container, create_monitor(_, _, _, _))
        .WillOnce(Return(mock_monitor1))
        .WillOnce(Return(mock_monitor2));

    EXPECT_CALL(*mock_monitor1, is_stopped())
        .WillOnce(Return(false))
        .WillOnce(Return(true));

    EXPECT_CALL(*mock_monitor1, run(_));
    
    // This first call should create the monitor.
    auto monitor1 = mock_thread_container->get_or_create_monitor(
        keys, host, monitor_disposal_time, nullptr);
    EXPECT_NE(nullptr, monitor1);

    mock_thread_container->add_task(monitor1, monitor_service);

    EXPECT_TRUE(TEST_UTILS::has_monitor(mock_thread_container, *keys.begin()));
    EXPECT_TRUE(TEST_UTILS::has_task(mock_thread_container, monitor1));

    // This should remove the node key mappings and add the monitor to the available monitors queue.
    mock_thread_container->reset_resource(monitor1);

    EXPECT_TRUE(TEST_UTILS::has_available_monitor(mock_thread_container));
    auto available_monitor = TEST_UTILS::get_available_monitor(mock_thread_container);
    EXPECT_NE(nullptr, available_monitor);

    EXPECT_TRUE(monitor1 == available_monitor);

    // This second call should get the monitor from the available monitors queue
    // instead of creating a new monitor.
    auto monitor2 = mock_thread_container->get_or_create_monitor(
        keys, host, monitor_disposal_time, nullptr);
    EXPECT_NE(nullptr, monitor2);

    EXPECT_TRUE(monitor2 == available_monitor);

    // Adding monitor back to available monitors queue.
    mock_thread_container->reset_resource(monitor2);

    // This call will discard the available monitor because it is now stopped
    // and create a new monitor.
    auto monitor3 = mock_thread_container->get_or_create_monitor(
        { node_key }, host, monitor_disposal_time, nullptr);
    EXPECT_NE(nullptr, monitor3);

    EXPECT_NE(monitor1, monitor3);

    EXPECT_FALSE(TEST_UTILS::has_any_tasks(mock_thread_container));

    monitor_service->release_resources();
}

TEST_F(MonitorThreadContainerTest, PopulateAndRemoveMappings) {
    auto context = service->start_monitoring(
        nullptr,
        nullptr,
        node_keys,
        host,
        failure_detection_time,
        failure_detection_interval,
        failure_detection_count,
        monitor_disposal_time);

    EXPECT_TRUE(TEST_UTILS::has_monitor(thread_container, node_key));
    EXPECT_TRUE(TEST_UTILS::has_task(thread_container, thread_container->get_monitor(node_key)));

    service->stop_monitoring(context);

    std::this_thread::sleep_for(std::chrono::milliseconds(1500));

    EXPECT_FALSE(TEST_UTILS::has_monitor(thread_container, node_key));
    EXPECT_FALSE(TEST_UTILS::has_any_tasks(thread_container));
}
