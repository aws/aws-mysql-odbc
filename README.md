# Amazon Web Services (AWS) ODBC Driver for MySQL

**The Amazon Web Services (AWS) ODBC Driver for MySQL** allows an application to take advantage of the features of clustered MySQL databases. It is based on and can be used as a drop-in compatible for the [MySQL Connector/ODBC driver](https://github.com/mysql/mysql-connector-odbc/), and is compatible with all MySQL deployments.

## Table of Contents
- [Amazon Web Services (AWS) ODBC Driver for MySQL](#amazon-web-services-aws-odbc-driver-for-mysql)
  - [Table of Contents](#table-of-contents)
  - [What is Failover?](#what-is-failover)
  - [Benefits of the AWS ODBC Driver for MySQL](#benefits-of-the-aws-odbc-driver-for-mysql)
  - [Getting Started](#getting-started)
    - [Installing the AWS ODBC Driver for MySQL](#installing-the-aws-odbc-driver-for-mysql)
      - [Windows](#windows)
      - [MacOS](#macos)
      - [Linux](#linux)
      - [Configuring the Driver and DSN Entries](#configuring-the-driver-and-dsn-entries)
        - [odbc.ini](#odbcini)
        - [odbcinst.ini](#odbcinstini)
  - [Using the AWS ODBC Driver for MySQL](#using-the-aws-odbc-driver-for-mysql)
    - [Connection Strings and Configuring the Driver](#connection-strings-and-configuring-the-driver)
    - [Failover Specific Options](#failover-specific-options)
      - [Driver Behaviour During Failover For Different Connection URLs](#driver-behaviour-during-failover-for-different-connection-urls)
      - [Host Pattern](#host-pattern)
    - [Failover Exception Codes](#failover-exception-codes)
      - [08S01 - Communication Link Failure](#08s01---communication-link-failure)
      - [08S02 - Communication Link Changed](#08s02---communication-link-changed)
        - [Sample Code](#sample-code)
      - [08007 - Connection Failure During Transaction](#08007---connection-failure-during-transaction)
        - [Sample Code](#sample-code-1)
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
Although Aurora is able to provide maximum availability through the use of failover, existing client drivers do not fully support this functionality. This is partially due to the time required for the DNS of the new primary DB instance to be fully resolved in order to properly direct the connection. The AWS ODBC Driver for MySQL fully utilizes failover behaviour by maintaining a cache of the Aurora cluster topology and each DB instance's role (Aurora Replica or primary DB instance). This topology is provided via a direct query to the Aurora database, essentially providing a shortcut to bypass the delays caused by DNS resolution. With this knowledge, the AWS ODBC Driver can more closely monitor the Aurora DB cluster status so that a connection to the new primary DB instance can be established as fast as possible. Additionally, as noted above, the AWS ODBC Driver is designed to be a drop-in compatible for other MySQL ODBC drivers and can be used to interact with RDS and MySQL databases as well as Aurora MySQL.

## Getting Started

### Installing the AWS ODBC Driver for MySQL

#### Windows
Download the `.msi` Windows installer for the given system, then run the installer and follow the instructions. The default target installation location for the driver files is `C:\Program Files\AWS ODBC Driver for MySQL`. An ANSI and a Unicode driver will be installed, named `AWS ODBC ANSI Driver for MySQL` and `AWS ODBC Unicode Driver for MySQL`. To uninstall the driver, open the same installer file and follow the installer instructions to remove the driver.

#### MacOS
Download the `.pkg` installer, the run the installer and follow the instructions. The default target installation location for the driver folder is `/usr/local/`. Note that for a MacOS system, additional steps are required to configure the driver and Data Source Name (DSN) entries before the driver(s) can be used. This is done by the installer, as it will register two driver entries with two corresponding DSN entries. See [how to configure the driver and DSN settings](#configuring-the-driver-and-dsn-entries) for a configuration sample. There is no uninstaller at this time, but all the driver files can be removed by deleting the target installation directory.

#### Linux
Download the `.tar.gz` file, then extract the contents to your desired location. For a Linux system, additional steps are required to configure the driver and Data Source Name (DSN) entries before the driver(s) can be used. See [how to configure the driver and DSN settings](#configuring-the-driver-and-dsn-entries). There is no uninstaller at this time, but all the driver files can be removed by deleting the target installation directory.

#### Configuring the Driver and DSN Entries 
To use the driver on MacOS or Linux systems, the two files `odbc.ini` and `odbcinst.ini` must exist and contain the correct driver and data source name (DSN) configurations. This can be done through changing the files manually, or through tools with a GUI such as the iODBC Administrator (available for MacOS). The sample odbc.ini and odbcinst.ini files show how an ANSI driver could be set up for a MacOS system. For a Linux system, the files would be similar, but the driver file would have a `.so` extension instead of the `.dylib` extension as seen in the sample. On a MacOS system, the `odbc.ini` and `odbcinst.ini` files are typically located in the `/Library/ODBC/` directory. On a Linux system, the `odbc.ini` and `odbcinst.ini` files are typically located in the `/etc` directory.

##### odbc.ini
```bash
[ODBC Data Sources]
awsmysqlodbcw = AWS ODBC Unicode Driver for MySQL
awsmysqlodbca = AWS ODBC ANSI Driver for MySQL

[awsmysqlodbcw]
Driver          = AWS ODBC Unicode Driver for MySQL
SERVER          = localhost
NO_SCHEMA       = 1
TOPOLOGY_RR     = 30000
FAILOVER_T      = 60000
FAILOVER_TRR    = 5000
FAILOVER_WRI    = 5000
FAILOVER_RCT    = 30000
CONNECT_TIMEOUT = 30
NETWORK_TIMEOUT = 30

[awsmysqlodbca]
Driver          = AWS ODBC ANSI Driver for MySQL
SERVER          = localhost
NO_SCHEMA       = 1
TOPOLOGY_RR     = 30000
FAILOVER_T      = 60000
FAILOVER_TRR    = 5000
FAILOVER_WRI    = 5000
FAILOVER_RCT    = 30000
CONNECT_TIMEOUT = 30
NETWORK_TIMEOUT = 30
```

##### odbcinst.ini
```bash
[ODBC Drivers]
AWS ODBC Unicode Driver for MySQL = Installed
AWS ODBC ANSI Driver for MySQL    = Installed

[AWS ODBC Unicode Driver for MySQL]
Driver = /usr/local/aws-mysql-odbc-0.1.0-macos/lib/awsmysqlodbcw.dylib

[AWS ODBC ANSI Driver for MySQL]
Driver = /usr/local/aws-mysql-odbc-0.1.0-macos/lib/awsmysqlodbca.dylib
```

## Using the AWS ODBC Driver for MySQL
The AWS ODBC Driver for MySQL is drop-in compatible, so usage is identical to the [MySQL Connector/ODBC driver](https://github.com/mysql/mysql-connector-odbc/). The sections below highlight driver usage specific to failover.

### Connection Strings and Configuring the Driver
To set up a connection, the driver uses an ODBC connection string. ODBC connection strings are semicolon-delimited, and they specify a set of connection options. Typically, a connection string will either:

1. specify a Data Source Name containing a preconfigured set of options (DSN=xxx;) or
2. configure options explicitly (SERVER=xxx;PORT=xxx;...). This option will override values set inside the DSN.

### Failover Specific Options
In addition to the parameters that you can configure for the [MySQL Connector/ODBC driver](https://dev.mysql.com/doc/connector-odbc/en/connector-odbc-configuration-connection-parameters.html), you can pass the following parameters to the AWS ODBC Driver for MySQL through the connection string to specify additional driver behaviour. If the values for these options are not specified, the default values will be used.

| Option                             | Description                                                                                                                                                                                                                                                                                                                                                                        | Type   | Required | Default                                  |
| ---------------------------------- |------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|--------|----------|------------------------------------------|
| `DISABLE_CLUSTER_FAILOVER`         | Set to `1` to disable the fast failover behaviour offered by the AWS ODBC Driver for MySQL.                                                                                                                                                                                                                                                                                        | bool   | No       | `0`                                      |
| `ALLOW_READER_CONNECTIONS`         | Set to `1` to allow connections to reader instances during the failover process.                                                                                                                                                                                                                                                                                                   | bool   | No       | `0`                                      |
| `GATHER_PERF_METRICS`              | Set to `1` to record failover-associated metrics.                                                                                                                                                                                                                                                                                                                                  | bool   | No       | `0`                                      |
| `GATHER_PERF_METRICS_PER_INSTANCE` | Set to `1` to gather additional performance metrics per instance as well as cluster.                                                                                                                                                                                                                                                                                               | bool   | No       | `0`                                      |
| `HOST_PATTERN`                     | This parameter is not required unless connecting to an AWS RDS cluster via an IP address or custom domain URL. In those cases, this parameter specifies the cluster instance DNS pattern that will be used to build a complete instance endpoint. A "?" character in this pattern should be used as a placeholder for the DB instance identifiers of the instances in the cluster.  <br/><br/>Example: `?.my-domain.com`, `any-subdomain.?.my-domain.com:9999`<br/><br/>Usecase Example: If your cluster instance endpoints followed this pattern:`instanceIdentifier1.customHost`, `instanceIdentifier2.customHost`, etc. and you wanted your initial connection to be to `customHost:1234`, then your connection string should look something like this: `SERVER=customHost;PORT=1234;DATABASE=test;HOST_PATTERN=?.customHost` <br><br/> If the provided connection string is not an IP address or custom domain, the driver will automatically acquire the cluster instance host pattern from the customer-provided connection string. | char\* | If connecting using an IP address or custom domain URL: Yes <br><br> Otherwise: No <br><br> See [host pattern](#host-pattern) for more details. | `NONE`                                   |
| `CLUSTER_ID`                       | A unique identifier for the cluster. Connections with the same cluster ID share a cluster topology cache. This connection parameter is not required and thus should only be set if desired.                                                                                                                                                                                        | char\* | No       | Either the cluster ID or the instance ID, depending on whether the provided connection string is a cluster or instance URL. |
| `TOPOLOGY_RR`                      | Cluster topology refresh rate in milliseconds. The cached topology for the cluster will be invalidated after the specified time, after which it will be updated during the next interaction with the connection.                                                                                                                                                                   | int    | No       | `30000`                                  |
| `FAILOVER_T`                       | Maximum allowed time in milliseconds to attempt reconnecting to a new writer or reader instance after a cluster failover is initiated.                                                                                                                                                                                                                                             | int    | No       | `60000`                                  |
| `FAILOVER_TRR`                     | Cluster topology refresh rate in milliseconds during a writer failover process. During the writer failover process, cluster topology may be refreshed at a faster pace than normal to speed up discovery of the newly promoted writer.                                                                                                                                             | int    | No       | `5000`                                   |
| `FAILOVER_WRI`                     | Interval of time in milliseconds to wait between attempts to reconnect to a failed writer during a writer failover process.                                                                                                                                                                                                                                                        | int    | No       | `5000`                                   |
| `FAILOVER_RCT`                     | Maximum allowed time in milliseconds to attempt a connection to a reader instance during a reader failover process.                                                                                                                                                                                                                                                                | int    | No       | `30000`                                  |
| `CONNECT_TIMEOUT`                  | Timeout (in seconds) for socket connect, with 0 being no timeout.                                                                                                                                                                                                                                                                                                                  | int    | No       | `30`                                     |
| `NETWORK_TIMEOUT`                  | Timeout (in seconds) on network socket operations, with 0 being no timeout.                                                                                                                                                                                                                                                                                                        | int    | No       | `30`                                     |

#### Driver Behaviour During Failover For Different Connection URLs
![failover_behaviour](doc/failover_behaviour.jpg)

#### Host Pattern
When connecting to Aurora clusters, this parameter is required when the connection string does not provide enough information about the database cluster domain name. If the Aurora cluster endpoint is used directly, the driver will recognize the standard Aurora domain name and can re-build a proper Aurora instance name when needed. In cases where the connection string uses an IP address, a custom domain name or localhost, the driver won't know how to build a proper domain name for a database instance endpoint. For example, if a custom domain was being used and the cluster instance endpoints followed a pattern of `instanceIdentifier1.customHost`, `instanceIdentifier2.customHost`, etc, the driver would need to know how to construct the instance endpoints using the specified custom domain. Because there isn't enough information from the custom domain alone to create the instance endpoints, the `HOST_PATTERN` should be set to `?.customHost`, making the connection string `SERVER=customHost;PORT=1234;DATABASE=test;HOST_PATTERN=?.customHost`. Refer to [Driver Behaviour During Failover For Different Connection URLs](#driver-behaviour-during-failover-for-different-connection-urls) for more examples.

### Failover Exception Codes

#### 08S01 - Communication Link Failure
When the driver returns an error code ```08S01```, the original connection failed, and the driver tried to failover to a new instance, but was not able to. There are various reasons this may happen: no nodes were available, a network failure occurred, and so on. In this scenario, please wait until the server is up or other problems are solved.

#### 08S02 - Communication Link Changed
When the driver returns an error code ```08S02```, the original connection failed while autocommit was set to true, and the driver successfully failed over to another available instance in the cluster. However, any session state configuration of the initial connection is now lost. In this scenario, you should:

1. Reconfigure and reuse the original connection (the reconfigured session state will be the same as the original connection).
2. Repeat the query that was executed when the connection failed and continue work as desired.

##### Sample Code
```cpp
#include <iostream>
#include <sql.h>
#include <sqlext.h>

#define MAX_NAME_LEN 255
#define SQL_MAX_MESSAGE_LENGTH 512

// Scenario 1: Failover happens when autocommit is set to true - SQLRETURN with code 08S02.

void setInitialSessionState(SQLHDBC dbc, SQLHSTMT handle) {
    const auto set_timezone = (SQLCHAR*)"SET time_zone = \"+00:00\"";
    SQLExecDirect(handle, set_timezone, SQL_NTS);
}

bool executeQuery(SQLHDBC dbc, SQLCHAR* query) {
    int MAX_RETRIES = 5;
    SQLHSTMT handle;
    SQLCHAR sqlstate[6], message[SQL_MAX_MESSAGE_LENGTH];
    SQLRETURN ret;

    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &handle);

    int retries = 0;
    bool success = false;

    while (true) {
        ret = SQLExecDirect(handle, query, SQL_NTS);

        if (SQL_SUCCEEDED(ret)) {
            success = true;
            break;
        }
        else {
            if (retries > MAX_RETRIES) {
                break;
            }

            // Check what kind of error has occurred
            SQLSMALLINT stmt_length;
            SQLINTEGER native_error;
            SQLError(nullptr, nullptr, handle, sqlstate, &native_error, message, SQL_MAX_MESSAGE_LENGTH - 1, &stmt_length);
            const std::string state = (char*)sqlstate;

            // Failover has occurred and the driver has failed over to another instance successfully
            if (state.compare("08S02") == 0) {
                // Reconfigure the connection
                setInitialSessionState(dbc, handle);
                // Re-execute that query again
                retries++;
            }
            else {
                // If other exceptions occur
                break;
            }
        }
    }

    SQLFreeHandle(SQL_HANDLE_STMT, handle);
    return success;
}

int main() {
    SQLHENV env;
    SQLHDBC dbc;
    SQLSMALLINT len;
    SQLRETURN ret;
    SQLCHAR conn_in[4096], conn_out[4096];

    SQLAllocHandle(SQL_HANDLE_ENV, nullptr, &env);
    SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0);
    SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc);

    const char* dsn = "AWSODBCDriverDSN";
    const char* user = "username";
    const char* pwd = "password";
    const char* server = "db-identifier-cluster-XYZ.us-east-2.rds.amazonaws.com";
    int port = 3306;
    const char* db = "employees";

    sprintf(reinterpret_cast<char*>(conn_in), "DSN=%s;UID=%s;PWD=%s;SERVER=%s;PORT=%d;DATABASE=%s;", dsn, user, pwd, server, port, db);
    
    // attempt a connection
    ret = SQLDriverConnect(dbc, nullptr, conn_in, SQL_NTS, conn_out, MAX_NAME_LEN, &len, SQL_DRIVER_NOPROMPT);
    
    if (SQL_SUCCEEDED(ret)) {
        // if the connection is successful, execute a query using the AWS ODBC Driver for MySQL
        const auto sleep_stmt = (SQLCHAR*)"SELECT * from employees*";
        executeQuery(dbc, sleep_stmt);
    }

    SQLDisconnect(dbc);
    SQLFreeHandle(SQL_HANDLE_DBC, dbc);
    SQLFreeHandle(SQL_HANDLE_ENV, env);

    return 0;
}
```

#### 08007 - Connection Failure During Transaction
When the driver returns an error code ```08007```, the original connection failed within a transaction (while autocommit was set to false). In this scenario, when the transaction ends the driver first attempts to rollback the transaction, and then fails over to another available instance in the cluster. Note that the rollback might be unsuccessful as the initial connection may be broken at the time that the driver recognizes the problem. Note also that any session state configuration of the initial connection is now lost. In this scenario, you should:

1. Reconfigure and reuse the original connection (the reconfigured session state will be the same as the original connection).
2. Re-start the transaction and repeat all queries which were executed during the transaction before the connection failed.
3. Repeat the query that was executed when the connection failed and continue work as desired.

##### Sample Code
```cpp
#include <iostream>
#include <sql.h>
#include <sqlext.h>

#define MAX_NAME_LEN 255
#define SQL_MAX_MESSAGE_LENGTH 512

// Scenario 2: Failover happens when autocommit is set to false - SQLRETURN with code 08007.

void setInitialSessionState(SQLHDBC dbc, SQLHSTMT handle) {
    const auto set_timezone = (SQLCHAR*)"SET time_zone = \"+00:00\"";
    const auto setup_autocommit_query = (SQLCHAR*)"SET autocommit = 0";

    SQLExecDirect(handle, set_timezone, SQL_NTS);
    SQLExecDirect(handle, setup_autocommit_query, SQL_NTS);
}

bool executeQuery(SQLHDBC dbc) {
    int MAX_RETRIES = 5;
    SQLHSTMT handle;
    SQLCHAR sqlstate[6], message[SQL_MAX_MESSAGE_LENGTH];
    SQLRETURN ret;

    // Queries to execute in a transaction
    SQLCHAR* queries[] = {
        (SQLCHAR*)"INSERT INTO employees(emp_no, birth_date, first_name, last_name, gender, hire_date) VALUES (5000000, '1958-05-01', 'John', 'Doe', 'M', '1997-11-30')",
        (SQLCHAR*)"INSERT INTO employees(emp_no, birth_date, first_name, last_name, gender, hire_date) VALUES (5000001, '1958-05-01', 'Mary', 'Malcolm', 'F', '1997-11-30')",
        (SQLCHAR*)"INSERT INTO employees(emp_no, birth_date, first_name, last_name, gender, hire_date) VALUES (5000002, '1958-05-01', 'Tom', 'Jerry', 'M', '1997-11-30')"
    };

    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &handle);
    setInitialSessionState(dbc, handle);

    int retries = 0;
    bool success = false;

    while (true) {
        for (SQLCHAR* query : queries) {
            ret = SQLExecDirect(handle, query, SQL_NTS);
        }
        SQLEndTran(SQL_HANDLE_DBC, dbc, SQL_COMMIT);

        if (SQL_SUCCEEDED(ret)) {
            success = true;
            break;
        } else {
            if (retries > MAX_RETRIES) {
                break;
            }

            // Check what kind of error has occurred
            SQLSMALLINT stmt_length;
            SQLINTEGER native_error;
            SQLError(nullptr, nullptr, handle, sqlstate, &native_error, message, SQL_MAX_MESSAGE_LENGTH - 1, &stmt_length);
            const std::string state = (char*)sqlstate;

            // Failover has occurred and the driver has failed over to another instance successfully
            if (state.compare("08007") == 0) {
                // Reconfigure the connection
                setInitialSessionState(dbc, handle);
                // Re-execute every queriy that was inside the transaction
                retries++;
            }
            else {
                // If other exceptions occur
                break;
            }
        }
    }

    return success;
}

int main() {
    SQLHENV env;
    SQLHDBC dbc;
    SQLSMALLINT len;
    SQLRETURN rc;
    SQLCHAR conn_in[4096], conn_out[4096];

    SQLAllocHandle(SQL_HANDLE_ENV, nullptr, &env);
    SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0);
    SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc);

    const char* dsn = "AWSODBCDriverDSN";
    const char* user = "username";
    const char* pwd = "password";
    const char* server = "db-identifier-cluster-XYZ.us-east-2.rds.amazonaws.com";
    int port = 3306;
    const char* db = "employees";

    sprintf(reinterpret_cast<char*>(conn_in), "DSN=%s;UID=%s;PWD=%s;SERVER=%s;PORT=%d;DATABASE=%s;", dsn, user, pwd, server, port, db);
    
    // attempt a connection
    rc = SQLDriverConnect(dbc, nullptr, conn_in, SQL_NTS, conn_out, MAX_NAME_LEN, &len, SQL_DRIVER_NOPROMPT);
    
    if (SQL_SUCCEEDED(rc)) {
        // if the connection is successful, execute a query using the AWS ODBC Driver for MySQL
        executeQuery(dbc);
    }

    SQLDisconnect(dbc);
    SQLFreeHandle(SQL_HANDLE_DBC, dbc);
    SQLFreeHandle(SQL_HANDLE_ENV, env);

    return 0;
}
```

>### :warning: Warnings About Proper Usage of the AWS ODBC Driver for MySQL
>It is highly recommended that you use the cluster and read-only cluster endpoints instead of the direct instance endpoints of your Aurora cluster, unless you are confident about your application's usage of instance endpoints. Although the driver will correctly failover to the new writer instance when using instance endpoints, use of these endpoints is discouraged because individual instances can spontaneously change reader/writer status when failover occurs. The driver will always connect directly to the instance specified if an instance endpoint is provided, so a write-safe connection cannot be assumed if the application uses instance endpoints.

## Building the AWS ODBC Driver for MySQL

### Windows
<!-- TODO: Verify that the driver can be built with newer versions of Visual Studio after rebasing -->
1. Install the following programs to build the driver:
    - [CMake](https://cmake.org/download/)
    - [Visual Studio](https://visualstudio.microsoft.com/downloads/)
      > The driver has been built successfully using `Visual Studio 2019`, and it may not build correctly with other versions. When installing Visual Studio, ensure the `Visual C++ 2019` and `Visual Studio Tools for CMake` packages are also installed.
    - [MySQL Server](https://dev.mysql.com/downloads/installer/)
2. Build the driver to the `build` directory with the following commands:
    ```
    cmake -S . -B build -G "Visual Studio 16 2019" -DMYSQL_DIR="C:\Program Files\MySQL\MySQL Server 8.0" -DMYSQLCLIENT_STATIC_LINKING=TRUE
    cmake --build build --config Release
    ```

### MacOS
1. Install the following packages available on `Homebrew` or another package management system of your choice:
     - `libiodbc`
     - `cmake`
     - `mysql-client`
     - `mysql`
2. Set the environment variable MYSQL_DIR as the path to your `mysql-client` installation location
    ```
    export MYSQL_DIR=/usr/local/opt/mysql-client
    ```
3. Build the driver to the `build` directory with the following commands:
    ```
    cmake -S . -B build -G "Unix Makefiles" -DMYSQLCLIENT_STATIC_LINKING=true -DODBC_INCLUDES=/usr/local/Cellar/libiodbc/3.52.15/include
    cmake --build build --config Release
    ```
    Note: you may have a different `libiodbc` version. Change `3.52.15` to your respective version.

#### Troubleshoot
If you see a `ld: library not found for -lzstd` error, run the following and the rebuild the driver:
```
export LIBRARY_PATH=$LIBRARY_PATH:$(brew --prefix zstd)/lib/
```

If you see a `ld: library not found for -lssl` error, run one of the following depending on what openssl library you have and the rebuild the driver:
```
export LIBRARY_PATH=$LIBRARY_PATH:/usr/local/opt/openssl/lib/
```
or
```
export LIBRARY_PATH=$LIBRARY_PATH:/usr/local/opt/openssl@1.1/lib/
```

### Linux
1. Install the following required packages on Linux.
    ```
    sudo apt-get update
    sudo apt-get install build-essential libgtk-3-dev libmysqlclient-dev unixodbc unixodbc-dev
    ```
2. Build the driver to the `build` directory with the following commands:
    ```
    cmake -S . -B build -G "Unix Makefiles" -DMYSQLCLIENT_STATIC_LINKING=true -DWITH_UNIXODBC=1
    cmake --build build --config Release
    ```

## Testing the AWS ODBC Driver for MySQL
### Unit Tests
1. Build driver binaries with `ENABLE_UNIT_TESTS` command set to `TRUE`.
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
2. There are two options to run the unit tests.
    - Run `ctest` directly from the `testing` directory, or;
    - Navigate to `testing/bin/Release` and run `testing.exe`. To specify a particular test or test suite, use `--gtest_filter` in the command.

The following is an example running all the tests in the `TopologyServiceTest` suite with the `.\testing.exe --gtest_filter=TopologyServiceTest.*` command:

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
There are two types of integration tests you can run. One type is the integration tests against a MySQL Server, and the other type consists of the two sets of integration tests specific to the failover functionality provided by the AWS ODBC Driver for MySQL.

#### Integration Tests Against A MySQL Server

##### Prerequisites
- Install MySQL Server. See the [build instructions for the desired system](#building-the-aws-odbc-driver-for-mysql) for instructions.
- [**Optional**] Install [Docker](https://docs.docker.com/get-docker/)

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
- Install JDK 8
  ```
  sudo apt-get install openjdk-8-jdk
  ```
- Install [Docker](https://docs.docker.com/get-docker/)
  
##### Steps
> **NOTE:** Running these tests will automatically create an Amazon Aurora MySQL DB cluster with at least 5 instances and may induce a cost. Ensure the test cluster is cleaned up after testing on the [Amazon RDS Management Console](https://console.aws.amazon.com/rds/home).

1. This set of tests runs against an Amazon Aurora MySQL DB cluster with at least 5 instances. The test will automatically generate the required AWS MySQL DB cluster and instances if proper AWS credentials are set up. Refer to this [documentation](https://docs.aws.amazon.com/AmazonRDS/latest/AuroraUserGuide/CHAP_SettingUp_Aurora.html) to set up a development environment for Amazon Aurora.
2. Specify the following environment variables on Linux:
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
5. Navigate to the `testframework` directory and run `./gradlew --no-parallel --no-daemon test-failover --info`.
6. Logs are outputted to the `build` directory as `myodbc.sql`.

## Getting Help and Opening Issues
If you encounter a bug with the AWS ODBC Driver for MySQL, we would like to hear about it. Please search the [existing issues](https://github.com/awslabs/aws-mysql-odbc/issues) and see if others are also experiencing the issue before opening a new issue. When opening a new issue, we will need the version of AWS ODBC Driver for MySQL, C++ language version, OS you’re using, and the MySQL database version you're running against. Please include a reproduction case for the issue when appropriate.

The GitHub issues are intended for bug reports and feature requests. Keeping the list of open issues lean will help us respond in a timely manner.

### Logging
If you encounter an issue with the AWS ODBC Driver for MySQL and would like to report it, please include driver logs if possible, as they help us diagnose problems quicker.

#### Enabling Logs On Windows
When connecting the AWS ODBC Driver for MySQL using a Windows system, ensure logging is enabled by following the steps below:

1. Open the ODBC Data Source Administrator
2. Add a new DSN or configure an existing DSN of your choice
3. Open the details for the DSN
4. Navigate to the Debug tab
5. Ensure the box to log queries is checked

##### Example
![enable-logging-windows](doc/enable-logging-windows.jpg)

The log file, named `myodbc.sql`, can be found under `%temp%`.

#### Enabling Logs On MacOS and Linux
When connecting the AWS ODBC Driver for MySQL using a MacOS or Linux system, use the `LOG_QUERY` parameter in the connection string to enable logging (`DSN=XXX;LOG_QUERY=1;...`). The log file, named `myodbc.sql`, can be found in the current working directory.

## Documentation
For additional documentation on the AWS ODBC Driver for MySQL, [please refer to the documentation for the open-source mysql-connector-odbc driver that the AWS ODBC Driver for MySQL is based on](https://dev.mysql.com/doc/connector-cpp/8.0/en/).

## License
This software is released under version 2 of the GNU General Public License (GPLv2).