# Testing the AWS ODBC Driver for MySQL
## Unit Tests
1. Build driver binaries with `ENABLE_UNIT_TESTS` command set to `TRUE`:
   - **Windows**
        ```
        cmake -S . -B build -G "Visual Studio 16 2019" -DMYSQL_DIR="C:\Program Files\MySQL\MySQL Server 8.0" -DMYSQLCLIENT_STATIC_LINKING=TRUE -DENABLE_UNIT_TESTS=TRUE
        cmake --build build --config Release
        ```
   - **MacOS**
        ```
        cmake -S . -B build -G "Unix Makefiles" -DMYSQLCLIENT_STATIC_LINKING=true -DODBC_INCLUDES=/usr/local/Cellar/libiodbc/3.52.15/include -DENABLE_UNIT_TESTS=TRUE
        cmake --build build --config Release
        ```
   - **Linux**
        ```
        cmake -S . -B build -G "Unix Makefiles" -DMYSQLCLIENT_STATIC_LINKING=true -DWITH_UNIXODBC=1 -DENABLE_UNIT_TESTS=TRUE
        cmake --build build --config Release
        ```
2. There are two options to run the unit tests:
    - Run `ctest` directly from the `unit_testing` directory.
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
