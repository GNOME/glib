# escape=`

FROM 'mcr.microsoft.com/windows/servercore:1809'

# Make sure any failure in PowerShell scripts is fatal
SHELL ["powershell", "-Command", "$ErrorActionPreference = 'Stop';"]
ENV ErrorActionPreference='Stop'

COPY install_choco.ps1 C:\
RUN C:\install_choco.ps1

RUN pip install meson==0.49.2

COPY cache-subprojects.ps1 C:\
RUN C:\cache-subprojects.ps1
