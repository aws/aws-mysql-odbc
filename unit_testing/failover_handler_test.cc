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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "test_utils.h"
#include "mock_objects.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::DeleteArg;
using ::testing::Return;
using ::testing::ReturnNew;
using ::testing::StrEq;

namespace {
    const std::string US_EAST_REGION_CLUSTER = "database-test-name.cluster-XYZ.us-east-2.rds.amazonaws.com";
    const std::string US_EAST_REGION_CLUSTER_READ_ONLY = "database-test-name.cluster-ro-XYZ.us-east-2.rds.amazonaws.com";
    const std::string US_EAST_REGION_PROXY = "proxy-test-name.proxy-XYZ.us-east-2.rds.amazonaws.com";
    const std::string US_EAST_REGION_CUSTON_DOMAIN = "custom-test-name.cluster-custom-XYZ.us-east-2.rds.amazonaws.com";

    const std::string CHINA_REGION_CLUSTER = "database-test-name.cluster-XYZ.rds.cn-northwest-1.amazonaws.com.cn";
    const std::string CHINA_REGION_CLUSTER_READ_ONLY = "database-test-name.cluster-ro-XYZ.rds.cn-northwest-1.amazonaws.com.cn";
    const std::string CHINA_REGION_PROXY = "proxy-test-name.proxy-XYZ.rds.cn-northwest-1.amazonaws.com.cn";
    const std::string CHINA_REGION_CUSTON_DOMAIN = "custom-test-name.cluster-custom-XYZ.rds.cn-northwest-1.amazonaws.com.cn";

}  // namespace

class FailoverHandlerTest : public testing::Test {
   protected:
    static std::shared_ptr<HOST_INFO> writer_host;
    static std::shared_ptr<HOST_INFO> reader_host;
    static std::shared_ptr<CLUSTER_TOPOLOGY_INFO> topology;
    SQLHENV env;
    DBC* dbc;
    DataSource* ds;
    std::shared_ptr<MOCK_TOPOLOGY_SERVICE> mock_ts;
    std::shared_ptr<MOCK_CONNECTION_HANDLER> mock_connection_handler;
    std::shared_ptr<MOCK_CLUSTER_AWARE_METRICS_CONTAINER> mock_metrics;

    static void SetUpTestSuite() {
        writer_host = std::make_shared<HOST_INFO>("writer-host.com", 1234, UP, true);
        reader_host = std::make_shared<HOST_INFO>("reader-host.com", 1234, UP, false);
        topology = std::make_shared<CLUSTER_TOPOLOGY_INFO>();
        topology->add_host(writer_host);
        topology->add_host(reader_host);
    }

    static void TearDownTestSuite() {}

    void SetUp() override {
        allocate_odbc_handles(env, dbc, ds);
        ds->enable_cluster_failover = true;
        ds->gather_perf_metrics = false;

        mock_ts = std::make_shared<MOCK_TOPOLOGY_SERVICE>();
        mock_connection_handler = std::make_shared<MOCK_CONNECTION_HANDLER>();
        mock_metrics = std::make_shared<MOCK_CLUSTER_AWARE_METRICS_CONTAINER>();
    }

    void TearDown() override {
        cleanup_odbc_handles(env, dbc, ds);
    }
};

std::shared_ptr<HOST_INFO> FailoverHandlerTest::writer_host = nullptr;
std::shared_ptr<HOST_INFO> FailoverHandlerTest::reader_host = nullptr;
std::shared_ptr<CLUSTER_TOPOLOGY_INFO> FailoverHandlerTest::topology = nullptr;

TEST_F(FailoverHandlerTest, NullDBC) {
    EXPECT_THROW(FAILOVER_HANDLER failover_handler(nullptr, ds), std::runtime_error);
}

