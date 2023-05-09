# Building the AWS ODBC Driver for MySQL

## Windows
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

## MacOS
1. Install the following packages available on `Homebrew` or other package management system of your choice:
     - `libiodbc`
     - `cmake`
     - `mysql-client`
     - `mysql`
     - `aws-sdk-cpp`
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

### Troubleshoot
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

## Linux
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
