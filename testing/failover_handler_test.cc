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

#include "driver/driver.h"
#include "mock_objects.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Return;
using ::testing::StrEq;

class FailoverHandlerTest : public testing::Test {
   protected:
    static std::shared_ptr<HOST_INFO> writer_host;
    static std::shared_ptr<HOST_INFO> reader_host;
    static std::shared_ptr<CLUSTER_TOPOLOGY_INFO> topology;
    SQLHENV env = SQL_NULL_HENV;
    SQLHDBC dbc = SQL_NULL_HDBC;
    DataSource* ds;
    std::shared_ptr<MOCK_TOPOLOGY_SERVICE> mock_ts;
    std::shared_ptr<MOCK_CONNECTION_HANDLER> mock_connection_handler;

    static void SetUpTestSuite() {
        writer_host = std::make_shared<HOST_INFO>("writer-host.com", 1234, UP, true);
        reader_host = std::make_shared<HOST_INFO>("reader-host.com", 1234, UP, false);
        topology = std::make_shared<CLUSTER_TOPOLOGY_INFO>();
        topology->add_host(writer_host);
        topology->add_host(reader_host);
    }

    static void TearDownTestSuite() {}

    void SetUp() override {
        env = SQL_NULL_HENV;
        dbc = SQL_NULL_HDBC;

        SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env);
        SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc);
        ds = ds_new();
        ds->disable_cluster_failover = false;

        mock_ts = std::make_shared<MOCK_TOPOLOGY_SERVICE>();
        mock_connection_handler = std::make_shared<MOCK_CONNECTION_HANDLER>();
    }

    void TearDown() override {
        if (SQL_NULL_HDBC != dbc) {
            SQLFreeHandle(SQL_HANDLE_DBC, dbc);
        }
        if (SQL_NULL_HENV != env) {
            SQLFreeHandle(SQL_HANDLE_ENV, env);
        }
    }
};

std::shared_ptr<HOST_INFO> FailoverHandlerTest::writer_host = nullptr;
std::shared_ptr<HOST_INFO> FailoverHandlerTest::reader_host = nullptr;
std::shared_ptr<CLUSTER_TOPOLOGY_INFO> FailoverHandlerTest::topology = nullptr;

TEST_F(FailoverHandlerTest, NullDBC) {
    EXPECT_THROW(FAILOVER_HANDLER failover_handler(nullptr, ds), std::string);
}

TEST_F(FailoverHandlerTest, NullDS) {
    EXPECT_THROW(
        FAILOVER_HANDLER failover_handler(static_cast<DBC*>(dbc), nullptr),
        std::string);
}

TEST_F(FailoverHandlerTest, CustomDomain) {
    SQLCHAR server[] = "my-custom-domain.com";
    SQLCHAR host_pattern[] = "?.my-custom-domain.com";

    ds_setattr_from_utf8(&ds->server, server);
    ds_setattr_from_utf8(&ds->host_pattern, host_pattern);
    ds->port = 1234;

    EXPECT_CALL(*mock_ts, get_topology(_, false)).WillOnce(Return(topology));
    EXPECT_CALL(*mock_ts, set_cluster_instance_template(_))
        .Times(AtLeast(1));
    EXPECT_CALL(*mock_ts, set_cluster_id(_)).Times(0);

    EXPECT_CALL(*mock_connection_handler, do_connect(static_cast<DBC*>(dbc), ds))
        .WillOnce(Return(SQL_SUCCESS));

    FAILOVER_HANDLER failover_handler(static_cast<DBC*>(dbc), ds, mock_connection_handler, mock_ts);

    EXPECT_FALSE(failover_handler.is_rds());
    EXPECT_FALSE(failover_handler.is_rds_proxy());
    EXPECT_TRUE(failover_handler.is_cluster_topology_available());
    EXPECT_TRUE(failover_handler.is_failover_enabled());
}

TEST_F(FailoverHandlerTest, FailoverDisabled) {
    ds->disable_cluster_failover = true;

    EXPECT_CALL(*mock_connection_handler, do_connect(static_cast<DBC*>(dbc), ds)).Times(1);

    FAILOVER_HANDLER failover_handler(static_cast<DBC*>(dbc), ds,
                                      mock_connection_handler, mock_ts);

    EXPECT_FALSE(failover_handler.is_failover_enabled());
}

TEST_F(FailoverHandlerTest, IP_TopologyAvailable_PatternRequired) {
    SQLCHAR server[] = "10.10.10.10";

    ds_setattr_from_utf8(&ds->server, server);

    EXPECT_CALL(*mock_connection_handler, do_connect(static_cast<DBC*>(dbc), ds))
        .WillOnce(Return(SQL_SUCCESS));

    EXPECT_CALL(*mock_ts, get_topology(_, false)).WillOnce(Return(topology));

    EXPECT_THROW(FAILOVER_HANDLER failover_handler(static_cast<DBC*>(dbc), ds,
                                                   mock_connection_handler, mock_ts),
                 std::string);
}

