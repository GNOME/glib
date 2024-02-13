.. _gi-compile-repository(1):
.. meta::
   :copyright: Copyright 2010 Johan Dahlin
   :copyright: Copyright 2015 Ben Boeckel
   :copyright: Copyright 2013, 2015 Dieter Verfaillie
   :copyright: Copyright 2018 Emmanuele Bassi
   :copyright: Copyright 2018 Tomasz Miąsko
   :copyright: Copyright 2018 Christoph Reiter
   :copyright: Copyright 2020 Jan Tojnar
   :license: LGPL-2.1-or-later
..
   This has to be duplicated from above to make it machine-readable by `reuse`:
   SPDX-FileCopyrightText: 2010 Johan Dahlin
   SPDX-FileCopyrightText: 2015 Ben Boeckel
   SPDX-FileCopyrightText: 2013, 2015 Dieter Verfaillie
   SPDX-FileCopyrightText: 2018 Emmanuele Bassi
   SPDX-FileCopyrightText: 2018 Tomasz Miąsko
   SPDX-FileCopyrightText: 2018 Christoph Reiter
   SPDX-FileCopyrightText: 2020 Jan Tojnar
   SPDX-License-Identifier: LGPL-2.1-or-later

=============
g-ir-compiler
=============

----------------
Typelib compiler
----------------

:Manual section: 1


SYNOPSIS
========

**g-ir-compiler** [OPTION...] GIRFILE


DESCRIPTION
===========

g-ir-compiler converts one or more GIR files into one or more typelib. The
output will be written to standard output unless the ``--output`` is
specified.


OPTIONS
=======

--help
    Show help options

--output=FILENAME
    Save the resulting output in FILENAME.

--verbose
    Show verbose messages

--debug
    Show debug messages

--includedir=DIRECTORY
    Adds a directory which will be used to find includes inside the GIR format.

--module=MODULE
    FIXME

--shared-library=FILENAME
    Specifies the shared library where the symbols in the typelib can be
    found. The name of the library should not contain the ending shared
    library suffix.

--version
    Show program's version number and exit


BUGS
====

Report bugs at https://gitlab.gnome.org/GNOME/gobject-introspection/issues


HOMEPAGE and CONTACT
====================

https://gi.readthedocs.io/


AUTHORS
=======

Mattias Clasen
