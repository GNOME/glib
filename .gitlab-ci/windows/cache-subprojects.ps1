[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12;

# FIXME: Python fails to validate github.com SSL certificate, unless we first
# run a dummy download to force refreshing Windows' CA database.
# See: https://bugs.python.org/issue36137
(New-Object System.Net.WebClient).DownloadString("https://gitlab.gnome.org") >$null

git clone https://gitlab.gnome.org/GNOME/glib.git
meson subprojects download --sourcedir glib
Remove-Item -Force 'glib/subprojects/*.wrap'
Move-Item glib/subprojects C:\subprojects
Remove-Item -Recurse -Force glib
