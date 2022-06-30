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

#include "driver/monitor_connection_context.h"
#include "driver/driver.h"

#include "mock_objects.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::_;
using ::testing::Return;

namespace {
    const std::set<std::string> node_keys = { "any.node.domain" };
    const std::chrono::milliseconds failure_detection_time = std::chrono::milliseconds(10);
    const std::chrono::milliseconds failure_detection_interval = std::chrono::milliseconds(100);
    const int failure_detection_count = 3;
    const std::chrono::milliseconds validation_interval = std::chrono::milliseconds(50);
}

class MonitorConnectionContextTest : public testing::Test {
 protected:
    MONITOR_CONNECTION_CONTEXT* context;
    DBC* connection_to_abort;
    ENV* env;
    

    static void SetUpTestSuite() {}

    static void TearDownTestSuite() {}

    void SetUp() override {
        env = new ENV(SQL_OV_ODBC2);
        connection_to_abort = new DBC(env);
        context = new MONITOR_CONNECTION_CONTEXT(connection_to_abort,
                                                 node_keys,
                                                 failure_detection_time,
                                                 failure_detection_interval,
                                                 failure_detection_count);
    }

    void TearDown() override {
        delete context;
        delete connection_to_abort;
        delete env;
    }
};

TEST_F(MonitorConnectionContextTest, IsNodeUnhealthyWithConnection_ReturnFalse) {
    std::chrono::steady_clock::time_point current_time = std::chrono::steady_clock::now();
    context->set_connection_valid(true, current_time, current_time);
    EXPECT_FALSE(context->is_node_unhealthy());
    EXPECT_EQ(0, context->get_failure_count());
}

TEST_F(MonitorConnectionContextTest, IsNodeUnhealthyWithInvalidConnection_ReturnFalse) {
    std::chrono::steady_clock::time_point current_time = std::chrono::steady_clock::now();
    context->set_connection_valid(false, current_time, current_time);
    EXPECT_FALSE(context->is_node_unhealthy());
    EXPECT_EQ(1, context->get_failure_count());
}

TEST_F(MonitorConnectionContextTest, IsNodeUnhealthyExceedsFailureDetectionCount_ReturnTrue) {
    const int expected_failure_count = failure_detection_count + 1;
    context->set_failure_count(failure_detection_count);
    context->reset_invalid_node_start_time();

    std::chrono::steady_clock::time_point current_time = std::chrono::steady_clock::now();
    context->set_connection_valid(false, current_time, current_time);

    EXPECT_FALSE(context->is_node_unhealthy());
    EXPECT_EQ(expected_failure_count, context->get_failure_count());
    EXPECT_TRUE(context->is_invalid_node_start_time_defined());
}

TEST_F(MonitorConnectionContextTest, IsNodeUnhealthyExceedsFailureDetectionCount) {
    std::chrono::steady_clock::time_point current_time = std::chrono::steady_clock::now();
    context->set_failure_count(0);
    context->reset_invalid_node_start_time();

    // Simulate monitor loop that reports invalid connection for 5 times with interval 50 msec to wait 250 msec in total
    for (int i = 0; i < 5; i++) {
      std::chrono::steady_clock::time_point status_check_start_time = current_time;
      std::chrono::steady_clock::time_point status_check_end_time = current_time + validation_interval;

      context->set_connection_valid(false, status_check_start_time, status_check_end_time);
      EXPECT_FALSE(context->is_node_unhealthy());

      current_time += validation_interval;
    }

    // Simulate waiting another 50 msec that makes total waiting time to 300 msec
    // Expected max waiting time for this context is 300 msec (interval 100 msec, count 3)
    // So it's expected that this run turns node status to "unhealthy" since we reached max allowed waiting time.

    std::chrono::steady_clock::time_point status_check_start_time = current_time;
    std::chrono::steady_clock::time_point status_check_end_time = current_time + validation_interval;

    context->set_connection_valid(false, status_check_start_time, status_check_end_time);
    EXPECT_TRUE(context->is_node_unhealthy());
}
