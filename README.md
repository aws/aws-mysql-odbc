# Amazon Web Services (AWS) ODBC Driver for MySQL

**The Amazon Web Services (AWS) ODBC Driver for MySQL** allows an application to take advantage of the features of clustered MySQL databases. It is based on and can be used as a drop-in compatible for the [MySQL Connector/ODBC driver](https://github.com/mysql/mysql-connector-odbc/), and is compatible with all MySQL deployments.

## Table of Contents
- [Amazon Web Services (AWS) ODBC Driver for MySQL](#amazon-web-services-aws-odbc-driver-for-mysql)
  - [Table of Contents](#table-of-contents)
  - [What is Failover?](#what-is-failover)
  - [Benefits of the AWS ODBC Driver for MySQL](#benefits-of-the-aws-odbc-driver-for-mysql)
  - [Getting Started](#getting-started)
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
