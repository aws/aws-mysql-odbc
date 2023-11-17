# Building the AWS SDK for C++
The driver supports AWS IAM and AWS Secrets Manager authentication by using [AWS SDK for C++](https://docs.aws.amazon.com/sdk-for-cpp/). The driver requires `aws-cpp-sdk-core`, `aws-cpp-sdk-rds`, and `aws-cpp-sdk-secretsmanager`. You can either build the SDK yourself directly from the source using the script under `scripts` folder or [download the libraries using a package manager](https://docs.aws.amazon.com/sdk-for-cpp/v1/developer-guide/sdk-from-pm.html). The script will install the SDK under `aws_sdk/install` folder. The driver will look for SDK dependencies through `aws_sdk/install` folder and `CMAKE_PREFIX_PATH` environment variable.
> **_NOTE:_** On Windows, depending on the use case, you may build the SDK as a static library or dynamic library. On Linux and macOS, you should always build the SDK as a dynamic library.

## Windows
### For release version unit tests or driver
```
.\scripts\build_aws_sdk_win.ps1 x64 Release ON "Visual Studio 16 2019"
```
### For debug version unit tests
```
.\scripts\build_aws_sdk_win.ps1 x64 Debug ON "Visual Studio 16 2019"
```
Note:
- Use ```Visual Studio 17 2022`` for Visual Studio 2022. 

## Linux/macOS
```
./scripts/build_aws_sdk_unix.sh Release
```

## Troubleshoot
See https://docs.aws.amazon.com/sdk-for-cpp/v1/developer-guide/troubleshooting-cmake.html