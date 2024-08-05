Set-Location $env:windir\System32
Write-Host "Uninstalling AWS ANSI ODBC Driver for MySQL"
.\myodbc-installer -r -d -n "AWS ANSI ODBC Driver for MySQL"
Write-Host "Uninstalling AWS Unicode ODBC Driver for MySQL"
.\myodbc-installer -r -d -n "AWS Unicode ODBC Driver for MySQL"
Set-Location -
Write-Host "Complete"