TEST_F(FailoverHandlerTest, NullDS) {
    EXPECT_THROW(FAILOVER_HANDLER failover_handler(dbc, nullptr), std::runtime_error);
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

    EXPECT_CALL(*mock_connection_handler, do_connect_impl(dbc, ds, false, false))
        .WillOnce(Return(SQL_SUCCESS));
    EXPECT_CALL(*mock_connection_handler, do_connect_impl(dbc, ds, true, false))
        .WillOnce(Return(SQL_SUCCESS));

    FAILOVER_HANDLER failover_handler(dbc, ds, mock_connection_handler, mock_ts, mock_metrics);
    failover_handler.init_connection();

    EXPECT_FALSE(failover_handler.is_rds());
    EXPECT_FALSE(failover_handler.is_rds_proxy());
    EXPECT_TRUE(failover_handler.is_cluster_topology_available());
    EXPECT_TRUE(failover_handler.is_failover_enabled());
}

TEST_F(FailoverHandlerTest, FailoverDisabled) {
    ds->enable_cluster_failover = false;

    EXPECT_CALL(*mock_connection_handler, do_connect_impl(dbc, ds, false, false)).Times(1);

    FAILOVER_HANDLER failover_handler(dbc, ds, mock_connection_handler, mock_ts, mock_metrics);
    failover_handler.init_connection();

    EXPECT_FALSE(failover_handler.is_failover_enabled());
}

TEST_F(FailoverHandlerTest, IP_TopologyAvailable_PatternRequired) {
    SQLCHAR server[] = "10.10.10.10";

    ds_setattr_from_utf8(&ds->server, server);

    EXPECT_CALL(*mock_connection_handler, do_connect_impl(dbc, ds, false, false))
        .WillOnce(Return(SQL_SUCCESS));

    EXPECT_CALL(*mock_ts, get_topology(_, false)).WillOnce(Return(topology));

    FAILOVER_HANDLER failover_handler(dbc, ds, mock_connection_handler, mock_ts, mock_metrics);
    EXPECT_THROW(failover_handler.init_connection(), std::runtime_error);
}

TEST_F(FailoverHandlerTest, IP_TopologyNotAvailable) {
    SQLCHAR server[] = "10.10.10.10";

    ds_setattr_from_utf8(&ds->server, server);

    EXPECT_CALL(*mock_connection_handler, do_connect_impl(dbc, ds, false, false))
        .WillOnce(Return(SQL_SUCCESS));

    EXPECT_CALL(*mock_ts, get_topology(_, false))
        .WillOnce(Return(std::make_shared<CLUSTER_TOPOLOGY_INFO>()));

    FAILOVER_HANDLER failover_handler(dbc, ds, mock_connection_handler, mock_ts, mock_metrics);
    failover_handler.init_connection();

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

    EXPECT_CALL(*mock_connection_handler, do_connect_impl(dbc, ds, false, false))
        .WillOnce(Return(SQL_SUCCESS));
    EXPECT_CALL(*mock_connection_handler, do_connect_impl(dbc, ds, true, false))
        .WillOnce(Return(SQL_SUCCESS));

    EXPECT_CALL(*mock_ts, get_topology(_, false)).WillOnce(Return(topology));
    EXPECT_CALL(*mock_ts, set_cluster_instance_template(_)).Times(AtLeast(1));
    EXPECT_CALL(*mock_ts, set_cluster_id(_)).Times(0);

    FAILOVER_HANDLER failover_handler(dbc, ds, mock_connection_handler, mock_ts, mock_metrics);
    failover_handler.init_connection();

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

    EXPECT_CALL(*mock_connection_handler, do_connect_impl(dbc, ds, false, false))
        .WillOnce(Return(SQL_SUCCESS));
    EXPECT_CALL(*mock_connection_handler, do_connect_impl(dbc, ds, true, false))
        .WillOnce(Return(SQL_SUCCESS));

    EXPECT_CALL(*mock_ts, get_topology(_, false)).WillOnce(Return(topology));
    EXPECT_CALL(*mock_ts, set_cluster_instance_template(_)).Times(AtLeast(1));
    EXPECT_CALL(*mock_ts, set_cluster_id(StrEq(reinterpret_cast<const char*>(cluster_id)))).Times(AtLeast(1));

    FAILOVER_HANDLER failover_handler(dbc, ds, mock_connection_handler, mock_ts, mock_metrics);
    failover_handler.init_connection();

    EXPECT_FALSE(failover_handler.is_rds());
    EXPECT_FALSE(failover_handler.is_rds_proxy());
    EXPECT_TRUE(failover_handler.is_cluster_topology_available());
    EXPECT_TRUE(failover_handler.is_failover_enabled());
}

