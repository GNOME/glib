# escape=`

FROM microsoft/windowsservercore

RUN powershell.exe -command `
    $ErrorActionPreference = 'Stop' ; `
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12 ; `
    `
    Invoke-WebRequest -Uri 'https://aka.ms/vs/15/release/vs_buildtools.exe' -OutFile C:\vs_buildtools.exe ; `
    Start-Process C:\vs_buildtools.exe -ArgumentList '--quiet --wait --norestart --nocache `
        --installPath C:\BuildTools `
        --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended' -Wait ; `
    Remove-Item C:\vs_buildtools.exe -Force ; `
    `
    Invoke-WebRequest -Uri 'https://www.python.org/ftp/python/3.7.0/python-3.7.0.exe' -OutFile C:\python-3.7.0.exe ; `
    Start-Process C:\python-3.7.0.exe -ArgumentList '/quiet InstallAllUsers=1 PrependPath=1' -Wait ; `
    Remove-Item C:\python-3.7.0.exe -Force ; `
    `
    Invoke-WebRequest -Uri 'https://github.com/git-for-windows/git/releases/download/v2.19.1.windows.1/MinGit-2.19.1-64-bit.zip' `
        -OutFile C:\mingit.zip ; `
    Expand-Archive C:\mingit.zip -DestinationPath c:\mingit ; `
    Remove-Item C:\mingit.zip -Force ; `
    $env:PATH = [System.Environment]::GetEnvironmentVariable('PATH', 'Machine') + ';' + 'c:\mingit\cmd' ; `
    [Environment]::SetEnvironmentVariable('PATH', $env:PATH, [EnvironmentVariableTarget]::Machine)

RUN pip install meson==0.48.0

# Start developer command prompt with any other commands specified.
ENTRYPOINT C:\BuildTools\Common7\Tools\VsDevCmd.bat &&

# Default to PowerShell if no other command specified.
CMD ["powershell.exe", "-NoLogo", "-ExecutionPolicy", "Bypass"]
