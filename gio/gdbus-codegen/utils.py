# -*- Mode: Python -*-

# GDBus - GLib D-Bus Library
#
# Copyright (C) 2008-2011 Red Hat, Inc.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General
# Public License along with this library; if not, write to the
# Free Software Foundation, Inc., 59 Temple Place, Suite 330,
# Boston, MA 02111-1307, USA.
#
# Author: David Zeuthen <davidz@redhat.com>

def strip_dots(s):
    ret = ''
    force_upper = False
    for c in s:
        if c == '.':
            force_upper = True
        else:
            if force_upper:
                ret += c.upper()
                force_upper = False
            else:
                ret += c
    return ret

def dots_to_hyphens(s):
    return s.replace('.', '-')

def camel_case_to_uscore(s):
    ret = ''
    insert_uscore = False
    prev_was_lower = False
    for c in s:
        if c.isupper():
            if prev_was_lower:
                insert_uscore = True
            prev_was_lower = False
        else:
            prev_was_lower = True
        if insert_uscore:
            ret += '_'
        ret += c.lower()
        insert_uscore = False
    return ret

def is_ugly_case(s):
    if s and s.find('_') > 0:
        return True
    return False

def lookup_annotation(annotations, key):
    if annotations:
        for a in annotations:
            if a.key == key:
                return a.value
    return None

def lookup_docs(annotations):
    s = lookup_annotation(annotations, 'org.gtk.GDBus.DocString')
    if s == None:
        return ''
    else:
        return s

def lookup_since(annotations):
    s = lookup_annotation(annotations, 'org.gtk.GDBus.Since')
    if s == None:
        return ''
    else:
        return s

def lookup_brief_docs(annotations):
    s = lookup_annotation(annotations, 'org.gtk.GDBus.DocString.Short')
    if s == None:
        return ''
    else:
        return s
