# Building the AWS SDK for C++
The driver supports AWS IAM and AWS Secrets Manager authentication by using [AWS SDK for C++](https://docs.aws.amazon.com/sdk-for-cpp/). The driver requires `aws-cpp-sdk-core`, `aws-cpp-sdk-rds`, and `aws-cpp-sdk-secretsmanager`. You can either build the SDK yourself directly from the source using the script under `scripts` folder or [download the libraries using a package manager](https://docs.aws.amazon.com/sdk-for-cpp/v1/developer-guide/sdk-from-pm.html). The script will install the SDK under `aws_sdk/install` folder. The driver will look for SDK dependencies through `aws_sdk/install` folder and `CMAKE_PREFIX_PATH` environment variable.
> **_NOTE:_** On Winodws, depending on the use case, you may build the SDK as a static library or dynamic library. On Linux and macOS, you should always build the SDK as a dynamic library.

## Windows
If you want to build and run the driver unit tests, you need to build the SDK as static library, as the unit test binary will link every dependency statically.
```
.\scripts\build_aws_sdk_win.ps1 x64 Debug OFF "Visual Studio 16 2019"
```
If you want to build the driver DLL, you need to build the SDK as dynamic library, as the driver DLL will link the SDK dynamically.
```
.\scripts\build_aws_sdk_win.ps1 x64 Release ON "Visual Studio 16 2019"
```
## Linux/macOS
```
./scripts/build_aws_sdk_unix.sh Release
```

## Troubleshoot
See https://docs.aws.amazon.com/sdk-for-cpp/v1/developer-guide/troubleshooting-cmake.html