# MySQL Connector/ODBC Driver

You can get the latest stable release from the [MySQL downloads](https://dev.mysql.com/downloads/connector/odbc/).

For detailed information please visit the official [MySQL Connector/ODBC documentation](https://dev.mysql.com/doc/connector-odbc/en/).

Source packages are available from our [github releases page](https://github.com/mysql/mysql-connector-odbc/releases).

## Licensing

Please refer to files README and LICENSE, available in this repository, and [Legal Notices in documentation](https://dev.mysql.com/doc/connector-cpp/8.0/en/preface.html) for further details. 

## Download & Install

MySQL Connector/ODBC can be installed from pre-compiled packages that can be downloaded from the [MySQL downloads page](https://dev.mysql.com/downloads/connector/odbc/).
The process of installing of Connector/ODBC from a binary distribution is described in [MySQL online manuals](https://dev.mysql.com/doc/connector-odbc/en/connector-odbc-installation.html)

### Building from sources

MySQL Connector/ODBC can be installed from the source. Please select the relevant platform in [MySQL online manuals](https://dev.mysql.com/doc/connector-odbc/en/connector-odbc-installation.html)

### GitHub Repository

This repository contains the MySQL Connector/ODBC source code as per latest released version. You should expect to see the same contents here and within the latest released Connector/ODBC package.

## Usage Scenarios

The MySQL Connector/ODBC can be used in a variety of programming languages and applications.
The most popular of them are:

* C and C++ programming using ODBC API
* C++ programming using ADODB objects
* Visual Basic programming using ADODB objects
* Java through JDBC/ODBC bridge
* .NET platform with ADO.NET/ODBC bridge
* PHP, Perl, Python, Ruby, Erlang.
* Office applications through linked tables and Visual Basic integration
* Multitude of other applications supporting ODBC

## Testing

Use Visual Studio to build the `testing` project. Navigate to the build directory and run `testing.exe`. To specify a particular test or test suite, use `--gtest_filter` in the command.

The following is an example running all the tests in the `TopologyServiceTest` suite:
```
PS C:\Other\dev\Bit-Quill\mysql-connector-odbc\testing\bin\RelWithDebInfo> .\testing.exe --gtest_filter=TopologyServiceTest.*
Running main() from C:\Other\dev\Bit-Quill\mysql-connector-odbc\_deps\googletest-src\googletest\src\gtest_main.cc
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

## Documentation

* [MySQL](http://www.mysql.com/)
* [Connector ODBC Developer Guide](https://dev.mysql.com/doc/connector-odbc/en/)
* [ODBC API Reference MSDN](https://msdn.microsoft.com/en-us/ie/ms714562(v=vs.94))

## Questions/Bug Reports

* [Discussion Forum](https://forums.mysql.com/list.php?37)
* [Slack](https://mysqlcommunity.slack.com)
* [Bugs](https://bugs.mysql.com)

## Contributing

Please see our [guidelines](CONTRIBUTING.md) for contributing to the driver.