TEST_F(FailoverHandlerTest, RDS_Cluster) {
    SQLCHAR server[] = "my-cluster-name.cluster-XYZ.us-east-2.rds.amazonaws.com";

    ds_setattr_from_utf8(&ds->server, server);
    ds->port = 1234;

    EXPECT_CALL(*mock_connection_handler, do_connect_impl(dbc, ds, false, false))
        .WillOnce(Return(SQL_SUCCESS));
    EXPECT_CALL(*mock_connection_handler, do_connect_impl(dbc, ds, true, false))
        .WillOnce(Return(SQL_SUCCESS));

    EXPECT_CALL(*mock_ts, get_topology(_, false)).WillOnce(Return(topology));
    EXPECT_CALL(*mock_ts, set_cluster_instance_template(_)).Times(AtLeast(1));
    EXPECT_CALL(*mock_ts, set_cluster_id(StrEq("my-cluster-name.cluster-XYZ.us-east-2.rds.amazonaws.com:1234"))).Times(AtLeast(1));

    FAILOVER_HANDLER failover_handler(dbc, ds, mock_connection_handler, mock_ts, mock_metrics);
    failover_handler.init_connection();

    EXPECT_TRUE(failover_handler.is_rds());
    EXPECT_FALSE(failover_handler.is_rds_proxy());
    EXPECT_TRUE(failover_handler.is_cluster_topology_available());
    EXPECT_TRUE(failover_handler.is_failover_enabled());
}

TEST_F(FailoverHandlerTest, RDS_CustomCluster) {
    SQLCHAR server[] = "my-custom-cluster-name.cluster-custom-XYZ.us-east-2.rds.amazonaws.com";

    ds_setattr_from_utf8(&ds->server, server);

    EXPECT_CALL(*mock_connection_handler, do_connect_impl(dbc, ds, false, false))
        .WillOnce(Return(SQL_SUCCESS));
    EXPECT_CALL(*mock_connection_handler, do_connect_impl(dbc, ds, true, false))
        .WillOnce(Return(SQL_SUCCESS));

    EXPECT_CALL(*mock_ts, get_topology(_, false)).WillOnce(Return(topology));
    EXPECT_CALL(*mock_ts, set_cluster_instance_template(_)).Times(AtLeast(1));
    EXPECT_CALL(*mock_ts, set_cluster_id(_)).Times(1);

    FAILOVER_HANDLER failover_handler(dbc, ds, mock_connection_handler, mock_ts, mock_metrics);
    failover_handler.init_connection();

    EXPECT_TRUE(failover_handler.is_rds());
    EXPECT_FALSE(failover_handler.is_rds_proxy());
    EXPECT_TRUE(failover_handler.is_cluster_topology_available());
    EXPECT_TRUE(failover_handler.is_failover_enabled());
}

