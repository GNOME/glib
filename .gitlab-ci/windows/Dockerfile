# escape=`

FROM microsoft/windowsservercore

# Make sure any failure in PowerShell scripts is fatal
SHELL ["powershell", "-Command", "$ErrorActionPreference = 'Stop';"]

COPY prepare.ps1 C:\
RUN C:\prepare.ps1

# FIXME: Fix missing CA when Meson downloads tarballs over https. We should
# cache those tarballs inside the docker image instead of downloading them for
# each job.
ENV SSL_CERT_FILE='C:\mingit\mingw64\ssl\cert.pem'
