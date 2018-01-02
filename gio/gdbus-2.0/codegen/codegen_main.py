# -*- Mode: Python -*-

# GDBus - GLib D-Bus Library
#
# Copyright (C) 2008-2011 Red Hat, Inc.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General
# Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
#
# Author: David Zeuthen <davidz@redhat.com>

import argparse
import sys
from os import path

from . import config
from . import dbustypes
from . import parser
from . import codegen
from . import codegen_docbook
from .utils import print_error, print_warning

def find_arg(arg_list, arg_name):
    for a in arg_list:
        if a.name == arg_name:
            return a
    return None

def find_method(iface, method):
    for m in iface.methods:
        if m.name == method:
            return m
    return None

def find_signal(iface, signal):
    for m in iface.signals:
        if m.name == signal:
            return m
    return None

def find_prop(iface, prop):
    for m in iface.properties:
        if m.name == prop:
            return m
    return None

def apply_annotation(iface_list, iface, method, signal, prop, arg, key, value):
    iface_obj = None
    for i in iface_list:
        if i.name == iface:
            iface_obj = i
            break

    if iface_obj == None:
        print_error('No interface "{}"'.format(iface))

    target_obj = None

    if method:
        method_obj = find_method(iface_obj, method)
        if method_obj == None:
            print_error('No method "{}" on interface "{}"'.format(method, iface))
        if arg:
            arg_obj = find_arg(method_obj.in_args, arg)
            if (arg_obj == None):
                arg_obj = find_arg(method_obj.out_args, arg)
                if (arg_obj == None):
                    print_error('No arg "{}" on method "{}" on interface "{}"'.format(arg, method, iface))
            target_obj = arg_obj
        else:
            target_obj = method_obj
    elif signal:
        signal_obj = find_signal(iface_obj, signal)
        if signal_obj == None:
            print_error('No signal "{}" on interface "{}"'.format(signal, iface))
        if arg:
            arg_obj = find_arg(signal_obj.args, arg)
            if (arg_obj == None):
                print_error('No arg "{}" on signal "{}" on interface "{}"'.format(arg, signal, iface))
            target_obj = arg_obj
        else:
            target_obj = signal_obj
    elif prop:
        prop_obj = find_prop(iface_obj, prop)
        if prop_obj == None:
            print_error('No property "{}" on interface "{}"'.format(prop, iface))
        target_obj = prop_obj
    else:
        target_obj = iface_obj
    target_obj.annotations.insert(0, dbustypes.Annotation(key, value))


def apply_annotations(iface_list, annotation_list):
    # apply annotations given on the command line
    for (what, key, value) in annotation_list:
        pos = what.find('::')
        if pos != -1:
            # signal
            iface = what[0:pos];
            signal = what[pos + 2:]
            pos = signal.find('[')
            if pos != -1:
                arg = signal[pos + 1:]
                signal = signal[0:pos]
                pos = arg.find(']')
                arg = arg[0:pos]
                apply_annotation(iface_list, iface, None, signal, None, arg, key, value)
            else:
                apply_annotation(iface_list, iface, None, signal, None, None, key, value)
        else:
            pos = what.find(':')
            if pos != -1:
                # property
                iface = what[0:pos];
                prop = what[pos + 1:]
                apply_annotation(iface_list, iface, None, None, prop, None, key, value)
            else:
                pos = what.find('()')
                if pos != -1:
                    # method
                    combined = what[0:pos]
                    pos = combined.rfind('.')
                    iface = combined[0:pos]
                    method = combined[pos + 1:]
                    pos = what.find('[')
                    if pos != -1:
                        arg = what[pos + 1:]
                        pos = arg.find(']')
                        arg = arg[0:pos]
                        apply_annotation(iface_list, iface, method, None, None, arg, key, value)
                    else:
                        apply_annotation(iface_list, iface, method, None, None, None, key, value)
                else:
                    # must be an interface
                    iface = what
                    apply_annotation(iface_list, iface, None, None, None, None, key, value)

def codegen_main():
    arg_parser = argparse.ArgumentParser(description='D-Bus code and documentation generator')
    arg_parser.add_argument('files', metavar='FILE', nargs='*',
                            help='D-Bus introspection XML file')
    arg_parser.add_argument('--xml-files', metavar='FILE', action='append', default=[],
                            help='D-Bus introspection XML file')
    arg_parser.add_argument('--interface-prefix', metavar='PREFIX', default='',
                            help='String to strip from D-Bus interface names for code and docs')
    arg_parser.add_argument('--c-namespace', metavar='NAMESPACE', default='',
                            help='The namespace to use for generated C code')
    arg_parser.add_argument('--c-generate-object-manager', action='store_true',
                            help='Generate a GDBusObjectManagerClient subclass when generating C code')
    arg_parser.add_argument('--generate-c-code', metavar='OUTFILES',
                            help='Generate C code in OUTFILES.[ch]')
    arg_parser.add_argument('--c-generate-autocleanup', choices=['none', 'objects', 'all'], default='objects',
                            help='Generate autocleanup support')
    arg_parser.add_argument('--generate-docbook', metavar='OUTFILES',
                            help='Generate Docbook in OUTFILES-org.Project.IFace.xml')
    arg_parser.add_argument('--annotate', nargs=3, action='append', metavar='WHAT KEY VALUE',
                            help='Add annotation (may be used several times)')
    arg_parser.add_argument('--output-directory', metavar='OUTDIR', default='',
                            help='Location to output generated files')
    args = arg_parser.parse_args();

    if len(args.xml_files) > 0:
        print_warning('The "--xml-files" option is deprecated; use positional arguments instead')

    all_ifaces = []
    for fname in args.files + args.xml_files:
        with open(fname, 'rb') as f:
            xml_data = f.read()
        parsed_ifaces = parser.parse_dbus_xml(xml_data)
        all_ifaces.extend(parsed_ifaces)

    if args.annotate != None:
        apply_annotations(all_ifaces, args.annotate)

    for i in all_ifaces:
        i.post_process(args.interface_prefix, args.c_namespace)

    outdir = args.output_directory

    docbook = args.generate_docbook
    docbook_gen = codegen_docbook.DocbookCodeGenerator(all_ifaces, docbook, outdir);
    if docbook:
        ret = docbook_gen.generate()

    c_code = args.generate_c_code
    if c_code:
        header_name = c_code + '.h'
        h = open(path.join(outdir, header_name), 'w')
        c = open(path.join(outdir, c_code + '.c'), 'w')
        gen = codegen.CodeGenerator(all_ifaces,
                                    args.c_namespace,
                                    args.interface_prefix,
                                    args.c_generate_object_manager,
                                    args.c_generate_autocleanup,
                                    docbook_gen,
                                    h, c,
                                    header_name)
        ret = gen.generate()
        h.close()
        c.close()

    sys.exit(0)

if __name__ == "__main__":
    codegen_main()
