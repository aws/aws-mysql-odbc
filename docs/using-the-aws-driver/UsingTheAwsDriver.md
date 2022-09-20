# Using the AWS ODBC Driver for MySQL
The AWS ODBC Driver for MySQL is drop-in compatible, so its usage is identical to the [MySQL Connector/ODBC driver](https://github.com/mysql/mysql-connector-odbc/). The sections below highlight driver usage specific to failover.

## Connection Strings and Configuring the Driver
To set up a connection, the driver uses an ODBC connection string. An ODBC connection string specifies a set of semicolon-delimited connection options. Typically, a connection string will either:

1. specify a Data Source Name containing a preconfigured set of options (DSN=xxx;) or
2. configure options explicitly (SERVER=xxx;PORT=xxx;...). This option will override values set inside the DSN.

## Failover Specific Options
In addition to the parameters that you can configure for the [MySQL Connector/ODBC driver](https://dev.mysql.com/doc/connector-odbc/en/connector-odbc-configuration-connection-parameters.html), you can configure the following parameters in a DSN or connection string to specify failover behaviour. If the values for these options are not specified, the default values will be used. If you are dealing with the Windows DSN UI, click `Details >>` and navigate to the `Cluster Failover` tab to find the equivalent parameters.

| Option                             | Description                                                                                                                                                                                                                                                                                                                                                                        | Type   | Required | Default                                  |
| ---------------------------------- |------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|--------|----------|------------------------------------------|
| `ENABLE_CLUSTER_FAILOVER`            | Set to `1` to enable the fast failover behaviour offered by the AWS ODBC Driver for MySQL.                                                                                                                                                                                                                                                                                         | bool   | No       | `1`                                      |
| `ALLOW_READER_CONNECTIONS`           | Set to `1` to allow connections to reader instances during the failover process.                                                                                                                                                                                                                                                                                                   | bool   | No       | `0`                                      |
| `GATHER_PERF_METRICS`                | Set to `1` to record failover-associated metrics.                                                                                                                                                                                                                                                                                                                                  | bool   | No       | `0`                                      |
| `GATHER_PERF_METRICS_PER_INSTANCE`   | Set to `1` to gather additional performance metrics per instance as well as cluster.                                                                                                                                                                                                                                                                                               | bool   | No       | `0`                                      |
| `HOST_PATTERN`                       | This parameter is not required unless connecting to an AWS RDS cluster via an IP address or custom domain URL. In those cases, this parameter specifies the cluster instance DNS pattern that will be used to build a complete instance endpoint. A "?" character in this pattern should be used as a placeholder for the DB instance identifiers of the instances in the cluster.  <br/><br/>Example: `?.my-domain.com`, `any-subdomain.?.my-domain.com:9999`<br/><br/>Usecase Example: If your cluster instance endpoint follows this pattern:`instanceIdentifier1.customHost`, `instanceIdentifier2.customHost`, etc. and you want your initial connection to be to `customHost:1234`, then your connection string should look like this: `SERVER=customHost;PORT=1234;DATABASE=test;HOST_PATTERN=?.customHost` <br><br/> If the provided connection string is not an IP address or custom domain, the driver will automatically acquire the cluster instance host pattern from the customer-provided connection string. | char\* | If connecting using an IP address or custom domain URL: Yes <br><br> Otherwise: No <br><br> See [Host Pattern](#host-pattern) for more details. | `NONE`                                   |
| `CLUSTER_ID`                         | A unique identifier for the cluster. Connections with the same cluster ID share a cluster topology cache. This connection parameter is not required and thus should only be set if desired.                                                                                                                                                                                        | char\* | No       | Either the cluster ID or the instance ID, depending on whether the provided connection string is a cluster or instance URL. |
| `TOPOLOGY_REFRESH_RATE`              | Cluster topology refresh rate in milliseconds. The cached topology for the cluster will be invalidated after the specified time, after which it will be updated during the next interaction with the connection.                                                                                                                                                                   | int    | No       | `30000`                                  |
| `FAILOVER_TIMEOUT`                   | Maximum allowed time in milliseconds to attempt reconnecting to a new writer or reader instance after a cluster failover is initiated.                                                                                                                                                                                                                                             | int    | No       | `60000`                                  |
| `FAILOVER_TOPOLOGY_REFRESH_RATE`     | Cluster topology refresh rate in milliseconds during a writer failover process. During the writer failover process, cluster topology may be refreshed at a faster pace than normal to speed up discovery of the newly promoted writer.                                                                                                                                             | int    | No       | `5000`                                   |
| `FAILOVER_WRITER_RECONNECT_INTERVAL` | Interval of time in milliseconds to wait between attempts to reconnect to a failed writer during a writer failover process.                                                                                                                                                                                                                                                        | int    | No       | `5000`                                   |
| `FAILOVER_READER_CONNECT_TIMEOUT`    | Maximum allowed time in milliseconds to attempt a connection to a reader instance during a reader failover process.                                                                                                                                                                                                                                                                | int    | No       | `30000`                                  |
| `CONNECT_TIMEOUT`                    | Timeout (in seconds) for socket connect, with 0 being no timeout.                                                                                                                                                                                                                                                                                                                  | int    | No       | `30`                                     |
| `NETWORK_TIMEOUT`                    | Timeout (in seconds) on network socket operations, with 0 being no timeout.                                                                                                                                                                                                                                                                                                        | int    | No       | `30`                                     |

### Driver Behaviour During Failover For Different Connection URLs
![failover_behaviour](./failover_behaviour.jpg)

### Host Pattern
When connecting to Aurora clusters, this parameter is required when the connection string does not provide enough information about the database cluster domain name. If the Aurora cluster endpoint is used directly, the driver will recognize the standard Aurora domain name and can re-build a proper Aurora instance name when needed. In cases where the connection string uses an IP address, a custom domain name or localhost, the driver won't know how to build a proper domain name for a database instance endpoint. For example, if a custom domain was being used and the cluster instance endpoints followed a pattern of `instanceIdentifier1.customHost`, `instanceIdentifier2.customHost`, etc, the driver would need to know how to construct the instance endpoints using the specified custom domain. Because there isn't enough information from the custom domain alone to create the instance endpoints, the `HOST_PATTERN` should be set to `?.customHost`, making the connection string `SERVER=customHost;PORT=1234;DATABASE=test;HOST_PATTERN=?.customHost`. Refer to [Driver Behaviour During Failover For Different Connection URLs](#driver-behaviour-during-failover-for-different-connection-urls) for more examples.

## Failover Exception Codes

### 08S01 - Communication Link Failure
When the driver returns an error code ```08S01```, the original connection failed, and the driver tried to failover to a new instance, but was not able to. There are various reasons this may happen: no nodes were available, a network failure occurred, and so on. In this scenario, please wait until the server is up or other problems are solved.

### 08S02 - Communication Link Changed
When the driver returns an error code ```08S02```, the original connection failed while autocommit was set to true, and the driver successfully failed over to another available instance in the cluster. However, any session state configuration of the initial connection is now lost. In this scenario, you should:

1. Reconfigure and reuse the original connection (the reconfigured session state will be the same as the original connection).
2. Repeat the query that was executed when the connection failed and continue work as desired.

#### Sample Code
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

### 08007 - Connection Failure During Transaction
When the driver returns an error code ```08007```, the original connection failed within a transaction (while autocommit was set to false). In this scenario, when the transaction ends, the driver first attempts to rollback the transaction, and then fails over to another available instance in the cluster. Note that the rollback might be unsuccessful as the initial connection may be broken at the time that the driver recognizes the problem. Note also that any session state configuration of the initial connection is now lost. In this scenario, you should:

1. Reconfigure and reuse the original connection (the reconfigured session state will be the same as the original connection).
2. Re-start the transaction and repeat all queries which were executed during the transaction before the connection failed.
3. Repeat the query that was executed when the connection failed, and continue work.

#### Sample Code
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

## Logging

### Enabling Logs On Windows
When connecting the AWS ODBC Driver for MySQL using a Windows system, ensure logging is enabled by following the steps below:

1. Open the ODBC Data Source Administrator.
2. Add a new DSN or configure an existing DSN of your choice.
3. Open the details for the DSN.
4. Navigate to the Debug tab.
5. Ensure the box to log queries is checked.

#### Example
![enable-logging-windows](./enable-logging-windows.jpg)

The resulting log file, named `myodbc.log`, can be found under `%temp%`.

### Enabling Logs On MacOS and Linux
When connecting the AWS ODBC Driver for MySQL using a MacOS or Linux system, include the `LOG_QUERY` parameter in the connection string with the value of `1` to enable logging (`DSN=XXX;LOG_QUERY=1;...`). The log file, named `myodbc.log`, can be found in the current working directory.

>## :warning: Warnings About Proper Usage of the AWS ODBC Driver for MySQL
>It is highly recommended that you use the cluster and read-only cluster endpoints instead of the direct instance endpoints of your Aurora cluster, unless you are confident about your application's use of instance endpoints. Although the driver will correctly failover to the new writer instance when using instance endpoints, use of these endpoints is discouraged because individual instances can spontaneously change reader/writer status when failover occurs. The driver will always connect directly to the instance specified if an instance endpoint is provided, so a write-safe connection cannot be assumed if the application uses instance endpoints.
