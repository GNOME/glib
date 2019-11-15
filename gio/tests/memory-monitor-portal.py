#!/usr/bin/python3

# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU Lesser General Public License as published by the Free
# Software Foundation; either version 3 of the License, or (at your option) any
# later version.  See http://www.gnu.org/copyleft/lgpl.html for the full text
# of the license.

__author__ = 'Bastien Nocera'
__email__ = 'hadess@hadess.net'
__copyright__ = '(c) 2019 Red Hat Inc.'
__license__ = 'LGPL 3+'

import unittest
import sys
import subprocess
import dbus
import dbus.mainloop.glib
import dbusmock
import fcntl
import os
import time

from gi.repository import GLib
from gi.repository import Gio

dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)

# XDG_DESKTOP_PORTAL_PATH = os.path.expanduser("~/.cache/jhbuild/build/xdg-desktop-portal/xdg-desktop-portal")
XDG_DESKTOP_PORTAL_PATH = "/usr/libexec/xdg-desktop-portal"

class TestLowMemoryMonitorPortal(dbusmock.DBusTestCase):
    '''Test GMemoryMonitorPortal'''

    @classmethod
    def setUpClass(klass):
        klass.start_system_bus()
        klass.dbus_con = klass.get_dbus(True)
        # Start session bus so that xdg-desktop-portal can run on it
        klass.start_session_bus()

    def setUp(self):
        (self.p_mock, self.obj_lmm) = self.spawn_server_template(
            'low_memory_monitor', {}, stdout=subprocess.PIPE)
        # set log to nonblocking
        flags = fcntl.fcntl(self.p_mock.stdout, fcntl.F_GETFL)
        fcntl.fcntl(self.p_mock.stdout, fcntl.F_SETFL, flags | os.O_NONBLOCK)
        self.last_warning = -1
        self.dbusmock = dbus.Interface(self.obj_lmm, dbusmock.MOCK_IFACE)
        self.xdp = subprocess.Popen([XDG_DESKTOP_PORTAL_PATH])
        try:
            self.wait_for_bus_object('org.freedesktop.portal.Desktop',
                                     '/org/freedesktop/portal/desktop')
        except:
            raise
        # subprocess.Popen(['gdbus', 'monitor', '--session', '--dest', 'org.freedesktop.portal.Desktop'])

        os.environ['GTK_USE_PORTAL'] = "1"
        self.memory_monitor = Gio.MemoryMonitor.dup_default()
        assert("GMemoryMonitorPortal" in str(self.memory_monitor))
        self.memory_monitor.connect("low-memory-warning", self.portal_memory_warning_cb)
        self.mainloop = GLib.MainLoop()
        main_context = self.mainloop.get_context()

    def tearDown(self):
        self.p_mock.terminate()
        self.p_mock.wait()

    def wait_loop(self):
        main_context = self.mainloop.get_context()
        timeout = 2
        while timeout > 0:
            time.sleep(0.5)
            timeout -= 0.5
            main_context.iteration(False)

    def portal_memory_warning_cb(self, monitor, level):
        print ("portal_memory_warning_cb %d" % level)
        self.last_warning = level

    def test_low_memory_warning_portal_signal(self):
        '''LowMemoryWarning signal'''

        self.wait_loop()

        self.dbusmock.EmitWarning(100)
        self.wait_loop()
        self.assertEqual (self.last_warning, 100)

        self.dbusmock.EmitWarning(255)
        self.wait_loop()
        self.assertEqual (self.last_warning, 255)


if __name__ == '__main__':
    # avoid writing to stderr
    unittest.main(testRunner=unittest.TextTestRunner(stream=sys.stdout, verbosity=2))
