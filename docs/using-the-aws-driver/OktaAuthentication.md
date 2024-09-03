### Okta Authentication Plu# Okta Authentication Plugin

The Okta Authentication Plugin adds support for authentication via Federated Identity and then database access via IAM.

## What is Federated Identity

Federated Identity allows users to use the same set of credentials to access multiple services or resources across
different organizations. This works by having Identity Providers (IdP) that manage and authenticate user credentials,
and Service Providers (SP) that are services or resources that can be internal, external, and/or belonging to various
organizations. Multiple SPs can establish trust relationships with a single IdP.

When a user wants access to a resource, it authenticates with the IdP. From this an authentication token generated and is
passed to the SP then grants access to said resource.
In the case of AD FS, the user signs into the AD FS sign in page. This generates a SAML Assertion which acts as a
security token. The user then passes the SAML Assertion to the SP when requesting access to resources. The SP verifies
the SAML Assertion and grants access to the user.

## How to use Okta Authentication?

> [!NOTE]\
> AWS IAM database authentication is needed to use Okta Authentication. This is because after the plugin
> acquires SAML assertion from the identity provider, the SAML Assertion is then used to acquire an AWS authentication token. The
> AWS authentication token is then subsequently used to access the database.

1. Enable AWS IAM database authentication on an existing database or create a new database with AWS IAM database authentication on the AWS RDS Console:
    1. If needed, review the documentation about [creating a new database](https://docs.aws.amazon.com/AmazonRDS/latest/UserGuide/USER_CreateDBInstance.html).
    2. If needed, review the documentation about [modifying an existing database](https://docs.aws.amazon.com/AmazonRDS/latest/UserGuide/Overview.DBInstance.Modifying.html).
2. Set up an [AWS IAM policy](https://docs.aws.amazon.com/AmazonRDS/latest/UserGuide/UsingWithRDS.IAMDBAuth.IAMPolicy.html) for AWS IAM database authentication.
3. [Create a database account](https://docs.aws.amazon.com/AmazonRDS/latest/UserGuide/UsingWithRDS.IAMDBAuth.DBAccounts.html) using AWS IAM database authentication:
   Use the following commands to create a new IAM user:<br>
   `CREATE USER example_user_name IDENTIFIED WITH AWSAuthenticationPlugin AS 'RDS';`
   `GRANT ALL PRIVILEGES ON example_database.* TO 'example_user_name'@'%';`
4. Connect to a MySQL database with the following connection parameters configured in a DSN or connection string. (Note these are in addition to the parameters that you can configure for the [MySQL Connector/ODBC driver](https://dev.mysql.com/doc/connector-odbc/en/connector-odbc-configuration-connection-parameters.html))

| Parameter                     | Required | Description                                                                                                                                                                                                    | Default Value            | Example Value                                          |
|-------------------------------|:--------:|:---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|--------------------------|--------------------------------------------------------|
| `IDP_USERNAME`                |   Yes    | The user name to log into Okta.                                                                                                                                                                                | `null`                   | `jimbob@example.com`                                   |
| `IDP_PASSWORD`                |   Yes    | The password associated with the `IDP_ENDPOINT` username. This is used to log into the Okta.                                                                                                                   | `null`                   | `someRandomPassword`                                   |
| `IDP_ENDPOINT`                |   Yes    | The hosting URL for the Okta service that you are using to authenticate into AWS Aurora.                                                                                                                       | `null`                   | `ec2amaz-ab3cdef.example.com`                          |
| `APP_ID`                      |   Yes    | The Amazon Web Services (AWS) app [configured](https://help.okta.com/en-us/content/topics/deploymentguides/aws/aws-configure-aws-app.htm) on Okta.                                                             | `null`                   | `ec2amaz-ab3cdef.example.com`                          |
| `IAM_ROLE_ARN`                |   Yes    | The ARN of the IAM Role that is to be assumed to access AWS Aurora.                                                                                                                                            | `null`                   | `arn:aws:iam::123456789012:role/adfs_example_iam_role` |
| `IAM_IDP_ARN`                 |   Yes    | The ARN of the Identity Provider.                                                                                                                                                                              | `null`                   | `arn:aws:iam::123456789012:saml-provider/adfs_example` |
| `AWS_REGION`                  |   Yes    | The AWS region where the identity provider is located.                                                                                                                                                         | `null`                   | `us-east-2`                                            |
| `USERNAME`                    |   Yes    | The Username must be set to the [IAM database user](https://docs.aws.amazon.com/AmazonRDS/latest/UserGuide/UsingWithRDS.IAMDBAuth.html).                                                                       | `null`                   | `us-east-2`                                            |
| `IDP_PORT`                    |    No    | The port that the host for the authentication service listens at.                                                                                                                                              | `urn:amazon:webservices` | `urn:amazon:webservices`                               |
| `IAM_HOST`                    |    NO    | Overrides the host used to generate the authentication token. This is useful when you are connecting using a custom endpoint, since authentication tokens need to be generated using the RDS/Aurora endpoints. | `NULL`                   | `database.cluster-hash.region.rds.amazonaws.com`       |
| `IAM_DEFAULT_PORT`            |    No    | This property overrides the default port that is used to generate the authentication token. The default port is the default MySQL port.                                                                        | `3306`                   | `1234`                                                 |
| `IAM_TOKEN_EXPIRATION`        |    No    | Overrides the default IAM token cache expiration in seconds.                                                                                                                                                   | `900`                    | `123`                                                  |
| `HTTP_CLIENT_SOCKET_TIMEOUT`  |    No    | The socket timeout value in milliseconds for the HttpClient used during the Okta authentication workflow.                                                                                                      | `60000`                  | `60000`                                                |
| `HTTP_CLIENT_CONNECT_TIMEOUT` |    No    | The connect timeout value in milliseconds for the HttpClient used during the Okta authentication workflow.                                                                                                     | `60000`                  | `60000`                                                |
| `SSL_INSECURE`                |    No    | Indicates whether or not the SSL connection is secure or not. If not, it will allow SSL connections to be made without validating the server's certificates.                                                   | `true`                   | `false`                                                |