TEST_F(FailoverHandlerTest, RDS_Instance) {
    SQLCHAR server[] = "my-instance-name.XYZ.us-east-2.rds.amazonaws.com";

    ds_setattr_from_utf8(&ds->server, server);

    EXPECT_CALL(*mock_connection_handler, do_connect_impl(dbc, ds, false, false))
        .WillOnce(Return(SQL_SUCCESS));
    EXPECT_CALL(*mock_connection_handler, do_connect_impl(dbc, ds, true, false))
        .WillOnce(Return(SQL_SUCCESS));

    EXPECT_CALL(*mock_ts, get_topology(_, false)).WillOnce(Return(topology));
    EXPECT_CALL(*mock_ts, set_cluster_instance_template(_)).Times(AtLeast(1));
    EXPECT_CALL(*mock_ts, set_cluster_id(_)).Times(1);

    FAILOVER_HANDLER failover_handler(dbc, ds, mock_connection_handler, mock_ts, mock_metrics);
    failover_handler.init_connection();

    EXPECT_TRUE(failover_handler.is_rds());
    EXPECT_FALSE(failover_handler.is_rds_proxy());
    EXPECT_TRUE(failover_handler.is_cluster_topology_available());
    EXPECT_TRUE(failover_handler.is_failover_enabled());
}

TEST_F(FailoverHandlerTest, RDS_Proxy) {
    SQLCHAR server[] = "test-proxy.proxy-XYZ.us-east-2.rds.amazonaws.com";

    ds_setattr_from_utf8(&ds->server, server);
    ds->port = 1234;

    EXPECT_CALL(*mock_connection_handler, do_connect_impl(dbc, ds, false, false))
        .WillOnce(Return(SQL_SUCCESS));

    EXPECT_CALL(*mock_ts, get_topology(_, false)).WillOnce(Return(topology));
    EXPECT_CALL(*mock_ts, set_cluster_instance_template(_)).Times(AtLeast(1));
    EXPECT_CALL(*mock_ts, set_cluster_id(StrEq("test-proxy.proxy-XYZ.us-east-2.rds.amazonaws.com:1234"))).Times(AtLeast(1));

    FAILOVER_HANDLER failover_handler(dbc, ds, mock_connection_handler, mock_ts, mock_metrics);
    failover_handler.init_connection();

    EXPECT_TRUE(failover_handler.is_rds());
    EXPECT_TRUE(failover_handler.is_rds_proxy());
    EXPECT_TRUE(failover_handler.is_cluster_topology_available());
    EXPECT_FALSE(failover_handler.is_failover_enabled());
}

TEST_F(FailoverHandlerTest, RDS_ReaderCluster) {
    SQLCHAR server[] = "my-cluster-name.cluster-XYZ.us-east-2.rds.amazonaws.com";

    ds_setattr_from_utf8(&ds->server, server);
    ds->port = 1234;

    EXPECT_CALL(*mock_connection_handler, do_connect_impl(dbc, ds, false, false))
        .WillOnce(Return(SQL_SUCCESS));
    EXPECT_CALL(*mock_connection_handler, do_connect_impl(dbc, ds, true, false))
        .WillOnce(Return(SQL_SUCCESS));

    EXPECT_CALL(*mock_ts, get_topology(_, false)).WillOnce(Return(topology));
    EXPECT_CALL(*mock_ts, set_cluster_instance_template(_)).Times(AtLeast(1));
    EXPECT_CALL(*mock_ts,
                set_cluster_id(StrEq(
                    "my-cluster-name.cluster-XYZ.us-east-2.rds.amazonaws.com:1234")))
        .Times(AtLeast(1));

    FAILOVER_HANDLER failover_handler(dbc, ds, mock_connection_handler, mock_ts, mock_metrics);
    failover_handler.init_connection();

    EXPECT_TRUE(failover_handler.is_rds());
    EXPECT_FALSE(failover_handler.is_rds_proxy());
    EXPECT_TRUE(failover_handler.is_cluster_topology_available());
    EXPECT_TRUE(failover_handler.is_failover_enabled());
}

