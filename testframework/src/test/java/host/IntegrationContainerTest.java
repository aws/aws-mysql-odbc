/*
 * AWS ODBC Driver for MySQL
 * Copyright Amazon.com Inc. or affiliates.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation and/or
 * other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

package host;

import com.amazonaws.util.StringUtils;
import java.io.IOException;
import java.net.UnknownHostException;
import java.util.ArrayList;
import java.util.List;
import org.junit.jupiter.api.AfterAll;
import org.junit.jupiter.api.BeforeAll;
import org.junit.jupiter.api.Test;
import org.testcontainers.containers.GenericContainer;
import org.testcontainers.containers.Network;
import org.testcontainers.containers.ToxiproxyContainer;
import utility.AuroraClusterInfo;
import utility.AuroraTestUtility;
import utility.ContainerHelper;

public class IntegrationContainerTest {
  private static final int MYSQL_PORT = 3306;
  private static final String TEST_CONTAINER_NAME = "test-container";
  private static final String TEST_DATABASE = "test";
  private static final String TEST_DSN = "atlas";
  private static final String TEST_USERNAME =
      !StringUtils.isNullOrEmpty(System.getenv("TEST_USERNAME")) ?
          System.getenv("TEST_USERNAME") : "my_test_username";
  private static final String TEST_PASSWORD =
      !StringUtils.isNullOrEmpty(System.getenv("TEST_PASSWORD")) ?
          System.getenv("TEST_PASSWORD") : "my_test_password";
  private static final String ACCESS_KEY = System.getenv("AWS_ACCESS_KEY_ID");
  private static final String SECRET_ACCESS_KEY = System.getenv("AWS_SECRET_ACCESS_KEY");
  private static final String SESSION_TOKEN = System.getenv("AWS_SESSION_TOKEN");

  private static final String DRIVER_LOCATION = System.getenv("DRIVER_PATH");
  private static final String TEST_DB_CLUSTER_IDENTIFIER = System.getenv("TEST_DB_CLUSTER_IDENTIFIER");
  private static final String PROXIED_DOMAIN_NAME_SUFFIX = ".proxied";
  private static List<ToxiproxyContainer> proxyContainers = new ArrayList<>();

  private static final ContainerHelper containerHelper = new ContainerHelper();
  private static final AuroraTestUtility auroraUtil = new AuroraTestUtility("us-east-2");

  private static int mySQLProxyPort;
  private static List<String> mySqlInstances = new ArrayList<>();
  private static GenericContainer<?> integrationTestContainer;
  private static GenericContainer<?> mysqlContainer;
  private static String dbHostCluster = "";
  private static String dbHostClusterRo = "";
  private static String runnerIP = null;
  private static String dbConnStrSuffix = "";

  @BeforeAll
  static void setUp() throws InterruptedException, UnknownHostException {
    // Comment out below to not create a new cluster & instances
    AuroraClusterInfo clusterInfo = auroraUtil.createCluster(TEST_USERNAME, TEST_PASSWORD, TEST_DB_CLUSTER_IDENTIFIER);

    // Comment out getting public IP to not add & remove from EC2 whitelist
    runnerIP = auroraUtil.getPublicIPAddress();
    auroraUtil.ec2AuthorizeIP(runnerIP);

    dbConnStrSuffix = clusterInfo.getClusterSuffix();
    dbHostCluster = clusterInfo.getClusterEndpoint();
    dbHostClusterRo = clusterInfo.getClusterROEndpoint();

    Network network = Network.newNetwork();
    mysqlContainer = ContainerHelper.createMysqlContainer(network);
    mysqlContainer.start();

    mySqlInstances = clusterInfo.getInstances();

    proxyContainers = containerHelper.createProxyContainers(network, mySqlInstances, PROXIED_DOMAIN_NAME_SUFFIX);
    for (ToxiproxyContainer container : proxyContainers) {
      container.start();
    }
    mySQLProxyPort = containerHelper.createAuroraInstanceProxies(mySqlInstances, proxyContainers, MYSQL_PORT);

    proxyContainers.add(containerHelper.createAndStartProxyContainer(
        network,
        "toxiproxy-instance-cluster",
        dbHostCluster + PROXIED_DOMAIN_NAME_SUFFIX,
        dbHostCluster,
        MYSQL_PORT,
        mySQLProxyPort)
    );

    proxyContainers.add(containerHelper.createAndStartProxyContainer(
        network,
        "toxiproxy-ro-instance-cluster",
        dbHostClusterRo + PROXIED_DOMAIN_NAME_SUFFIX,
        dbHostClusterRo,
        MYSQL_PORT,
        mySQLProxyPort)
    );

    integrationTestContainer = initializeTestContainer(network);
  }

  @AfterAll
  static void tearDown() {
    if (StringUtils.isNullOrEmpty(TEST_DB_CLUSTER_IDENTIFIER)) {
      auroraUtil.deleteCluster();
    } else {
      auroraUtil.deleteCluster(TEST_DB_CLUSTER_IDENTIFIER);
    }

    auroraUtil.ec2DeauthorizesIP(runnerIP);

    for (ToxiproxyContainer proxy : proxyContainers) {
      proxy.stop();
    }
    integrationTestContainer.stop();
    mysqlContainer.stop();
  }

  @Test
  public void testRunTestInContainer()
      throws UnsupportedOperationException, IOException, InterruptedException {
    containerHelper.runCTest(integrationTestContainer, "test");
  }

  @Test
  public void testRunFailoverTestInContainer()
      throws UnsupportedOperationException, IOException, InterruptedException {
    containerHelper.runExecutable(integrationTestContainer, "integration/bin", "integration");
  }

  protected static GenericContainer<?> initializeTestContainer(final Network network) {
    final GenericContainer<?> container = containerHelper.createTestContainer(
            "odbc/rds-test-container",
            DRIVER_LOCATION)
        .withNetworkAliases(TEST_CONTAINER_NAME)
        .withNetwork(network)
        .withEnv("TEST_DSN", TEST_DSN)
        .withEnv("TEST_UID", TEST_USERNAME)
        .withEnv("TEST_PASSWORD", TEST_PASSWORD)
        .withEnv("AWS_ACCESS_KEY_ID", ACCESS_KEY)
        .withEnv("AWS_SECRET_ACCESS_KEY", SECRET_ACCESS_KEY)
        .withEnv("TEST_DATABASE", TEST_DATABASE)
        .withEnv("AWS_ACCESS_KEY_ID", ACCESS_KEY)
        .withEnv("AWS_SECRET_ACCESS_KEY", SECRET_ACCESS_KEY)
        .withEnv("AWS_SESSION_TOKEN", SESSION_TOKEN)
        .withEnv("TEST_SERVER", dbHostCluster)
        .withEnv("TEST_RO_SERVER", dbHostClusterRo)
        .withEnv("TOXIPROXY_CLUSTER_NETWORK_ALIAS", "toxiproxy-instance-cluster")
        .withEnv("TOXIPROXY_RO_CLUSTER_NETWORK_ALIAS", "toxiproxy-ro-instance-cluster")
        .withEnv("PROXIED_DOMAIN_NAME_SUFFIX", PROXIED_DOMAIN_NAME_SUFFIX)
        .withEnv("PROXIED_CLUSTER_TEMPLATE", "?." + dbConnStrSuffix + PROXIED_DOMAIN_NAME_SUFFIX)
        .withEnv("DB_CONN_STR_SUFFIX", "." + dbConnStrSuffix)
        .withEnv("ODBCINI", "/etc/odbc.ini")
        .withEnv("ODBCINSTINI", "/etc/odbcinst.ini");

    // Add mysql instances & proxies to container env
    for (int i = 0; i < mySqlInstances.size(); i++) {
      // Add instance
      container.addEnv(
          "MYSQL_INSTANCE_" + (i + 1) + "_URL",
          mySqlInstances.get(i));

      // Add proxies
      container.addEnv(
          "TOXIPROXY_INSTANCE_" + (i + 1) + "_NETWORK_ALIAS",
          "toxiproxy-instance-" + (i + 1));
    }
    container.addEnv("MYSQL_PORT", Integer.toString(MYSQL_PORT));
    container.addEnv("PROXIED_DOMAIN_NAME_SUFFIX", PROXIED_DOMAIN_NAME_SUFFIX);
    container.addEnv("MYSQL_PROXY_PORT", Integer.toString(mySQLProxyPort));

    System.out.println("Toxyproxy Instances port: " + mySQLProxyPort);
    System.out.println("Instances Proxied: " + mySqlInstances.size());

    container.start();
    return container;
  }
}
