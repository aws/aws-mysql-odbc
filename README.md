# Amazon Web Services (AWS) ODBC Driver for MySQL

**The Amazon Web Services (AWS) ODBC Driver for MySQL** allows an application to take advantage of the features of clustered MySQL databases. It is based on and can be used as a drop-in compatible for the [MySQL Connector/ODBC driver](https://github.com/mysql/mysql-connector-odbc/), and is compatible with all MySQL deployments.

## Table of Contents
- [Amazon Web Services (AWS) ODBC Driver for MySQL](#amazon-web-services-aws-odbc-driver-for-mysql)
  - [Table of Contents](#table-of-contents)
  - [What is Failover?](#what-is-failover)
  - [Benefits of the AWS ODBC Driver for MySQL](#benefits-of-the-aws-odbc-driver-for-mysql)
  - [Getting Started](#getting-started)
  - [Building the AWS ODBC Driver for MySQL](#building-the-aws-odbc-driver-for-mysql)
    - [Windows](#windows-1)
    - [MacOS](#macos-1)
      - [Troubleshoot](#troubleshoot)
    - [Linux](#linux-1)
  - [Testing the AWS ODBC Driver for MySQL](#testing-the-aws-odbc-driver-for-mysql)
    - [Unit Tests](#unit-tests)
    - [Integration Tests](#integration-tests)
      - [Integration Tests Against A MySQL Server](#integration-tests-against-a-mysql-server)
        - [Prerequisites](#prerequisites)
        - [Steps](#steps)
      - [Failover-specific Integration Tests](#failover-specific-integration-tests)
        - [Prerequisites](#prerequisites-1)
        - [Steps](#steps-1)
  - [Getting Help and Opening Issues](#getting-help-and-opening-issues)
    - [Logging](#logging)
      - [Enabling Logs On Windows](#enabling-logs-on-windows)
        - [Example](#example)
      - [Enabling Logs On MacOS and Linux](#enabling-logs-on-macos-and-linux)
  - [Documentation](#documentation)
  - [License](#license)

## What is Failover?
An Amazon Aurora database (DB) cluster uses failover to automatically repair the DB cluster status when a primary DB instance becomes unavailable. During failover, Aurora promotes a replica to become the new primary DB instance, so that the DB cluster can provide maximum availability to a primary read-write DB instance. The AWS ODBC Driver for MySQL is designed to coordinate with this behaviour in order to provide minimal downtime in the event of a DB instance failure.

## Benefits of the AWS ODBC Driver for MySQL
Although Aurora is able to provide maximum availability through the use of failover, existing client drivers do not fully support this functionality. This is partially due to the time required for the DNS of the new primary DB instance to be fully resolved in order to properly direct the connection. The AWS ODBC Driver for MySQL fully utilizes failover behaviour by maintaining a cache of the Aurora cluster topology and each DB instance's role (Aurora Replica or primary DB instance). This topology is provided via a direct query to the Aurora database, essentially providing a shortcut to bypass the delays caused by DNS resolution. With this knowledge, the AWS ODBC Driver can more closely monitor the Aurora DB cluster status so that a connection to the new primary DB instance can be established as fast as possible. Additionally, as noted above, the AWS ODBC Driver is designed to be drop-in compatible for other MySQL ODBC drivers and can be used to interact with Aurora MySQL, RDS MySQL, and commercial/open-source MySQL databases.

## Getting Started
For more information on how to install and configure the AWS ODBC Driver for MySQL, please visit the [getting started page](./docs/GettingStarted.md).

## Building the AWS ODBC Driver for MySQL

### Windows
<!-- TODO: Verify that the driver can be built with newer versions of Visual Studio after rebasing -->
1. Install the following programs to build the driver:
    - [CMake](https://cmake.org/download/)
    - [Visual Studio](https://visualstudio.microsoft.com/downloads/)
      > The driver has been built successfully using `Visual Studio 2019`, and it may not build correctly with other versions. When installing Visual Studio, ensure the `Visual C++ 2019` and `Visual Studio Tools for CMake` packages are also installed.
    - [MySQL Server](https://dev.mysql.com/downloads/installer/)
2. Build the driver in the `build` directory with the following commands:
    ```
    cmake -S . -B build -G "Visual Studio 16 2019" -DMYSQL_DIR="C:\Program Files\MySQL\MySQL Server 8.0" -DMYSQLCLIENT_STATIC_LINKING=TRUE
    cmake --build build --config Release
    ```

### MacOS
1. Install the following packages available on `Homebrew` or other package management system of your choice:
     - `libiodbc`
     - `cmake`
     - `mysql-client`
     - `mysql`
2. Set the environment variable MYSQL_DIR as the path to your `mysql-client` installation location:
    ```
    export MYSQL_DIR=/usr/local/opt/mysql-client
    ```
3. Build the driver in the `build` directory with the following commands:
    ```
    cmake -S . -B build -G "Unix Makefiles" -DMYSQLCLIENT_STATIC_LINKING=true -DODBC_INCLUDES=/usr/local/Cellar/libiodbc/3.52.15/include
    cmake --build build --config Release
    ```
    Note: you may have a different `libiodbc` version. Change `3.52.15` to your respective version.

#### Troubleshoot
If you encounter an `ld: library not found for -lzstd` error, run the following command, and then rebuild the driver:
```
export LIBRARY_PATH=$LIBRARY_PATH:$(brew --prefix zstd)/lib/
```

If you encounter an `ld: library not found for -lssl` error, run one of the following commands (depending on what openssl library you have) and then rebuild the driver:
```
export LIBRARY_PATH=$LIBRARY_PATH:/usr/local/opt/openssl/lib/
```
or
```
export LIBRARY_PATH=$LIBRARY_PATH:/usr/local/opt/openssl@1.1/lib/
```

### Linux
1. Install the following required packages:
    ```
    sudo apt-get update
    sudo apt-get install build-essential libgtk-3-dev libmysqlclient-dev unixodbc unixodbc-dev
    ```
2. Build the driver in the `build` directory with the following commands:
    ```
    cmake -S . -B build -G "Unix Makefiles" -DMYSQLCLIENT_STATIC_LINKING=true -DWITH_UNIXODBC=1
    cmake --build build --config Release
    ```

## Testing the AWS ODBC Driver for MySQL
### Unit Tests
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
    - Run `ctest` directly from the `testing` directory.
    - Navigate to `testing/bin/Release` and run `testing.exe`. To specify a particular test or test suite, include `--gtest_filter` in the command.

The following example demonstrates running all the tests in the `TopologyServiceTest` suite with the `.\testing.exe --gtest_filter=TopologyServiceTest.*` command:

```
PS C:\Other\dev\aws-mysql-odbc\testing\bin\Release> .\testing.exe --gtest_filter=TopologyServiceTest.*
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

### Integration Tests
There are two types of integration tests you can run. One type is an integration test against a MySQL Server, and the other type consists of the two sets of integration tests specific to the failover functionality provided by the AWS ODBC Driver for MySQL.

#### Integration Tests Against A MySQL Server

##### Prerequisites
- Install MySQL Server. See the [build instructions for the desired system](#building-the-aws-odbc-driver-for-mysql) for instructions.
- [**Optional**] Install [Docker](https://docs.docker.com/get-docker/).

##### Steps
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
2. Build and install the driver for a specific platform as described in [Installing the AWS ODBC Driver for MySQL](#installing-the-aws-odbc-driver-for-mysql).
3. Start the MySQL Server. You may either start a local server or use a docker images. 
4. [**Optional**] To start the MySQL Server via a Docker image. Navigate to `test/docker` and execute `docker-compose up -d` to start the server in the background.
5. Navigate to the `test` directory and execute `ctest`.

#### Failover-specific Integration Tests
> **NOTE:** This set of tests can only be run on Linux at the moment.

##### Prerequisites
- Install JDK 8:
  ```
  sudo apt-get install openjdk-8-jdk
  ```
- Install [Docker](https://docs.docker.com/get-docker/)
  
##### Steps
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
6. Log files are written to the `build` directory as `myodbc.sql`.

## Getting Help and Opening Issues
If you encounter a bug with the AWS ODBC Driver for MySQL, we would like to hear about it. Please search the [existing issues](https://github.com/awslabs/aws-mysql-odbc/issues) and see if others are also experiencing the issue before opening a new issue. When opening a new issue, please provide: 

- the version of AWS ODBC Driver for MySQL
- C++ language version
- the OS platform and version
- the MySQL database version you're running against

Please include a reproduction case for the issue when appropriate.

The GitHub issues are intended for bug reports and feature requests. Keeping the list of open issues lean will help us respond in a timely manner.

### Logging
If you encounter an issue with the AWS ODBC Driver for MySQL and would like to report it, please include driver logs if possible, as they help us diagnose problems quicker.

#### Enabling Logs On Windows
When connecting the AWS ODBC Driver for MySQL using a Windows system, ensure logging is enabled by following the steps below:

1. Open the ODBC Data Source Administrator.
2. Add a new DSN or configure an existing DSN of your choice.
3. Open the details for the DSN.
4. Navigate to the Debug tab.
5. Ensure the box to log queries is checked.

##### Example
![enable-logging-windows](docs/enable-logging-windows.jpg)

The resulting log file, named `myodbc.sql`, can be found under `%temp%`.

#### Enabling Logs On MacOS and Linux
When connecting the AWS ODBC Driver for MySQL using a MacOS or Linux system, include the `LOG_QUERY` parameter in the connection string to enable logging (`DSN=XXX;LOG_QUERY=1;...`). The log file, named `myodbc.sql`, can be found in the current working directory.

## Documentation
For additional documentation about the AWS ODBC Driver for MySQL, [please refer to the documentation for the open-source mysql-connector-odbc driver that the AWS ODBC Driver for MySQL is based on](https://dev.mysql.com/doc/connector-cpp/8.0/en/).

## License
This software is released under version 2 of the GNU General Public License (GPLv2).