TEST_F(FailoverHandlerTest, ReconnectWithFailoverSettings) {
    SQLCHAR server[] = "10.10.10.10";
    SQLCHAR host_pattern[] = "?.my-custom-domain.com";

    ds_setattr_from_utf8(&ds->server, server);
    ds_setattr_from_utf8(&ds->host_pattern, host_pattern);
    ds->connect_timeout = 100;
    ds->network_timeout = 100;

    EXPECT_CALL(*mock_ts, get_topology(_, false)).WillOnce(Return(topology));
    EXPECT_CALL(*mock_ts, set_cluster_instance_template(_))
        .Times(AtLeast(1));

    EXPECT_CALL(*mock_connection_handler, do_connect_impl(dbc, ds, false, false))
        .WillOnce(Return(SQL_SUCCESS));

    EXPECT_CALL(*mock_connection_handler, do_connect_impl(dbc, ds, true, false))
        .WillOnce(Return(SQL_SUCCESS));

    FAILOVER_HANDLER failover_handler(dbc, ds, mock_connection_handler, mock_ts, mock_metrics);
    failover_handler.init_connection();

    EXPECT_TRUE(failover_handler.is_failover_enabled());
}

TEST_F(FailoverHandlerTest, IsRdsDns) {
    EXPECT_TRUE(TEST_UTILS::is_rds_dns(US_EAST_REGION_CLUSTER));
    EXPECT_TRUE(TEST_UTILS::is_rds_dns(US_EAST_REGION_CLUSTER_READ_ONLY));
    EXPECT_TRUE(TEST_UTILS::is_rds_dns(US_EAST_REGION_PROXY));
    EXPECT_TRUE(TEST_UTILS::is_rds_dns(US_EAST_REGION_CUSTON_DOMAIN));

    EXPECT_TRUE(TEST_UTILS::is_rds_dns(CHINA_REGION_CLUSTER));
    EXPECT_TRUE(TEST_UTILS::is_rds_dns(CHINA_REGION_CLUSTER_READ_ONLY));
    EXPECT_TRUE(TEST_UTILS::is_rds_dns(CHINA_REGION_PROXY));
    EXPECT_TRUE(TEST_UTILS::is_rds_dns(CHINA_REGION_CUSTON_DOMAIN));
}

TEST_F(FailoverHandlerTest, IsRdsClusterDns) {
    EXPECT_TRUE(TEST_UTILS::is_rds_cluster_dns(US_EAST_REGION_CLUSTER));
    EXPECT_TRUE(TEST_UTILS::is_rds_cluster_dns(US_EAST_REGION_CLUSTER_READ_ONLY));
    EXPECT_FALSE(TEST_UTILS::is_rds_cluster_dns(US_EAST_REGION_PROXY));
    EXPECT_FALSE(TEST_UTILS::is_rds_cluster_dns(US_EAST_REGION_CUSTON_DOMAIN));

    EXPECT_TRUE(TEST_UTILS::is_rds_cluster_dns(CHINA_REGION_CLUSTER));
    EXPECT_TRUE(TEST_UTILS::is_rds_cluster_dns(CHINA_REGION_CLUSTER_READ_ONLY));
    EXPECT_FALSE(TEST_UTILS::is_rds_cluster_dns(CHINA_REGION_PROXY));
    EXPECT_FALSE(TEST_UTILS::is_rds_cluster_dns(CHINA_REGION_CUSTON_DOMAIN));
}

TEST_F(FailoverHandlerTest, IsRdsProxyDns) {
    EXPECT_FALSE(TEST_UTILS::is_rds_proxy_dns(US_EAST_REGION_CLUSTER));
    EXPECT_FALSE(TEST_UTILS::is_rds_proxy_dns(US_EAST_REGION_CLUSTER_READ_ONLY));
    EXPECT_TRUE(TEST_UTILS::is_rds_proxy_dns(US_EAST_REGION_PROXY));
    EXPECT_FALSE(TEST_UTILS::is_rds_proxy_dns(US_EAST_REGION_CUSTON_DOMAIN));

    EXPECT_FALSE(TEST_UTILS::is_rds_proxy_dns(CHINA_REGION_CLUSTER));
    EXPECT_FALSE(TEST_UTILS::is_rds_proxy_dns(CHINA_REGION_CLUSTER_READ_ONLY));
    EXPECT_TRUE(TEST_UTILS::is_rds_proxy_dns(CHINA_REGION_PROXY));
    EXPECT_FALSE(TEST_UTILS::is_rds_proxy_dns(CHINA_REGION_CUSTON_DOMAIN));
}

