# -*- Mode: Python -*-

import sys
import argparse

import config
import utils
import dbustypes
import parser
import codegen
import codegen_docbook

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
    for i in iface_list:
        if i.name == iface:
            iface_obj = i
            break

    if iface_obj == None:
        raise RuntimeError('No interface %s'%iface)

    target_obj = None

    if method:
        method_obj = find_method(iface_obj, method)
        if method_obj == None:
            raise RuntimeError('No method %s on interface %s'%(method, iface))
        if arg:
            arg_obj = find_arg(method_obj.in_args, arg)
            if (arg_obj == None):
                arg_obj = find_arg(method_obj.out_args, arg)
                if (arg_obj == None):
                    raise RuntimeError('No arg %s on method %s on interface %s'%(arg, method, iface))
            target_obj = arg_obj
        else:
            target_obj = method_obj
    elif signal:
        signal_obj = find_signal(iface_obj, signal)
        if signal_obj == None:
            raise RuntimeError('No signal %s on interface %s'%(signal, iface))
        if arg:
            arg_obj = find_arg(signal_obj.args, arg)
            if (arg_obj == None):
                raise RuntimeError('No arg %s on signal %s on interface %s'%(arg, signal, iface))
            target_obj = arg_obj
        else:
            target_obj = signal_obj
    elif prop:
        prop_obj = find_prop(iface_obj, prop)
        if prop_obj == None:
            raise RuntimeError('No property %s on interface %s'%(prop, iface))
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
    arg_parser = argparse.ArgumentParser(description='GDBus Code Generator')
    arg_parser.add_argument('xml_files', metavar='FILE', type=file, nargs='+',
                            help='D-Bus introspection XML file')
    arg_parser.add_argument('--interface-prefix', nargs='?', metavar='PREFIX', default='',
                            help='String to strip from D-Bus interface names for code and docs')
    arg_parser.add_argument('--c-namespace', nargs='?', metavar='NAMESPACE', default='',
                            help='The namespace to use for generated C code')
    arg_parser.add_argument('--generate-c-code', nargs='?', metavar='OUTFILES',
                            help='Generate C code in OUTFILES.[ch]')
    arg_parser.add_argument('--generate-docbook', nargs='?', metavar='OUTFILES',
                            help='Generate Docbook in OUTFILES-org.Project.IFace.xml')
    arg_parser.add_argument('--annotate', nargs=3, action='append', metavar=('WHAT', 'KEY', 'VALUE'),
                            help='Add annotation (may be used several times)')
    args = arg_parser.parse_args();

    all_ifaces = []
    for f in args.xml_files:
        xml_data = f.read()
        f.close()
        parsed_ifaces = parser.parse_dbus_xml(xml_data)
        all_ifaces.extend(parsed_ifaces)

    if args.annotate != None:
        apply_annotations(all_ifaces, args.annotate)

    for i in parsed_ifaces:
        i.post_process(args.interface_prefix, args.c_namespace)

    c_code = args.generate_c_code
    if c_code:
        h = file(c_code + '.h', 'w')
        c = file(c_code + '.c', 'w')
        gen = codegen.CodeGenerator(all_ifaces, args.c_namespace, args.interface_prefix, h, c);
        ret = gen.generate()

    docbook = args.generate_docbook
    if docbook:
        gen = codegen_docbook.DocbookCodeGenerator(all_ifaces, docbook);
        ret = gen.generate()

    sys.exit(0)

if __name__ == "__main__":
    codegen_main()
