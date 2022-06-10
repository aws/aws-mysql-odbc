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

#include "mock_objects.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::_;
using ::testing::Return;

#define FAILURE_DETECTION_TIME_MS 10
#define FAILURE_DETECTION_INTERVAL_MS 100
#define FAILURE_DETECTION_COUNT 3
#define VALIDATION_INTERVAL_MILLIS 50

namespace {
    std::set<std::string> node_keys = { "any.node.domain" };
}

class MonitorConnectionContextTest : public testing::Test {
 protected:
    MONITOR_CONNECTION_CONTEXT* context;

    static void SetUpTestSuite() {}

    static void TearDownTestSuite() {}

    void SetUp() override {
        context = new MONITOR_CONNECTION_CONTEXT(NULL,
                                                 node_keys,
                                                 FAILURE_DETECTION_INTERVAL_MS,
                                                 FAILURE_DETECTION_TIME_MS,
                                                 FAILURE_DETECTION_COUNT);
    }

    void TearDown() override {
        delete context;
    }
};

TEST_F(MonitorConnectionContextTest, IsNodeUnhealthyWithConnection_ReturnFalse) {
    //auto now = std::chrono::system_clock::now();
    //std::chrono::duration<long> current_time_ms = std::chrono::milliseconds(now).count();
    context->set_connection_valid(true, 1234, 1234);
    EXPECT_FALSE(context->is_node_unhealthy());
    EXPECT_EQ(0, context->get_failure_count());
}

TEST_F(MonitorConnectionContextTest, IsNodeUnhealthyWithInvalidConnection_ReturnFalse) {
    //auto now = std::chrono::system_clock::now();
    //std::chrono::duration<long> current_time_ms = std::chrono::milliseconds(now).count();
    context->set_connection_valid(false, 1234, 1234);
    EXPECT_FALSE(context->is_node_unhealthy());
    EXPECT_EQ(1, context->get_failure_count());
}