TEST_F(FailoverHandlerTest, IsRdsWriterClusterDns) {
    EXPECT_TRUE(TEST_UTILS::is_rds_writer_cluster_dns(US_EAST_REGION_CLUSTER));
    EXPECT_FALSE(TEST_UTILS::is_rds_writer_cluster_dns(US_EAST_REGION_CLUSTER_READ_ONLY));
    EXPECT_FALSE(TEST_UTILS::is_rds_writer_cluster_dns(US_EAST_REGION_PROXY));
    EXPECT_FALSE(TEST_UTILS::is_rds_writer_cluster_dns(US_EAST_REGION_CUSTON_DOMAIN));

    EXPECT_TRUE(TEST_UTILS::is_rds_writer_cluster_dns(CHINA_REGION_CLUSTER));
    EXPECT_FALSE(TEST_UTILS::is_rds_writer_cluster_dns(CHINA_REGION_CLUSTER_READ_ONLY));
    EXPECT_FALSE(TEST_UTILS::is_rds_writer_cluster_dns(CHINA_REGION_PROXY));
    EXPECT_FALSE(TEST_UTILS::is_rds_writer_cluster_dns(CHINA_REGION_CUSTON_DOMAIN));
}

TEST_F(FailoverHandlerTest, IsRdsCustomClusterDns) {
    EXPECT_FALSE(TEST_UTILS::is_rds_custom_cluster_dns(US_EAST_REGION_CLUSTER));
    EXPECT_FALSE(TEST_UTILS::is_rds_custom_cluster_dns(US_EAST_REGION_CLUSTER_READ_ONLY));
    EXPECT_FALSE(TEST_UTILS::is_rds_custom_cluster_dns(US_EAST_REGION_PROXY));
    EXPECT_TRUE(TEST_UTILS::is_rds_custom_cluster_dns(US_EAST_REGION_CUSTON_DOMAIN));

    EXPECT_FALSE(TEST_UTILS::is_rds_custom_cluster_dns(CHINA_REGION_CLUSTER));
    EXPECT_FALSE(TEST_UTILS::is_rds_custom_cluster_dns(CHINA_REGION_CLUSTER_READ_ONLY));
    EXPECT_FALSE(TEST_UTILS::is_rds_custom_cluster_dns(CHINA_REGION_PROXY));
    EXPECT_TRUE(TEST_UTILS::is_rds_custom_cluster_dns(CHINA_REGION_CUSTON_DOMAIN));
}

TEST_F(FailoverHandlerTest, GetRdsInstanceHostPattern) {
    const std::string expected_pattern = "?.XYZ.us-east-2.rds.amazonaws.com";
    EXPECT_EQ(expected_pattern, TEST_UTILS::get_rds_instance_host_pattern(US_EAST_REGION_CLUSTER));
    EXPECT_EQ(expected_pattern, TEST_UTILS::get_rds_instance_host_pattern(US_EAST_REGION_CLUSTER_READ_ONLY));
    EXPECT_EQ(expected_pattern, TEST_UTILS::get_rds_instance_host_pattern(US_EAST_REGION_PROXY));
    EXPECT_EQ(expected_pattern, TEST_UTILS::get_rds_instance_host_pattern(US_EAST_REGION_CUSTON_DOMAIN));

    const std::string expected_china_pattern = "?.XYZ.rds.cn-northwest-1.amazonaws.com.cn";
    EXPECT_EQ(expected_china_pattern, TEST_UTILS::get_rds_instance_host_pattern(CHINA_REGION_CLUSTER));
    EXPECT_EQ(expected_china_pattern, TEST_UTILS::get_rds_instance_host_pattern(CHINA_REGION_CLUSTER_READ_ONLY));
    EXPECT_EQ(expected_china_pattern, TEST_UTILS::get_rds_instance_host_pattern(CHINA_REGION_PROXY));
    EXPECT_EQ(expected_china_pattern, TEST_UTILS::get_rds_instance_host_pattern(CHINA_REGION_CUSTON_DOMAIN));
}

