.. _gi-decompile-typelib(1):
.. meta::
   :copyright: Copyright 2008, 2010 Johan Dahlin
   :copyright: Copyright 2014 Robert Roth
   :copyright: Copyright 2015 Dieter Verfaillie
   :copyright: Copyright 2018 Tomasz Miąsko
   :copyright: Copyright 2018 Christoph Reiter
   :copyright: Copyright 2020 Jan Tojnar
   :license: LGPL-2.1-or-later
..
   This has to be duplicated from above to make it machine-readable by `reuse`:
   SPDX-FileCopyrightText: 2008, 2010 Johan Dahlin
   SPDX-FileCopyrightText: 2014 Robert Roth
   SPDX-FileCopyrightText: 2015 Dieter Verfaillie
   SPDX-FileCopyrightText: 2018 Tomasz Miąsko
   SPDX-FileCopyrightText: 2018 Christoph Reiter
   SPDX-FileCopyrightText: 2020 Jan Tojnar
   SPDX-License-Identifier: LGPL-2.1-or-later

=============
g-ir-generate
=============

-----------------
Typelib generator
-----------------

:Manual section: 1


SYNOPSIS
========

**g-ir-generate** [OPTION...] FILES...


DESCRIPTION
===========

g-ir-generate is an GIR generator, using the repository API. It generates GIR
files from a raw typelib or in a shared library (``--shlib``). The output will
be written to standard output unless the ``--output`` is specified.


OPTIONS
=======

--help
    Show help options

--shlib=FILENAME
    The shared library to read the symbols from.

--output=FILENAME
    Save the resulting output in FILENAME.

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
