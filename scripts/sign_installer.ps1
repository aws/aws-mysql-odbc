# Sign a single file
function Invoke-SignFile {
    [OutputType([Boolean])]
    Param(
        # The path to the file to sign
        [Parameter(Mandatory=$true)]
        [string]$SourcePath,
        # The path to the signed file
        [Parameter(Mandatory=$true)]
        [string]$TargetPath,
        # The name of the unsigned AWS bucket
        [Parameter(Mandatory=$true)]
        [string]$AwsUnsignedBucket,
        [Parameter(Mandatory=$true)]
        # The name of the signed AWS bucket
        [string]$AwsSignedBucket,
        # The name of the AWS key
        [Parameter(Mandatory=$true)]
        [string]$AwsKey,
        [Parameter(Mandatory=$false)]
        [bool]$AsMockResponse=$false
    )

    Write-Host "Signing is enabled. Will attempt to sign"
    
    # Remember to install 'jq' dependency before
    if ($AsMockResponse) {
        Copy-Item $SourcePath $TargetPath
        return $true
    }
    
    # Upload unsigned .msi to S3 Bucket
    Write-Host "Obtaining version id and uploading unsigned .msi to S3 Bucket"
    $versionId = $( aws s3api put-object --bucket $AwsUnsignedBucket --key $AwsKey --body $SourcePath --acl bucket-owner-full-control | jq '.VersionId' )
    $versionId = $versionId.Replace("`"","")
    $jobId = ""

    if ([string]::IsNullOrEmpty($versionId)) {
        Write-Host "Failed to PUT unsigned file in bucket."
        return $false
    }
    
    # Attempt to get Job ID from bucket tagging, will retry up to 3 times before exiting with a failure code.
    # Will sleep for 5 seconds between retries.
    Write-Host "Attempt to get Job ID from bucket tagging, will retry up to 3 times before exiting with a failure code."
    for ( $i = 0; $i -lt 3; $i++ ) {
        # Get job ID
        $id=$( aws s3api get-object-tagging --bucket $AwsUnsignedBucket --key $AwsKey --version-id $versionId | jq -r '.TagSet[0].Value' )
        if ( $id -ne "null" ) {
            $jobId = $id
            break
        }
    
        Write-Host "Will sleep for 5 seconds between retries."
        Start-Sleep -Seconds 5
    }
    
    if ( $jobId -eq "" ) {
        Write-Host "Exiting because unable to retrieve job ID"
        return $false
    }
    
    # Poll signed S3 bucket to see if the signed artifact is there
    Write-Host "Poll signed S3 bucket to see if the signed artifact is there"
    aws s3api wait object-exists --bucket $AwsSignedBucket --key $AwsKey-$jobId
    
    # Get signed msi from S3
    Write-Host "Get signed msi from S3 to $TargetPath"
    aws s3api get-object --bucket $AwsSignedBucket --key $AwsKey-$jobId $TargetPath
    
    Write-Host "Signing completed"
    return $true
}

# Sign the installer file for architecture and ODBC version
function Invoke-SignInstaller {
    [OutputType([Boolean])]
    Param(
        # The path to the build directory.
        [Parameter(Mandatory=$true)]
        [string]$BuildDir,
        # The architecture name.
        [Parameter(Mandatory=$true)]
        [string]$Architecture,
        # The ODBC version.
        [Parameter(Mandatory=$true)]
        [string]$OdbcVersion,
        # The name of the unsigned AWS bucket
        [Parameter(Mandatory=$true)]
        [string]$AwsUnsignedBucket,
        [Parameter(Mandatory=$true)]
        # The name of the signed AWS bucket
        [string]$AwsSignedBucket,
        # The name of the AWS key
        [Parameter(Mandatory=$true)]
        [string]$AwsKey,
        [Parameter(Mandatory=$false)]
        [bool]$AsMockResponse=$false
    )

    $unsignedInstallerPath=$(Join-Path $BuildDir "aws-mysql-odbc-$OdbcVersion-$Architecture.msi")
    $signedInstallerPath=$(Join-Path $BuildDir "aws-mysql-odbc-$OdbcVersion-$Architecture-signed.msi")
    
    Write-Host "unsignedInstallerPath=${unsignedInstallerPath}"
    Write-Host "signedInstallerPath=${signedInstallerPath}"

    # Sign the installer
    Write-Host "Signing the installer."
    if ( !(Invoke-SignFile $unsignedInstallerPath $signedInstallerPath $AwsUnsignedBucket $AwsSignedBucket $AwsKey -AsMockResponse $AsMockResponse) ) {
        Write-Host "Failed to sign installer file."
        return $false
    }

    # Remove unsigned installer and remove "-signed" in signed installer name
    Write-Host "Removing unsigned executable."
    Remove-Item -Path $unsignedInstallerPath
    Rename-Item -Path $signedInstallerPath -NewName "aws-mysql-odbc-$OdbcVersion-$Architecture.msi"

    return $true
}
