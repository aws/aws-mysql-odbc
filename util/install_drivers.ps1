Write-Host "Copying AWS MySQL ODBC driver files to $env:windir"
Copy-Item ..\build\bin\release\*.* $env:windir\System32
Copy-Item ..\build\lib\Release\*.* $env:windir\System32
Set-Location $env:windir\System32
Write-Host "Installing AWS ANSI ODBC Driver for MySQL"
.\myodbc-installer -a -d -n "AWS ANSI ODBC Driver for MySQL" -t "DRIVER=awsmysqlodbca.dll;SETUP=awsmysqlodbcS.dll"
Write-Host "Installing AWS Unicode ODBC Driver for MySQL"
.\myodbc-installer -a -d -n "AWS Unicode ODBC Driver for MySQL" -t "DRIVER=awsmysqlodbcw.dll;SETUP=awsmysqlodbcS.dll"
Set-Location -
Write-Host "Complete"