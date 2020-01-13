# escape=`

FROM 'mcr.microsoft.com/windows/servercore:1909'

# Make sure any failure in PowerShell scripts is fatal
SHELL ["powershell", "-Command", "$ErrorActionPreference = 'Stop';"]
ENV ErrorActionPreference='Stop'

COPY install_toolchain.ps1 cache-subprojects.ps1 C:\
RUN C:\install_toolchain.ps1
RUN C:\cache-subprojects.ps1
