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
    const std::chrono::milliseconds failure_detection_time(10);
    const std::chrono::milliseconds failure_detection_interval(100);
    const int failure_detection_count = 3;
    const std::chrono::milliseconds validation_interval(50);
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

TEST_F(MonitorConnectionContextTest, EfmSimpleTest2) {
  char* dsn = "test";              // std::getenv("TEST_DSN");
  char* db = "employees";          // std::getenv("TEST_DATABASE");
  char* user = "admin";            // std::getenv("TEST_UID");
  char* pwd = "my_password_2020";  // std::getenv("TEST_PASSWORD");
  char* server =
      "mysql-instance-4.czygpppufgy4.us-east-2.rds.amazonaws.com";  // std::getenv("TEST_SERVER");
  char* sql = "select sleep(30)";
  int rc = 0;
  MYSQL* conn;
  MYSQL_RES* res;
  MYSQL_ROW row;
  //myodbc_init();
  conn = mysql_init(NULL);
  /* Connect to database */
  if (!mysql_real_connect(conn, server, user, pwd, db, 0, NULL, 0)) {
    EXPECT_TRUE(false) << mysql_error(conn) << "\n";
    //printf(stderr, "%s\n", mysql_error(conn));
    return;
  }
  auto close_connection_future = std::async(std::launch::async, [&conn]() {
    mysql_thread_init();
    time_t my_time = time(NULL);
    EXPECT_TRUE(false) << std::ctime(&my_time) << ": begin of thread\n";
    //printf("%s: begin of thread\n\n", std::ctime(&my_time));
    std::this_thread::sleep_for(std::chrono::seconds(10));
    my_time = time(NULL);
    EXPECT_TRUE(false) << std::ctime(&my_time) << ": before kill\n";
    //printf("%s: before kill\n\n", std::ctime(&my_time));
    /* kill connection here */
    // mysql_options(conn, MYSQL_OPT_READ_TIMEOUT, "1");
    // mysql_options(conn, MYSQL_OPT_WRITE_TIMEOUT, "1");
    //
    // mysql_close(conn);
    //
    //close(conn->net.fd);
    closesocket(conn->net.fd);
    //
    // mysql_reset_connection(conn);
    //
    // mysql_refresh(conn, REFRESH_THREADS);
    my_time = time(NULL);
    EXPECT_TRUE(false) << std::ctime(&my_time) << ": after kill\n";
    //printf("%s: after kill\n\n", std::ctime(&my_time));
    mysql_thread_end();
  });
  /* send SQL query */
  time_t my_time = time(NULL);
  EXPECT_TRUE(false) << std::ctime(&my_time) << ": before query\n";
  //printf("%s: before query\n\n", std::ctime(&my_time));
  rc = mysql_send_query(conn, sql, strlen(sql));
  EXPECT_TRUE(false) << rc << ": " << mysql_error(conn) << "\n";
  //printf("%u: %s\n", rc, mysql_error(conn));
  // net_async_status async_status = mysql_real_query_nonblocking(conn, sql,
  // strlen(sql)); while (async_status == NET_ASYNC_NOT_READY) {
  //    std::this_thread::sleep_for(std::chrono::seconds(1));
  //    my_time = time(NULL);
  //    printf("%s: wait, status=%u\n", std::ctime(&my_time), async_status);
  //    async_status = mysql_real_query_nonblocking(conn, sql, strlen(sql));
  //}
  my_time = time(NULL);
  EXPECT_TRUE(false) << std::ctime(&my_time) << ": after query\n";
  //printf("%s: after query\n\n", std::ctime(&my_time));
  rc = mysql_read_query_result(conn);
  EXPECT_TRUE(false) << rc << ": " << mysql_error(conn) << "\n";
  //printf("%u: %s\n", rc, mysql_error(conn));
  /* close connection */
  mysql_close(conn);
  mysql_library_end();
}

