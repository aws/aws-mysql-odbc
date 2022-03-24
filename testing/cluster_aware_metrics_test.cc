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
#include <iostream>
#include <fstream>
#include <stdio.h>

#include "driver/driver.h"
#include "mock_objects.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Return;
using ::testing::StrEq;

class ClusterAwareMetricsContainerTest : public testing::Test {
protected:
    SQLHENV env = SQL_NULL_HENV;
    SQLHDBC hdbc = SQL_NULL_HDBC;
    DBC* dbc = nullptr;
    DataSource* ds;

    std::shared_ptr<MOCK_CLUSTER_AWARE_METRICS_CONTAINER> metrics_container;

    std::string cluster_id = "test-cluster-id";
    std::string instance_url = "test-instance-url.domain.com:12345";

    static void SetUpTestSuite() {}

    static void TearDownTestSuite() {}

    void SetUp() override {
        env = SQL_NULL_HENV;
        hdbc = SQL_NULL_HDBC;
        dbc = nullptr;

        SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env);
        SQLAllocHandle(SQL_HANDLE_DBC, env, &hdbc);
        dbc = static_cast<DBC*>(hdbc);

        ds = ds_new();

        metrics_container = std::make_shared<MOCK_CLUSTER_AWARE_METRICS_CONTAINER>(dbc, ds);
    }

    void TearDown() override {
        if (SQL_NULL_HDBC != hdbc) {
            SQLFreeHandle(SQL_HANDLE_DBC, hdbc);
        }
        if (SQL_NULL_HENV != env) {
            SQLFreeHandle(SQL_HANDLE_ENV, env);
        }
        if (nullptr != dbc) {
            dbc = nullptr;
        }
        if (nullptr != ds) {
            ds_delete(ds);
            ds = nullptr;
        }
    }
};

TEST_F(ClusterAwareMetricsContainerTest, isEnabled) {
    std::shared_ptr<MOCK_CLUSTER_AWARE_METRICS_CONTAINER> metrics_container
        = std::make_shared<MOCK_CLUSTER_AWARE_METRICS_CONTAINER>();
    EXPECT_FALSE(metrics_container->is_enabled());
    EXPECT_FALSE(metrics_container->is_instance_metrics_enabled());
    metrics_container->set_gather_metric(true);
    EXPECT_TRUE(metrics_container->is_enabled());
    EXPECT_TRUE(metrics_container->is_instance_metrics_enabled());

    ds->gather_perf_metrics = true;
    ds->gather_metrics_per_instance = true;
    std::shared_ptr<MOCK_CLUSTER_AWARE_METRICS_CONTAINER> ds_metrics_container
        = std::make_shared<MOCK_CLUSTER_AWARE_METRICS_CONTAINER>(dbc, ds);
    EXPECT_TRUE(ds_metrics_container->is_enabled());
    EXPECT_TRUE(ds_metrics_container->is_instance_metrics_enabled());

    ds->gather_perf_metrics = false;
    ds->gather_metrics_per_instance = false;
    EXPECT_FALSE(ds_metrics_container->is_enabled());
    EXPECT_FALSE(ds_metrics_container->is_instance_metrics_enabled());

    ds->gather_perf_metrics = true;
    ds->gather_metrics_per_instance = false;
    EXPECT_TRUE(ds_metrics_container->is_enabled());
    EXPECT_FALSE(ds_metrics_container->is_instance_metrics_enabled());

    ds->gather_perf_metrics = false;
    ds->gather_metrics_per_instance = true;
    EXPECT_FALSE(ds_metrics_container->is_enabled());
    EXPECT_TRUE(ds_metrics_container->is_instance_metrics_enabled());
}

TEST_F(ClusterAwareMetricsContainerTest, collectClusterOnly) {
    ds->gather_perf_metrics = true;
    ds->gather_metrics_per_instance = false;

    EXPECT_CALL(*metrics_container, get_curr_conn_url())
        .Times(0);

    metrics_container->set_cluster_id(cluster_id);

    metrics_container->register_failure_detection_time(1234);
    metrics_container->register_writer_failover_procedure_time(1234);
    metrics_container->register_reader_failover_procedure_time(1234);
    metrics_container->register_failover_connects(true);
    metrics_container->register_invalid_initial_connection(true);
    metrics_container->register_use_cached_topology(true);
    metrics_container->register_topology_query_execution_time(1234);

    std::string cluster_logs = metrics_container->report_metrics(cluster_id, false);
    std::string instance_logs = metrics_container->report_metrics(cluster_id, true);

    int cluster_length = std::count(cluster_logs.begin(),
        cluster_logs.end(), '\n');

    int instance_length = std::count(instance_logs.begin(),
        instance_logs.end(), '\n');

    EXPECT_TRUE(cluster_length > instance_length);
}

TEST_F(ClusterAwareMetricsContainerTest, collectInstance) {
    ds->gather_perf_metrics = true;
    ds->gather_metrics_per_instance = true;

    EXPECT_CALL(*metrics_container, get_curr_conn_url())
        .Times(7)
        .WillRepeatedly(Return(instance_url));

    metrics_container->set_cluster_id(cluster_id);

    metrics_container->register_failure_detection_time(1234);
    metrics_container->register_writer_failover_procedure_time(1234);
    metrics_container->register_reader_failover_procedure_time(1234);
    metrics_container->register_failover_connects(true);
    metrics_container->register_invalid_initial_connection(true);
    metrics_container->register_use_cached_topology(true);
    metrics_container->register_topology_query_execution_time(1234);

    std::string cluster_logs = metrics_container->report_metrics(cluster_id, false);
    std::string instance_logs = metrics_container->report_metrics(instance_url, true);

    int cluster_length = std::count(cluster_logs.begin(),
        cluster_logs.end(), '\n');

    int instance_length = std::count(instance_logs.begin(),
        instance_logs.end(), '\n');

    EXPECT_TRUE(cluster_length == instance_length);
}
