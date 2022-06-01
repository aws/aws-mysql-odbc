Copyright Amazon.com Inc. or affiliates.

This is a release of the Amazon Web Services (AWS) ODBC Driver for MySQL, a driver 
that enables ODBC applications to communicate with MySQL servers.

License information can be found in the LICENSE file.
This distribution may include materials developed by third parties. For license
and attribution notices for these materials, please refer to the LICENSE file.

ABOUT THE DRIVER
================

The Amazon Web Services (AWS) ODBC Driver for MySQL allows an application to take 
advantage of the features of clustered MySQL databases. It is based on and can be 
used as a drop-in compatible for the MySQL Connector/ODBC driver which can be found 
at <https://github.com/mysql/mysql-connector-odbc/>, and is compatible with all 
MySQL deployments.

-- What is Failover?
An Amazon Aurora DB cluster uses failover to automatically repair the DB cluster 
status when a primary DB instance becomes unavailable. During failover, Aurora 
promotes a replica to become the new primary DB instance, so that the DB cluster 
can provide maximum availability to a primary read-write DB instance. The AWS 
ODBC Driver for MySQL is designed to coordinate with this behavior in order to 
provide minimal downtime in the event of a DB instance failure.

-- Benefits of the AWS ODBC Driver for MySQL
Although Aurora is able to provide maximum availability through the use of failover, 
existing client drivers do not fully support this functionality. This is partially 
due to the time required for the DNS of the new primary DB instance to be fully 
resolved in order to properly direct the connection. The AWS ODBC Driver for MySQL 
fully utilizes failover behavior by maintaining a cache of the Aurora cluster 
topology and each DB instance's role (Aurora Replica or primary DB instance). This 
topology is provided via a direct query to the Aurora database, essentially providing 
a shortcut to bypass the delays caused by DNS resolution. With this knowledge, the 
AWS ODBC Driver can more closely monitor the Aurora DB cluster status so that a 
connection to the new primary DB instance can be established as fast as possible. 
Additionally, as noted above, the AWS ODBC Driver is designed to be drop-in 
compatible for other MySQL ODBC drivers and can be used to interact with RDS and 
MySQL databases as well as Aurora MySQL.

DOCUMENTATION LOCATION
======================

To view the full documentation, visit <https://github.com/awslabs/aws-mysql-odbc>. 
This page contain details on how the driver works and how to use it.

For additional documentation on the AWS ODBC Driver for MySQL, please refer to the 
documentation for the open-source mysql-connector-odbc driver that the AWS ODBC Driver
for MySQL is based on: <https://dev.mysql.com/doc/connector-cpp/8.0/en/>.

CONTACT
=======

If you encounter a problem with the AWS ODBC Driver for MySQL, we would like to hear 
about it. Please search the existing issues at 
<https://github.com/awslabs/aws-mysql-odbc/issues> and see if others are also 
experiencing the issue before opening a new issue. When opening a new issue, please include
the following information: 

- the version of AWS ODBC Driver for MySQL
- C++ language version
- the OS platform and version
- the MySQL database version you're running against. 

Please include a reproduction case for the issue when appropriate.

The GitHub issues are intended for bug reports and feature requests. Keeping the list 
of open issues lean will help us respond in a timely manner.

LICENSE
=======
This software is released under version 2 of the GNU General Public License (GPLv2).
