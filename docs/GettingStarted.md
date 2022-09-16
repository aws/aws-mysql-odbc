# Getting Started

## Obtaining the AWS Advanced JDBC Wrapper

You will find installers for Windows, MacOS and Linux that can be downloaded directly from [GitHub Releases](https://github.com/awslabs/aws-mysql-odbc/releases) to install the AWS ODBC Driver for MySQL.

## Installing the AWS ODBC Driver for MySQL

### Windows
Download the `.msi` Windows installer for your system; execute the installer and follow the onscreen instructions. The default target installation location for the driver files is `C:\Program Files\AWS ODBC Driver for MySQL`. An ANSI driver and a Unicode driver will be installed, named respectively `AWS ODBC ANSI Driver for MySQL` and `AWS ODBC Unicode Driver for MySQL`. To uninstall the ODBC driver, open the same installer file, select the option to uninstall the driver and follow the onscreen instructions to successfully uninstall the driver.

### MacOS
Download the `.pkg` installer; run the installer and follow the onscreen instructions. The default target installation location for the driver folder is `/usr/local/`. Note that for a MacOS system, additional steps are required to configure the driver and Data Source Name (DSN) entries before you can use the driver(s). Initially, the installer will register two driver entries with two corresponding DSN entries. For information about [how to configure the driver and DSN settings](#configuring-the-driver-and-dsn-entries), review the configuration sample. There is no uninstaller at this time, but all the driver files can be removed by deleting the target installation directory.

### Linux
Download the `.tar.gz` file, and extract the contents to your desired location. For a Linux system, additional steps are required to configure the driver and Data Source Name (DSN) entries before the driver(s) can be used. For more information, see [Configuring the Driver and DSN settings](#configuring-the-driver-and-dsn-entries). There is no uninstaller at this time, but all the driver files can be removed by deleting the target installation directory.

### Configuring the Driver and DSN Entries 
To configure the driver on Windows, use the `ODBC Data Source Administrator` tool to add or configure a DSN for either the `AWS ODBC ANSI Driver for MySQL` or `AWS ODBC Unicode Driver for MySQL`. With this DSN you can specify the options for the desired connection. Additional configuration properties are available by clicking the `Details >>` button.

To use the driver on MacOS or Linux systems, you need to create two files (`odbc.ini` and `odbcinst.ini`), that will contain the configuration for the driver and the Data Source Name (DSN).

You can modify the files manually, or through tools with a GUI such as `iODBC Administrator` (available for MacOS). In the following sections, we show samples of `odbc.ini` and `odbcinst.ini` files that describe how an ANSI driver could be set up for a MacOS system. In a MacOS system, the `odbc.ini` and `odbcinst.ini` files are typically located in the `/Library/ODBC/` directory.

For a Linux system, the files would be similar, but the driver file would have the `.so` extension instead of the `.dylib` extension shown in the sample. On a Linux system, the `odbc.ini` and `odbcinst.ini` files are typically located in the `/etc` directory.

#### odbc.ini
```bash
[ODBC Data Sources]
awsmysqlodbcw = AWS ODBC Unicode Driver for MySQL
awsmysqlodbca = AWS ODBC ANSI Driver for MySQL

[awsmysqlodbcw]
Driver                             = AWS ODBC Unicode Driver for MySQL
SERVER                             = localhost
NO_SCHEMA                          = 1
TOPOLOGY_REFRESH_RATE              = 30000
FAILOVER_TIMEOUT                   = 60000
FAILOVER_TOPOLOGY_REFRESH_RATE     = 5000
FAILOVER_WRITER_RECONNECT_INTERVAL = 5000
FAILOVER_READER_CONNECT_TIMEOUT    = 30000
CONNECT_TIMEOUT                    = 30
NETWORK_TIMEOUT                    = 30

[awsmysqlodbca]
Driver                             = AWS ODBC ANSI Driver for MySQL
SERVER                             = localhost
NO_SCHEMA                          = 1
TOPOLOGY_REFRESH_RATE              = 30000
FAILOVER_TIMEOUT                   = 60000
FAILOVER_TOPOLOGY_REFRESH_RATE     = 5000
FAILOVER_WRITER_RECONNECT_INTERVAL = 5000
FAILOVER_READER_CONNECT_TIMEOUT    = 30000
CONNECT_TIMEOUT                    = 30
NETWORK_TIMEOUT                    = 30
```

#### odbcinst.ini
```bash
[ODBC Drivers]
AWS ODBC Unicode Driver for MySQL = Installed
AWS ODBC ANSI Driver for MySQL    = Installed

[AWS ODBC Unicode Driver for MySQL]
Driver = /usr/local/aws-mysql-odbc-0.1.0-macos/lib/awsmysqlodbcw.dylib

[AWS ODBC ANSI Driver for MySQL]
Driver = /usr/local/aws-mysql-odbc-0.1.0-macos/lib/awsmysqlodbca.dylib
```
