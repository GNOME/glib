.. _gapplication(1):
.. meta::
   :copyright: Copyright 2013 Allison Karlitskaya
   :license: LGPL-2.1-or-later
..
   This has to be duplicated from above to make it machine-readable by `reuse`:
   SPDX-FileCopyrightText: 2013 Allison Karlitskaya
   SPDX-License-Identifier: LGPL-2.1-or-later

============
gapplication
============

--------------------------
D-Bus application launcher
--------------------------

SYNOPSIS
--------

|  **gapplication** help [COMMAND]
|  **gapplication** version
|  **gapplication** list-apps
|  **gapplication** launch <APP-ID>
|  **gapplication** launch <APP-ID> [FILE…]
|  **gapplication** list-actions <APP-ID>
|  **gapplication** action <APP-ID> <ACTION> [PARAMETER]

DESCRIPTION
-----------

``gapplication`` is a commandline implementation of the client-side of the
``org.freedesktop.Application`` interface as specified by the freedesktop.org
Desktop Entry Specification.

``gapplication`` can be used to start applications that have ``DBusActivatable``
set to ``true`` in their ``.desktop`` files and can be used to send messages to
already-running instances of other applications.

It is possible for applications to refer to ``gapplication`` in the ``Exec``
line of their ``.desktop`` file to maintain backwards compatibility with
implementations that do not directly support ``DBusActivatable``.

``gapplication`` ships as part of GLib.

COMMANDS
--------

``help [COMMAND]``

  Displays a short synopsis of the available commands or provides detailed help
  on a specific command.

``version``

  Prints the GLib version whence ``gapplication`` came.

``list-apps``

  Prints a list of all application IDs that are known to support D-Bus
  activation.  This list is generated by scanning ``.desktop`` files as per the
  current ``XDG_DATA_DIRS``.

``launch <APP-ID> [FILE…]``

  Launches an application.

  The first parameter is the application ID in the familiar ‘reverse DNS’ style
  (e.g. ``org.gnome.app``) without the ``.desktop`` suffix.

  Optionally, if additional parameters are given, they are treated as the names
  of files to open and may be filenames or URIs.  If no files are given then the
  application is simply activated.

``list-actions <APP-ID>``

  List the actions declared in the application’s ``.desktop`` file.  The
  parameter is the application ID, as above.

``action <APP-ID> <ACTION> [PARAMETER]``

  Invokes the named action (in the same way as would occur when activating an
  action specified in the ``.desktop`` file).

  The application ID (as above) is the first parameter.  The action name
  follows.

  Optionally, following the action name can be one parameter, in GVariant
  format, given as a single argument.  Make sure to use sufficient quoting.

EXAMPLES
--------

From the commandline
^^^^^^^^^^^^^^^^^^^^

Launching an application::

   gapplication launch org.example.fooview

Opening a file with an application::

   gapplication launch org.example.fooview ~/file.foo

Opening many files with an application::

   gapplication launch org.example.fooview ~/foos/*.foo

Invoking an action on an application::

   gapplication action org.example.fooview create

Invoking an action on an application, with an action::

   gapplication action org.example.fooview show-item '"item_id_828739"'

From the ``Exec`` lines of a ``.desktop`` file
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The commandline interface of ``gapplication`` was designed so that it could be
used directly from the ``Exec`` line of a ``.desktop`` file.

You might want to do this to allow for backwards compatibility with
implementations of the specification that do not understand how to do D-Bus
activation, without having to install a separate utility program.

Consider the following example::

   [Desktop Entry]
   Version=1.1
   Type=Application
   Name=Foo Viewer
   DBusActivatable=true
   MimeType=image/x-foo;
   Exec=gapplication launch org.example.fooview %F
   Actions=gallery;create;

   [Desktop Action gallery]
   Name=Browse Gallery
   Exec=gapplication action org.example.fooview gallery

   [Desktop Action create]
   Name=Create a new Foo!
   Exec=gapplication action org.example.fooview create

From a script
^^^^^^^^^^^^^

If installing an application that supports D-Bus activation you may still want
to put a file in ``/usr/bin`` so that your program can be started from a
terminal.

It is possible for this file to be a shell script.  The script can handle
arguments such as ``--help`` and ``--version`` directly.  It can also parse
other command line arguments and convert them to uses of ``gapplication`` to
activate the application, open files, or invoke actions.

Here is a simplified example, as may be installed in ``/usr/bin/fooview``::

   #!/bin/sh

   case "$1" in
     --help)
       echo "see ‘man fooview’ for more information"
       ;;

     --version)
       echo "fooview 1.2"
       ;;

     --gallery)
       gapplication action org.example.fooview gallery
       ;;

     --create)
       gapplication action org.example.fooview create
       ;;

     -*)
       echo "unrecognized commandline argument"
       exit 1
       ;;

     *)
       gapplication launch org.example.fooview "$@"
       ;;
   esac

SEE ALSO
--------

`Desktop Entry Specification <https://specifications.freedesktop.org/desktop-entry-spec/latest/>`_,
`gdbus(1) <man:gdbus(1)>`_,
`xdg-open(1) <man:xdg-open(1)>`_,
`desktop-file-validate(1) <man:desktop-file-validate(1)>`_