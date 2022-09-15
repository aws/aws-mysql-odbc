# Amazon Web Services (AWS) ODBC Driver for MySQL

[![build_status](https://github.com/awslabs/aws-mysql-odbc/actions/workflows/main.yml/badge.svg)](https://github.com/awslabs/aws-mysql-odbc/actions/workflows/main.yml)
[![License](https://img.shields.io/badge/license-GPLv2-blue)](LICENSE)

**The Amazon Web Services (AWS) ODBC Driver for MySQL** allows an application to take advantage of the features of clustered MySQL databases. It is based on and can be used as a drop-in compatible for the [MySQL Connector/ODBC driver](https://github.com/mysql/mysql-connector-odbc/), and is compatible with all MySQL deployments.

## Table of Contents
- [Amazon Web Services (AWS) ODBC Driver for MySQL](#amazon-web-services-aws-odbc-driver-for-mysql)
  - [Table of Contents](#table-of-contents)
  - [About the Driver](#about-the-driver)
    - [What is Failover?](#what-is-failover)
    - [Benefits of the AWS ODBC Driver for MySQL](#benefits-of-the-aws-odbc-driver-for-mysql)
  - [Getting Started](#getting-started)
  - [Using the Driver](#using-the-driver)
  - [Documentation](#documentation)
  - [Getting Help and Opening Issues](#getting-help-and-opening-issues)
  - [License](#license)

## About the Driver

### What is Failover?
In an Amazon Aurora database (DB) cluster, failover is a mechanism by which Aurora automatically repairs the DB cluster status when a primary DB instance becomes unavailable. It achieves this goal by electing an Aurora Replica to become the new primary DB instance, so that the DB cluster can provide maximum availability to a primary read-write DB instance. The AWS ODBC Driver for MySQL is designed to coordinate with this behavior in order to provide minimal downtime in the event of a DB instance failure.

### Benefits of the AWS ODBC Driver for MySQL
Although Aurora is able to provide maximum availability through the use of failover, existing client drivers do not currently support this functionality. This is partially due to the time required for the DNS of the new primary DB instance to be fully resolved in order to properly direct the connection. The AWS ODBC Driver for MySQL fully utilizes failover behavior by maintaining a cache of the Aurora cluster topology and each DB instance's role (Aurora Replica or primary DB instance). This topology is provided via a direct query to the Aurora database, essentially providing a shortcut to bypass the delays caused by DNS resolution. With this knowledge, the AWS ODBC Driver for MySQL can more closely monitor the Aurora DB cluster status so that a connection to the new primary DB instance can be established as fast as possible. Additionally, as noted above, the AWS ODBC Driver for MySQL is designed to be drop-in compatible for other MySQL ODBC drivers and can be used to interact with Aurora MySQL, RDS MySQL, and commercial/open-source MySQL databases.

## Getting Started
For more information on how to install and configure the AWS ODBC Driver for MySQL, please visit the [Getting Started page](./docs/GettingStarted.md).

## Using the Driver
Please refer to the AWS Driver's [documentation](./docs/Documentation.md) for details on how to use, build, and test the AWS Driver.

## Documentation
Technical documentation regarding the functionality of the AWS ODBC Driver for MySQL will be maintained in this GitHub repository. For additional documentation, [please refer to the documentation for the open-source mysql-connector-odbc driver that the AWS ODBC Driver for MySQL is based on](https://dev.mysql.com/doc/connector-cpp/8.0/en/).

## Getting Help and Opening Issues
If you encounter an issue or bug with the AWS ODBC Driver for MySQL, we would like to hear about it. Please search the [existing issues](https://github.com/awslabs/aws-mysql-odbc/issues) and see if others are also experiencing the same problem before opening a new issue. When opening a new issue, please make sure to provide: 

- the version of AWS ODBC Driver for MySQL
- C++ language version
- the OS platform and version
- the MySQL database version you are running

Please include a reproduction case for the issue when appropriate. Also please [include driver logs](./docs/using-the-aws-driver/UsingTheAwsDriver.md#logging) if possible, as they help us diagnose problems quicker. 

The GitHub issues are intended for bug reports and feature requests. Keeping the list of open issues lean will help us respond in a timely manner.

## License
This software is released under version 2 of the GNU General Public License (GPLv2).