TEST_F(FailoverHandlerTest, IP_TopologyNotAvailable) {
    SQLCHAR server[] = "10.10.10.10";

    ds_setattr_from_utf8(&ds->server, server);

    EXPECT_CALL(*mock_connection_handler, do_connect(static_cast<DBC*>(dbc), ds))
        .WillOnce(Return(SQL_SUCCESS));

    EXPECT_CALL(*mock_ts, get_topology(_, false))
        .WillOnce(Return(std::make_shared<CLUSTER_TOPOLOGY_INFO>()));

    FAILOVER_HANDLER failover_handler(static_cast<DBC*>(dbc), ds,
                                      mock_connection_handler, mock_ts);

    EXPECT_FALSE(failover_handler.is_rds());
    EXPECT_FALSE(failover_handler.is_rds_proxy());
    EXPECT_FALSE(failover_handler.is_cluster_topology_available());
    EXPECT_FALSE(failover_handler.is_failover_enabled());
}

TEST_F(FailoverHandlerTest, IP_Cluster) {
    SQLCHAR server[] = "10.10.10.10";
    SQLCHAR host_pattern[] = "?.my-custom-domain.com";

    ds_setattr_from_utf8(&ds->server, server);
    ds_setattr_from_utf8(&ds->host_pattern, host_pattern);

    EXPECT_CALL(*mock_connection_handler, do_connect(static_cast<DBC*>(dbc), ds))
        .WillOnce(Return(SQL_SUCCESS));

    EXPECT_CALL(*mock_ts, get_topology(_, false)).WillOnce(Return(topology));
    EXPECT_CALL(*mock_ts, set_cluster_instance_template(_)).Times(AtLeast(1));
    EXPECT_CALL(*mock_ts, set_cluster_id(_)).Times(0);

    FAILOVER_HANDLER failover_handler(static_cast<DBC*>(dbc), ds,
                                      mock_connection_handler, mock_ts);

    EXPECT_FALSE(failover_handler.is_rds());
    EXPECT_FALSE(failover_handler.is_rds_proxy());
    EXPECT_TRUE(failover_handler.is_cluster_topology_available());
    EXPECT_TRUE(failover_handler.is_failover_enabled());
}

TEST_F(FailoverHandlerTest, IP_Cluster_ClusterID) {
    SQLCHAR server[] = "10.10.10.10";
    SQLCHAR host_pattern[] = "?.my-custom-domain.com";
    SQLCHAR cluster_id[] = "test-cluster-id";

    ds_setattr_from_utf8(&ds->server, server);
    ds_setattr_from_utf8(&ds->host_pattern, host_pattern);
    ds_setattr_from_utf8(&ds->cluster_id, cluster_id);

    EXPECT_CALL(*mock_connection_handler, do_connect(static_cast<DBC*>(dbc), ds))
        .WillOnce(Return(SQL_SUCCESS));

    EXPECT_CALL(*mock_ts, get_topology(_, false)).WillOnce(Return(topology));
    EXPECT_CALL(*mock_ts, set_cluster_instance_template(_)).Times(AtLeast(1));
    EXPECT_CALL(*mock_ts, set_cluster_id(StrEq(reinterpret_cast<const char*>(cluster_id)))).Times(AtLeast(1));

    FAILOVER_HANDLER failover_handler(static_cast<DBC*>(dbc), ds, mock_connection_handler, mock_ts);

    EXPECT_FALSE(failover_handler.is_rds());
    EXPECT_FALSE(failover_handler.is_rds_proxy());
    EXPECT_TRUE(failover_handler.is_cluster_topology_available());
    EXPECT_TRUE(failover_handler.is_failover_enabled());
}

TEST_F(FailoverHandlerTest, RDS_Cluster) {
    SQLCHAR server[] = "my-cluster-name.cluster-XYZ.us-east-2.rds.amazonaws.com";

    ds_setattr_from_utf8(&ds->server, server);
    ds->port = 1234;

    EXPECT_CALL(*mock_connection_handler, do_connect(static_cast<DBC*>(dbc), ds))
        .WillOnce(Return(SQL_SUCCESS));

    EXPECT_CALL(*mock_ts, get_topology(_, false)).WillOnce(Return(topology));
    EXPECT_CALL(*mock_ts, set_cluster_instance_template(_)).Times(AtLeast(1));
    EXPECT_CALL(*mock_ts, set_cluster_id(StrEq("my-cluster-name.cluster-XYZ.us-east-2.rds.amazonaws.com:1234"))).Times(AtLeast(1));

    FAILOVER_HANDLER failover_handler(static_cast<DBC*>(dbc), ds,
                                      mock_connection_handler, mock_ts);

    EXPECT_TRUE(failover_handler.is_rds());
    EXPECT_FALSE(failover_handler.is_rds_proxy());
    EXPECT_TRUE(failover_handler.is_cluster_topology_available());
    EXPECT_TRUE(failover_handler.is_failover_enabled());
}

