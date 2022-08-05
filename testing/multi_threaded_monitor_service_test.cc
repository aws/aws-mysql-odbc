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

#include "test_utils.h"
#include "mock_objects.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Return;
using ::testing::Sequence;

namespace {
    const std::chrono::milliseconds failure_detection_time(10);
    const std::chrono::milliseconds failure_detection_interval(100);
    const int failure_detection_count = 3;
    const std::chrono::milliseconds monitor_disposal_time(200);
}

class MultiThreadedMonitorServiceTest : public testing::Test {
protected:
    const int num_connections = 10;
    std::shared_ptr<HOST_INFO> host;
    std::shared_ptr<MOCK_MONITOR_THREAD_CONTAINER> mock_container;
    std::vector<std::shared_ptr<MONITOR_SERVICE>> services;

    static void SetUpTestSuite() {}

    static void TearDownTestSuite() {}

    void SetUp() override {
        host = std::make_shared<HOST_INFO>("host", 1234);
        mock_container = std::make_shared<MOCK_MONITOR_THREAD_CONTAINER>();
        services = generate_services(num_connections);
    }

    void TearDown() override {
        for (auto &service : services) {
            service->release_resources();
        }
        MONITOR_THREAD_CONTAINER::release_instance();
    }

    std::vector<std::shared_ptr<MONITOR_SERVICE>> generate_services(int num_services) {
        std::vector<std::shared_ptr<MONITOR_SERVICE>> services;
        for (int i = 0; i < num_services; i++) {
            services.push_back(std::make_shared<MONITOR_SERVICE>(mock_container));
        }

        return services;
    }

    std::vector<std::set<std::string>> generate_node_keys(int num_node_keys, bool diff_node) {
        std::vector<std::set<std::string>> node_key_list;
        for (int i = 0; i < num_node_keys; i++) {
            std::string node = diff_node ? "node" + std::to_string(i) : "node";
            node_key_list.push_back({ node });
        }

        return node_key_list;
    }

    bool has_node_keys(std::vector<std::set<std::string>> node_key_list, std::set<std::string> node_keys) {
        for (int i = 0; i < node_key_list.size(); i++) {
            if (node_key_list[i] == node_keys) {
                return true;
            }
        }

        return false;
    }

    void run_start_monitor(
        int num_threads,
        std::vector<std::shared_ptr<MONITOR_SERVICE>> services,
        std::vector<std::set<std::string>> node_key_list,
        std::shared_ptr<HOST_INFO> host) {

        std::vector<std::thread> threads;
        for (int i = 0; i < num_threads; i++) {
            auto service = services.at(i);
            auto node_keys = node_key_list.at(i);

            auto thread = std::thread([&service, node_keys, host]() {
                service->start_monitoring(
                    nullptr,
                    nullptr,
                    node_keys,
                    host,
                    failure_detection_time,
                    failure_detection_interval,
                    failure_detection_count,
                    monitor_disposal_time);
            });

            threads.push_back(std::move(thread));
        }

        for (auto& thread : threads) {
            thread.join();
        }
    }

    void run_stop_monitor(
        int num_threads,
        std::vector<std::shared_ptr<MONITOR_SERVICE>> services,
        std::vector<std::shared_ptr<MONITOR_CONNECTION_CONTEXT>> contexts) {
        
        std::vector<std::thread> threads;
        for (int i = 0; i < num_threads; i++) {
            auto service = services.at(i);
            auto context = contexts.at(i);

            auto thread = std::thread([&service, context]() {
                service->stop_monitoring(context);
            });

            threads.push_back(std::move(thread));
        }

        for (auto& thread : threads) {
            thread.join();
        }
    }
};

TEST_F(MultiThreadedMonitorServiceTest, StartAndStopMonitoring_MultipleConnectionsToDifferentNodes) {
    std::vector<std::set<std::string>> node_key_list = generate_node_keys(num_connections, true);

    std::vector<std::shared_ptr<MOCK_MONITOR2>> monitors;
    for (int i = 0; i < num_connections; i++) {
        auto mock_monitor = std::make_shared<MOCK_MONITOR2>(host, monitor_disposal_time);
        monitors.push_back(mock_monitor);

        EXPECT_CALL(*monitors[i], run(_)).Times(AtLeast(1));
    }

    Sequence s1;
    for (int i = 0; i < num_connections; i++) {
        EXPECT_CALL(*mock_container, create_monitor(_, _, _, _))
            .InSequence(s1)
            .WillOnce(Return(monitors[i]));
    }

    run_start_monitor(num_connections, services, node_key_list, host);
    
    EXPECT_EQ(num_connections, TEST_UTILS::get_map_size(mock_container));
    
    std::vector<std::shared_ptr<MONITOR_CONNECTION_CONTEXT>> contexts;
    for (int i = 0; i < num_connections; i++) {
        auto monitor_contexts = TEST_UTILS::get_contexts(monitors[i]);
        EXPECT_EQ(1, monitor_contexts.size());

        auto context = monitor_contexts.front();
        EXPECT_TRUE(has_node_keys(node_key_list, context->get_node_keys()));

        contexts.push_back(context);
    }

    run_stop_monitor(num_connections, services, contexts);

    for (int i = 0; i < num_connections; i++) {
        EXPECT_EQ(0, TEST_UTILS::get_contexts(monitors[i]).size());
    }
}

TEST_F(MultiThreadedMonitorServiceTest, StartAndStopMonitoring_MultipleConnectionsToOneNode) {
    std::vector<std::set<std::string>> node_key_list = generate_node_keys(num_connections, false);

    auto mock_monitor = std::make_shared<MOCK_MONITOR2>(host, monitor_disposal_time);

    EXPECT_CALL(*mock_container, create_monitor(_, _, _, _))
        .WillOnce(Return(mock_monitor));

    EXPECT_CALL(*mock_monitor, run(_)).Times(AtLeast(1));

    run_start_monitor(num_connections, services, node_key_list, host);

    EXPECT_EQ(1, TEST_UTILS::get_map_size(mock_container));

    auto monitor_contexts = TEST_UTILS::get_contexts(mock_monitor);
    EXPECT_EQ(num_connections, monitor_contexts.size());

    std::vector<std::shared_ptr<MONITOR_CONNECTION_CONTEXT>> contexts;
    for (auto it = monitor_contexts.begin(); it != monitor_contexts.end(); ++it) {
        std::shared_ptr<MONITOR_CONNECTION_CONTEXT> context = *it;
        EXPECT_TRUE(has_node_keys(node_key_list, context->get_node_keys()));

        contexts.push_back(context);
    }

    run_stop_monitor(num_connections, services, contexts);

    EXPECT_EQ(0, TEST_UTILS::get_contexts(mock_monitor).size());
}
