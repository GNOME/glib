[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12;

git clone https://gitlab.gnome.org/GNOME/glib.git
meson subprojects download --sourcedir glib
Remove-Item -Force 'subprojects/*.wrap'
Move-Item subprojects C:\subprojects
Remove-Item -Recurse -Force glib
