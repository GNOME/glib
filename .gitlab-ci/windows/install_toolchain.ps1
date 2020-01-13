[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12;

$msvc_2017_url = 'https://aka.ms/vs/15/release/vs_buildtools.exe'
$msys_url = 'https://ayera.dl.sourceforge.net/project/msys2/Base/x86_64/msys2-base-x86_64-20180531.tar.xz'

Write-Host "Installing VisualStudio"
Invoke-WebRequest -Uri $msvc_2017_url -OutFile C:\vs_buildtools.exe
Start-Process C:\vs_buildtools.exe -ArgumentList '--quiet --wait --norestart --nocache --installPath C:\BuildTools --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended' -Wait
Remove-Item C:\vs_buildtools.exe -Force

Write-Host "Installing MSYS2"
Invoke-WebRequest -Uri $msys_url -OutFile C:\msys2-x86_64.tar.xz
7z e C:\msys2-x86_64.tar.xz -Wait
7z x C:\msys2-x86_64.tar -o"C:\\"
Remove-Item C:\msys2-x86_64.tar.xz -Force
Remove-Item C:\msys2-x86_64.tar -Force

Write-Host "Installing Meson"
pip install meson==0.49.2