TEST_F(FailoverHandlerTest, GetRdsClusterHostUrl) {
    EXPECT_EQ(US_EAST_REGION_CLUSTER, TEST_UTILS::get_rds_cluster_host_url(US_EAST_REGION_CLUSTER));
    EXPECT_EQ(US_EAST_REGION_CLUSTER, TEST_UTILS::get_rds_cluster_host_url(US_EAST_REGION_CLUSTER_READ_ONLY));
    EXPECT_EQ(std::string(), TEST_UTILS::get_rds_cluster_host_url(US_EAST_REGION_PROXY));
    EXPECT_EQ(std::string(), TEST_UTILS::get_rds_cluster_host_url(US_EAST_REGION_CUSTON_DOMAIN));

    EXPECT_EQ(CHINA_REGION_CLUSTER, TEST_UTILS::get_rds_cluster_host_url(CHINA_REGION_CLUSTER));
    EXPECT_EQ(CHINA_REGION_CLUSTER, TEST_UTILS::get_rds_cluster_host_url(CHINA_REGION_CLUSTER_READ_ONLY));
    EXPECT_EQ(std::string(), TEST_UTILS::get_rds_cluster_host_url(CHINA_REGION_PROXY));
    EXPECT_EQ(std::string(), TEST_UTILS::get_rds_cluster_host_url(CHINA_REGION_CUSTON_DOMAIN));
}

TEST_F(FailoverHandlerTest, ConnectToNewWriter) {
    SQLCHAR server[] = "my-cluster-name.cluster-XYZ.us-east-2.rds.amazonaws.com";

    EXPECT_CALL(*mock_connection_handler, do_connect_impl(dbc, ds, _, false))
        .WillOnce(Return(SQL_SUCCESS))
        .WillOnce(Return(SQL_SUCCESS));

    ds_setattr_from_utf8(&ds->server, server);
    ds->port = 1234;
    ds->enable_cluster_failover = true;

    auto mock_proxy = new MOCK_CONNECTION_PROXY(dbc, ds);
    delete dbc->connection_proxy;
    dbc->connection_proxy = mock_proxy;
    
    EXPECT_CALL(*mock_proxy, query(_))
        .WillOnce(Return(0));

    EXPECT_CALL(*mock_proxy, store_result())
        .WillOnce(ReturnNew<MYSQL_RES>());

    char* row[1] = { "1" };
    EXPECT_CALL(*mock_proxy, fetch_row(_))
        .WillOnce(Return(row));

    EXPECT_CALL(*mock_proxy, free_result(_))
        .WillOnce(DeleteArg<0>());

    auto topology = std::make_shared<CLUSTER_TOPOLOGY_INFO>();
    topology->add_host(writer_host);
    topology->add_host(reader_host);

    EXPECT_CALL(*mock_ts, get_topology(_, false))
        .WillOnce(Return(topology));
    EXPECT_CALL(*mock_ts, get_topology(_, true))
        .WillOnce(Return(topology));
    
    auto mock_failover_handler = std::make_shared<MOCK_FAILOVER_HANDLER>(dbc, ds, mock_connection_handler, mock_ts, mock_metrics);
    EXPECT_CALL(*mock_failover_handler, host_to_IP(_))
        .WillOnce(Return("10.10.10.10"))
        .WillOnce(Return("20.20.20.20"));

    mock_failover_handler->init_connection();
    
    EXPECT_EQ(std::string("writer-host.com"), std::string((const char*)ds->server8));
}