TEST_F(FailoverHandlerTest, RDS_CustomCluster) {
    SQLCHAR server[] = "my-custom-cluster-name.cluster-custom-XYZ.us-east-2.rds.amazonaws.com";

    ds_setattr_from_utf8(&ds->server, server);

    EXPECT_CALL(*mock_connection_handler, do_connect(static_cast<DBC*>(dbc), ds))
        .WillOnce(Return(SQL_SUCCESS));

    EXPECT_CALL(*mock_ts, get_topology(_, false)).WillOnce(Return(topology));
    EXPECT_CALL(*mock_ts, set_cluster_instance_template(_)).Times(AtLeast(1));
    EXPECT_CALL(*mock_ts, set_cluster_id(_)).Times(0);

    FAILOVER_HANDLER failover_handler(static_cast<DBC*>(dbc), ds,
                                      mock_connection_handler, mock_ts);

    EXPECT_TRUE(failover_handler.is_rds());
    EXPECT_FALSE(failover_handler.is_rds_proxy());
    EXPECT_TRUE(failover_handler.is_cluster_topology_available());
    EXPECT_TRUE(failover_handler.is_failover_enabled());
}

TEST_F(FailoverHandlerTest, RDS_Instance) {
    SQLCHAR server[] = "my-instance-name.XYZ.us-east-2.rds.amazonaws.com";

    ds_setattr_from_utf8(&ds->server, server);

    EXPECT_CALL(*mock_connection_handler, do_connect(static_cast<DBC*>(dbc), ds))
        .WillOnce(Return(SQL_SUCCESS));

    EXPECT_CALL(*mock_ts, get_topology(_, false)).WillOnce(Return(topology));
    EXPECT_CALL(*mock_ts, set_cluster_instance_template(_)).Times(AtLeast(1));
    EXPECT_CALL(*mock_ts, set_cluster_id(_)).Times(0);

    FAILOVER_HANDLER failover_handler(static_cast<DBC*>(dbc), ds,
                                      mock_connection_handler, mock_ts);

    EXPECT_TRUE(failover_handler.is_rds());
    EXPECT_FALSE(failover_handler.is_rds_proxy());
    EXPECT_TRUE(failover_handler.is_cluster_topology_available());
    EXPECT_TRUE(failover_handler.is_failover_enabled());
}

TEST_F(FailoverHandlerTest, RDS_Proxy) {
    SQLCHAR server[] = "test-proxy.proxy-XYZ.us-east-2.rds.amazonaws.com";

    ds_setattr_from_utf8(&ds->server, server);
    ds->port = 1234;

    EXPECT_CALL(*mock_connection_handler, do_connect(static_cast<DBC*>(dbc), ds))
        .WillOnce(Return(SQL_SUCCESS));

    EXPECT_CALL(*mock_ts, get_topology(_, false)).WillOnce(Return(topology));
    EXPECT_CALL(*mock_ts, set_cluster_instance_template(_)).Times(AtLeast(1));
    EXPECT_CALL(*mock_ts, set_cluster_id(StrEq("test-proxy.proxy-XYZ.us-east-2.rds.amazonaws.com:1234"))).Times(AtLeast(1));

    FAILOVER_HANDLER failover_handler(static_cast<DBC*>(dbc), ds,
                                      mock_connection_handler, mock_ts);

    EXPECT_TRUE(failover_handler.is_rds());
    EXPECT_TRUE(failover_handler.is_rds_proxy());
    EXPECT_TRUE(failover_handler.is_cluster_topology_available());
    EXPECT_FALSE(failover_handler.is_failover_enabled());
}

TEST_F(FailoverHandlerTest, RDS_ReaderCluster) {
    SQLCHAR server[] = "my-cluster-name.cluster-XYZ.us-east-2.rds.amazonaws.com";

    ds_setattr_from_utf8(&ds->server, server);
    ds->port = 1234;

    EXPECT_CALL(*mock_connection_handler, do_connect(static_cast<DBC*>(dbc), ds))
        .WillOnce(Return(SQL_SUCCESS));

    EXPECT_CALL(*mock_ts, get_topology(_, false)).WillOnce(Return(topology));
    EXPECT_CALL(*mock_ts, set_cluster_instance_template(_)).Times(AtLeast(1));
    EXPECT_CALL(*mock_ts,
                set_cluster_id(StrEq(
                    "my-cluster-name.cluster-XYZ.us-east-2.rds.amazonaws.com:1234")))
        .Times(AtLeast(1));

    FAILOVER_HANDLER failover_handler(static_cast<DBC*>(dbc), ds,
                                      mock_connection_handler, mock_ts);

    EXPECT_TRUE(failover_handler.is_rds());
    EXPECT_FALSE(failover_handler.is_rds_proxy());
    EXPECT_TRUE(failover_handler.is_cluster_topology_available());
    EXPECT_TRUE(failover_handler.is_failover_enabled());
}
