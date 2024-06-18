# Testing the AWS ODBC Driver for MySQL
## Unit Tests
1. Build driver binaries with `ENABLE_UNIT_TESTS` command set to `TRUE`:
   - **Windows**
    
       Add AWS SDK dynamic libraries path to environment variable `path` so the dlls could be found when unit tests are running.
     
     - For release version unit tests, run the following command from repository root
        ```
        cmake -S . -B build -G "Visual Studio 17 2022" -DMYSQL_DIR="C:\Program Files\MySQL\MySQL Server 8.3" -DMYSQLCLIENT_STATIC_LINKING=TRUE -DENABLE_UNIT_TESTS=TRUE
        cmake --build build --config Release
        ```
     - For debug version unit tests

       - Download MySQL debug binaries from [MySQL::Download MySQL Community Server](https://dev.mysql.com/downloads/mysql/).

       - Extract the `mysqlclient.lib` from debug binaries. Make sure to backup your release version of mysqlclient.lib in `MYSQL_DIR\lib`, e.g. `C:\Program Files\MySQL\MySQL Server 8.3\lib`. Replace release version `mysqlclient.lib` with debug version of `mysqlclient.lib`.
       - Run the following command from repository root

        ```
        cmake -S . -B build -G "Visual Studio 17 2022" -DMYSQL_DIR="C:\Program Files\MySQL\MySQL Server 8.3" -DMYSQLCLIENT_STATIC_LINKING=TRUE -DENABLE_UNIT_TESTS=TRUE -DCMAKE_BUILD_TYPE=Debug
        cmake --build build --config Debug
        ```
   - **MacOS**
      - Run the following command from repository root 
        ```
        cmake -S . -B build -G "Unix Makefiles" -DMYSQLCLIENT_STATIC_LINKING=true -DODBC_INCLUDES=/usr/local/Cellar/libiodbc/3.52.16/include -DENABLE_UNIT_TESTS=TRUE
        cmake --build build --config Release
        ```
   - **Linux**
      - Run the following command from repository root
        ```
        cmake -S . -B build -G "Unix Makefiles" -DMYSQLCLIENT_STATIC_LINKING=true -DWITH_UNIXODBC=1 -DENABLE_UNIT_TESTS=TRUE
        cmake --build build --config Release
        ```
2. To run the unit test, AWS SDK dynamic link libraries path needs to be added to system environment `path` firstly for Windows. The AWS SDK dynamic link libraries could be found under `<repository root>/build/lib` or `<repository root>/build/lib/<build type>`. 

   There are two options to run the unit tests:
    - Run `ctest` directly from the `build/unit_testing` directory.
    - Navigate to `unit_testing/bin/Release` and run `unit_testing.exe`. To specify a particular test or test suite, include `--gtest_filter` in the command.

The following example demonstrates running all the tests in the `TopologyServiceTest` suite with the `.\unit_testing.exe --gtest_filter=TopologyServiceTest.*` command:

```
PS C:\Other\dev\aws-mysql-odbc\unit_testing\bin\Release> .\unit_testing.exe --gtest_filter=TopologyServiceTest.*
Running main() from C:\Other\dev\aws-mysql-odbc\_deps\googletest-src\googletest\src\gtest_main.cc
Note: Google Test filter = TopologyServiceTest.*
[==========] Running 7 tests from 1 test suite.
[----------] Global test environment set-up.
[----------] 7 tests from TopologyServiceTest
[ RUN      ] TopologyServiceTest.TopologyQuery
[       OK ] TopologyServiceTest.TopologyQuery (0 ms)
[ RUN      ] TopologyServiceTest.MultiWriter
[       OK ] TopologyServiceTest.MultiWriter (0 ms)
[ RUN      ] TopologyServiceTest.CachedTopology
[       OK ] TopologyServiceTest.CachedTopology (0 ms)
[ RUN      ] TopologyServiceTest.QueryFailure
[       OK ] TopologyServiceTest.QueryFailure (0 ms)
[ RUN      ] TopologyServiceTest.StaleTopology
[       OK ] TopologyServiceTest.StaleTopology (1007 ms)
[ RUN      ] TopologyServiceTest.RefreshTopology
[       OK ] TopologyServiceTest.RefreshTopology (1013 ms)
[ RUN      ] TopologyServiceTest.ClearCache
[       OK ] TopologyServiceTest.ClearCache (0 ms)
[----------] 7 tests from TopologyServiceTest (2026 ms total)

[----------] Global test environment tear-down
[==========] 7 tests from 1 test suite ran. (2030 ms total)
[  PASSED  ] 7 tests.
```

## Integration Tests
There are two types of integration tests you can run. One type is an integration test against a MySQL Server, and the other type consists of the two sets of integration tests specific to the failover functionality provided by the AWS ODBC Driver for MySQL.

### Integration Tests Against A MySQL Server

#### Prerequisites
- Install MySQL Server. See the [build instructions for the desired system](../building-the-aws-driver/BuildingTheAwsDriver.md) for instructions.
- [**Optional**] Install [Docker](https://docs.docker.com/get-docker/).

#### Steps
1. Specify the following environment variables on your target platform before building the driver:
    | Environment Variable | Description                                                     | Example                         | Platforms               |
    |----------------------|-----------------------------------------------------------------|---------------------------------|-------------------------|
    | TEST_DSN             | The DSN to use for the test                                     | AWSODBCDriverDSN                | All systems             |
    | TEST_USERNAME        | The name of the user with access to the MySQL Server            | root                            | All systems             |
    | TEST_PASSWORD        | The password for the test database user                         | root                            | All systems             |
    | TEST_DATABASE        | The test database                                               | test                            | All systems             |
    | DYLD_LIBRARY_PATH    | The path to the library folder of your MySQL server directory   | /usr/local/opt/mysql-client/lib | MacOS systems           |
    | ODBCINI              | The path to your odbc.ini file                                  | /etc/odbc.ini                   | MacOS and Linux systems |
    | ODBCINSTINI          | The path to your odbcinst.ini file                              | /etc/odbcinst.ini               | MacOS and Linux systems |
    > **NOTE:** The `TEST_PASSWORD` environment variable is only required if you have specified a password for the `root` user when installing the MySQL Server.
2. Build and install the driver for a specific platform as described in [Installing the AWS ODBC Driver for MySQL](../GettingStarted.md#installing-the-aws-odbc-driver-for-mysql).
3. Start the MySQL Server. You may either start a local server or use a docker images. 
4. [**Optional**] To start the MySQL Server via a Docker image. Navigate to `test/docker` and execute `docker-compose up -d` to start the server in the background.
5. Navigate to the `test` directory and execute `ctest`.

### Failover Specific Integration Tests
> **NOTE:** This set of tests can only be run on Linux at the moment.

#### Prerequisites
- Install JDK 8:
  ```
  sudo apt-get install openjdk-8-jdk
  ```
- Install [Docker](https://docs.docker.com/get-docker/)
  
#### Steps
> **NOTE:** Running these tests will automatically create an Amazon Aurora MySQL DB cluster with at least 5 instances and may induce a cost. Ensure the test cluster is cleaned up after testing on the [Amazon RDS Management Console](https://console.aws.amazon.com/rds/home).

1. This set of tests runs against an Amazon Aurora MySQL DB cluster with at least 5 instances. The test will automatically generate the required AWS MySQL DB cluster and instances if proper AWS credentials are set up. Refer to the [documentation](https://docs.aws.amazon.com/AmazonRDS/latest/AuroraUserGuide/CHAP_SettingUp_Aurora.html) for information about setting up a development environment for Amazon Aurora.
2. Define the following environment variables:
    | Environment Variable       | Description                                                                                                         | Example                                    |
    |----------------------------|---------------------------------------------------------------------------------------------------------------------|--------------------------------------------|
    | TEST_DSN                   | The DSN to use for the test.                                                                                        | AWSODBCDriverDSN                           |
    | TEST_USERNAME              | The name of the user with access to the Amazon Aurora MySQL DB cluster.                                             | username                                   |
    | TEST_PASSWORD              | The password for the test database user.                                                                            | password                                   |
    | TEST_DB_CLUSTER_IDENTIFIER | The unique identifier for the Amazon Aurora MySQL DB cluster.                                                       | db-identifier                              |
    | AWS_ACCESS_KEY_ID          | The access key ID to your AWS account.                                                                              | `ASIAIOSFODNN7EXAMPLE`                     |
    | AWS_SECRET_ACCESS_KEY      | The secret access key for your AWS account.                                                                         | `wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY` |
    | AWS_SESSION_TOKEN          | The AWS session token for your AWS count. This is only required if you have set up temporary security credentials.  | `AQoDYXdzEJr...`                           |
    | DRIVER_PATH                | The directory where the driver was built to.                                                                        | `~/dev/aws-mysql-odbc/build`               |
3. Ensure the following packages are installed on Linux:
    ```
    sudo apt-get update && sudo apt-get install \
                build-essential \
                libgtk-3-dev \
                libmysqlclient-dev \
                unixodbc \
                unixodbc-dev \
                curl  \
                libcurl4-openssl-dev
    ```
4. Build the driver with the following commands (the following commands assume the driver source code is stored within `~/dev/aws-mysql-odbc`):
   ```
    cmake -E make_directory ~/dev/aws-mysql-odbc/build
    cmake -S . -B build \
        -G "Unix Makefiles" \
        -DCMAKE_BUILD_TYPE=Release \
        -DMYSQLCLIENT_STATIC_LINKING=TRUE \
        -DENABLE_INTEGRATION_TESTS=TRUE \
        -DWITH_UNIXODBC=1
    cmake --build build --config Release
   ```
5. Navigate to the `testframework` directory and run the command: `./gradlew --no-parallel --no-daemon test-failover --info`.
6. Log files are written to the `build` directory as `myodbc.log`.

## Performance Test Metrics

### Failover Performance with EFM disabled

| NetworkOutageDelayMillis | FailoverTimeoutMillis | ConnectTimeoutSecs | NetworkTimeoutSecs | MinFailoverTimeMillis | MaxFailoverTimeMillis | AvgFailoverTimeMillis |
|--------------------------|-----------------------|--------------------|--------------------|-----------------------|-----------------------|-----------------------|
| 10000                    | 30000                 | 30                 | 30                 | 20187                 | 20243                 | 20209                 |
| 15000                    | 30000                 | 30                 | 30                 | 15197                 | 15229                 | 15211                 |
| 20000                    | 30000                 | 30                 | 30                 | 10185                 | 10238                 | 10211                 |
| 25000                    | 30000                 | 30                 | 30                 | 5167                  | 5230                  | 5206                  |
| 30000                    | 30000                 | 30                 | 30                 | 179                   | 257                   | 211                   |

![](../images/failover_performance.png)

### Enhanced Failure Monitoring Performance with failover disabled

| NetworkOutageDelayMillis | DetectionTimeMillis   | DetectionIntervalMillis | DetectionCount | MinFailureDectionTimeMillis | MaxFailureDetectionTimeMillis | AvgFailureDetectionTimeMillis |
|--------------------------|-----------------------|-------------------------|----------------|-----------------------------|-------------------------------|-------------------------------|
| 10000                    | 30000                 | 5000                    | 3              | 35078                       | 35084                         | 35080                         |
| 15000                    | 30000                 | 5000                    | 3              | 30067                       | 30074                         | 30071                         |
| 20000                    | 30000                 | 5000                    | 3              | 25066                       | 25074                         | 25070                         |
| 25000                    | 30000                 | 5000                    | 3              | 20057                       | 20068                         | 20063                         |
| 30000                    | 30000                 | 5000                    | 3              | 20061                       | 20072                         | 20067                         |
| 35000                    | 30000                 | 5000                    | 3              | 20060                       | 20071                         | 20066                         |
| 40000                    | 30000                 | 5000                    | 3              | 20062                       | 20072                         | 20066                         |
| 45000                    | 30000                 | 5000                    | 3              | 20066                       | 20072                         | 20069                         |
| 50000                    | 30000                 | 5000                    | 3              | 20063                       | 20075                         | 20070                         |

![](../images/efm_detection_performance.png)

### Failover Performance with Enhanced Failure Monitoring enabled

| NetworkOutageDelayMillis | DetectionTimeMillis   | DetectionIntervalMillis | DetectionCount | MinFailoverTimeMillis | MaxFailoverTimeMillis | AvgFailoverTimeMillis |
|--------------------------|-----------------------|-------------------------|----------------|-----------------------|-----------------------|-----------------------|
| 10000                    | 30000                 | 5000                    | 3              | 30310                 | 35089                 | 31121                 |
| 15000                    | 30000                 | 5000                    | 3              | 25309                 | 30095                 | 26133                 |
| 20000                    | 30000                 | 5000                    | 3              | 20331                 | 25090                 | 21135                 |
| 25000                    | 30000                 | 5000                    | 3              | 15316                 | 20073                 | 16916                 |
| 30000                    | 30000                 | 5000                    | 3              | 15325                 | 20051                 | 16897                 |
| 35000                    | 30000                 | 5000                    | 3              | 15311                 | 20174                 | 16927                 |
| 40000                    | 30000                 | 5000                    | 3              | 15314                 | 20087                 | 16127                 |
| 45000                    | 30000                 | 5000                    | 3              | 15309                 | 20070                 | 16123                 |
| 50000                    | 30000                 | 5000                    | 3              | 15309                 | 20007                 | 16100                 |

![](../images/efm_failover_performance.png)
