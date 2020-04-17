#!/usr/bin/env python3

# Copyright © 2011 Red Hat, Inc
# Copyright © 2020 Xavier Claessens
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
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, see <http://www.gnu.org/licenses/>.
#
# Author: Alexander Larsson <alexl@redhat.com>
#         Xavier Cleassens <xclaesse@gmail.com>

import argparse, sys, os, textwrap, struct, posixpath
import xml.etree.ElementTree as ET

gconstructor_code = '''
@GCONSTRUCTOR_CODE@
'''

class GResourceParserException(Exception):
    pass

def string_to_bool(s):
    return s in ["true", "t", "yes", "y", "1"]

MAX_UINT32 = 0xffffffff

def uint32(u):
    return u & MAX_UINT32

def djb_hash(key):
    hash_value = 5381;
    for c in key.encode():
        hash_value = uint32(uint32(hash_value * 33) + c)
    return hash_value

class Chunk:
    def __init__(self, offset, alignment=0):
        self.data = bytearray()
        self.offset = offset
        self.padding = 0
        while alignment > 0 and (self.offset + self.padding) % alignment != 0:
            self.padding += 1
        self.pack_zeros(self.padding)

    def start(self):
        return self.offset + self.padding

    def end(self):
        return self.offset + len(self.data)

    def size(self):
        return len(self.data)

    def merge(self, other):
        self.data.extend(other.data)

    def pack_zeros(self, n):
        self.data.extend((0).to_bytes(1, byteorder='little') * n)

    def pack_uint32(self, u):
        self.data.extend(u.to_bytes(4, byteorder='little'))

    def pack(self, sig, *args):
        self.data.extend(struct.pack(sig, *args))

class TableItem:
    def __init__(self, key):
        self.hash_value = djb_hash(key)
        self.key = key
        self.filename = None
        self.data = None
        self.flags = 0
        self.parent = None
        self.children = []
        self.assigned_index = 0

class GResourceCompiler:
    def __init__(self, internal, sourcedirs, manual_register, external_data, c_name):
        self.linkage = 'G_GNUC_INTERNAL' if internal else 'extern'
        self.sourcedirs = sourcedirs
        self.manual_register = manual_register
        self.external_data = external_data
        self.c_name = c_name
        self.table = {}

    def find_file(self, filename):
        if os.path.isabs(filename):
            return filename
        for d in self.sourcedirs:
            path = os.path.join(d, filename)
            if os.path.exists(path):
                return path
        return None

    def set_c_name(self):
        if self.c_name:
            return
        base = os.path.basename(self.filename)
        base = base[:base.find('.')]
        self.c_name = ''
        for c in base:
            ok = c.isalpha() or c == '_' or (self.c_name and c.isnumeric())
            self.c_name += c if ok else '_'

    def parse(self, filename, collect_data):
        self.collect_data = collect_data
        self.filename = filename
        self.set_c_name()
        for gresource_elem in ET.parse(filename).getroot():
            if gresource_elem.tag != 'gresource':
                m = 'Element {} not allowed inside gresources'
                raise GResourceParserException(m.format(gresource_elem.tag))
            self.prefix = gresource_elem.attrib.get('prefix')
            for file_elem in gresource_elem:
                if file_elem.tag != 'file':
                    m = 'Element {} not allowed inside gresource'
                    raise GResourceParserException(m.format(file_elem.tag))
                self.parse_file_elem(file_elem)

    def parse_file_elem(self, file_elem):
        alias = file_elem.attrib.get('alias')
        compressed = string_to_bool(file_elem.attrib.get('compressed', 'false'))
        preproc_options = string_to_bool(file_elem.attrib.get('preprocess', 'false'))

        file = file_elem.text
        key = alias if alias else file
        if self.prefix:
            key = posixpath.join('/', self.prefix, key)
        else:
            key = posixpath.join('/', key)

        item = self.table.get(key)
        if not item:
            item = TableItem(key)
            self.table[key] = item

        if item.filename:
            m = 'File {} appears multiple times in the resource'
            raise GResourceParserException(m.format(key))

        if self.sourcedirs:
            real_file = self.find_file(file)
            if not real_file and self.collect_data:
                m = 'Failed to locate {!r} in any source directory'
                raise GResourceParserException(m.format(file))
        else:
            if not os.path.exists(file) and self.collect_data:
                m = 'Failed to locate {!r} in current directory'
                raise GResourceParserException(m.format(file))
            real_file = file

        item.filename = real_file

        if not self.collect_data:
            return

        if preproc_options:
            raise GResourceException('FIXME: Preprocess not implemented yet')

        with open(real_file, 'rb') as f:
            data = f.read()

        if compressed:
            raise GResourceException('FIXME: Compressed not implemented yet')

        item.data = data
        if item.parent:
            return

        # Create parents
        while True:
            key = key[:key.rfind('/') + 1]
            parent = self.table.get(key)
            if parent:
                item.parent = parent
                parent.children.append(item)
                break
            parent = TableItem(key)
            parent.children.append(item)
            self.table[key] = parent
            item.parent = parent
            item = parent
            if len(key) == 1:
                break

    def serialise(self):
        self.offset = 0
        chunk = Chunk(self.offset)
        chunk.pack('<IIII', 1918981703, 1953390953, 0, 0)
        self.offset = chunk.end() + 4 * 2
        header, table = self.serialise_table()
        chunk.pack('<II', header.start(), header.end())
        chunk.merge(header)
        chunk.merge(table)
        self.data = chunk.data

    def serialise_table(self):
        self.buckets = [[] for k in self.table]
        for k, v in self.table.items():
            i = v.hash_value % len(self.buckets)
            self.buckets[i].append(v)

        bloom_shift = 5
        n_bloom_words = 0
        n_buckets = len(self.buckets)
        n_items = len(self.table)

        header = Chunk(self.offset, alignment=4)
        header.pack('<II', bloom_shift << 27 | n_bloom_words, n_buckets)
        header.pack_zeros(4 * n_bloom_words)

        index = 0
        for bucket in self.buckets:
            header.pack_uint32(index)
            for item in bucket:
                item.assigned_index = index
                index += 1

        size_hash_item = 24
        self.offset = header.end() + size_hash_item * n_items
        table = Chunk(self.offset)
        for bucket in self.buckets:
            for item in bucket:
                key_chunk = self.serialise_key(item)
                if item.data:
                    value_type, value_chunk = self.serialise_data(item)
                else:
                    value_type, value_chunk = self.serialise_children(item)
                table.merge(key_chunk)
                table.merge(value_chunk)

                # This is the 24 bytes of size_hash_item
                header.pack('<IIIHBBII',
                            item.hash_value,
                            item.parent.assigned_index if item.parent else MAX_UINT32,
                            key_chunk.start(),
                            key_chunk.size(),
                            ord(value_type),
                            0,
                            value_chunk.start(),
                            value_chunk.end())
        return header, table

    def serialise_key(self, item):
        chunk = Chunk(self.offset)
        basename = item.key
        if item.parent:
            basename = basename[len(item.parent.key):]
        chunk.data.extend(basename.encode())
        self.offset = chunk.end()
        return chunk

    def serialise_data(self, item):
        chunk = Chunk(self.offset, alignment=8)
        chunk.pack('II', len(item.data), item.flags)
        chunk.data.extend(item.data)
        self.offset = chunk.end()
        tmp = Chunk(self.offset, alignment=4)
        tmp.data.extend('(uuay)'.encode())
        chunk.merge(tmp)
        self.offset = chunk.end()
        return 'v', chunk

    def serialise_children(self, item):
        chunk = Chunk(self.offset, alignment=4)
        for child in item.children:
            chunk.pack_uint32(child.assigned_index)
        self.offset = chunk.end()
        return 'L', chunk

    def generate_source(self, target):
        c_name_no_underscores = self.c_name
        while(c_name_no_underscores[0] == '_'):
            c_name_no_underscores = c_name_no_underscores[1:]
        self.serialise()
        data_size = len(self.data)

        with open(target, 'w') as f:
            f.write(textwrap.dedent('''\
                #include <gio/gio.h>

                #if defined (__ELF__) && ( __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 6))
                # define SECTION __attribute__ ((section (\".gresource.{}\"), aligned (8)))
                #else
                # define SECTION
                #endif

                '''.format(c_name_no_underscores)))

            if self.external_data:
                f.write("extern const SECTION union {{ const guint8 data[{}]; const double alignment; void * const ptr;}}  {}_resource_data;\n".format(data_size, self.c_name))
            else:
                # For Visual Studio builds: Avoid surpassing the 65535-character limit for a string, GitLab issue #1580
                f.write("#ifdef _MSC_VER\n")
                f.write("static const SECTION union {{ const guint8 data[{}]; const double alignment; void * const ptr;}}  {}_resource_data = {{ {{\n".format(data_size + 1, self.c_name))

                for i, c in enumerate(self.data):
                    if i % 16 == 0:
                        f.write('  ')
                    f.write('{:04o}'.format(c))
                    if i != data_size - 1:
                        f.write(', ')
                    if i % 16 == 15 or i == data_size - 1:
                        f.write('\n')
                f.write("} };\n")

                # For other compilers, use the long string approach
                f.write("#else /* _MSC_VER */\n")
                f.write('static const SECTION union {{ const guint8 data[{}]; const double alignment; void * const ptr;}}  {}_resource_data = {{\n  "'.format(data_size + 1, self.c_name))

                for i, c in enumerate(self.data):
                    f.write('\\{:03o}'.format(c))
                    if i % 16 == 15:
                        f.write('"\n  "')
                f.write('" };\n')
                f.write("#endif /* !_MSC_VER */\n");


            f.write(textwrap.dedent('''
                static GStaticResource static_resource = {{ {0}_resource_data.data, sizeof ({0}_resource_data.data){2}, NULL, NULL, NULL }};
                {1} GResource *{0}_get_resource (void);
                GResource *{0}_get_resource (void)
                {{
                  return g_static_resource_get_resource (&static_resource);
                }}'''.format(self.c_name, self.linkage, '' if self.external_data else ' - 1 /* nul terminator */')))

            if self.manual_register:
                f.write(textwrap.dedent('''\
                    {1} void {0}_unregister_resource (void);
                    void {0}_unregister_resource (void)
                    {
                      g_static_resource_fini (&static_resource);
                    }

                    {1} void {0}_register_resource (void);
                    void {0}_register_resource (void)
                    {
                      g_static_resource_init (&static_resource);
                    }
                    '''.format(self.c_name, self.linkage)))
            else:
                f.write(gconstructor_code)
                f.write(textwrap.dedent('''\
                    #ifdef G_HAS_CONSTRUCTORS

                    #ifdef G_DEFINE_CONSTRUCTOR_NEEDS_PRAGMA
                    #pragma G_DEFINE_CONSTRUCTOR_PRAGMA_ARGS(resource_constructor)
                    #endif
                    G_DEFINE_CONSTRUCTOR(resource_constructor)
                    #ifdef G_DEFINE_DESTRUCTOR_NEEDS_PRAGMA
                    #pragma G_DEFINE_DESTRUCTOR_PRAGMA_ARGS(resource_destructor)
                    #endif
                    G_DEFINE_DESTRUCTOR(resource_destructor)

                    #else
                    #warning "Constructor not supported on this compiler, linking in resources will not work"
                    #endif

                    static void resource_constructor (void)
                    {
                      g_static_resource_init (&static_resource);
                    }

                    static void resource_destructor (void)
                    {
                      g_static_resource_fini (&static_resource);
                    }
                    '''))

    def generate_header(self, target):
        with open(target, 'w') as f:
            f.write(textwrap.dedent('''\
                #ifndef __RESOURCE_{0}_H__
                #define __RESOURCE_{0}_H__

                #include <gio/gio.h>

                {1} GResource *{0}_get_resource (void);
                '''.format(self.c_name, self.linkage)))

            if self.manual_register:
                f.write(textwrap.dedent('''\
                    {1} void {0}_register_resource (void);
                    {1} void {0}_unregister_resource (void);
                    '''.format(self.c_name, self.linkage)))
            f.write('#endif')

    def generate(self, target):
        base = os.path.basename(target)
        ext = os.path.splitext(base)[1][1:]
        if ext in ["c", "cc", "cpp", "cxx", "c++"]:
            self.generate_source(target)
        elif ext in ["h", "hh", "hpp", "hxx", "h++"]:
            self.generate_header(target)
        else:
            raise GResourceParserException('Unknown file format: {}'.format(base))

    def generate_dependencies(self):
        for item in self.table.values():
            print(item.filename)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Compile a resource specification into a resource file.')
    parser.add_argument('--generate', action='store_true',
                        help='Generate output in the format selected for by the target filename extension')
    parser.add_argument('--target',
                        help='Name of the output file')
    parser.add_argument('--internal', action='store_true',
                        help="Don't export functions; declare them G_GNUC_INTERNAL")
    parser.add_argument('--sourcedir', default=[], action='append',
                        help="The directories to load files referenced in FILE from (default: current directory")
    parser.add_argument('--manual-register', action='store_true',
                        help="Don't automatically create and register resource")
    parser.add_argument('--external-data', action='store_true',
                        help="Don't embed resource data in the C file; assume it's linked externally instead")
    parser.add_argument('--c-name',
                        help="C identifier name used for the generated source code")
    parser.add_argument('--generate-dependencies', action='store_true',
                        help="Generate dependency list")
    parser.add_argument('file')
    args = parser.parse_args()

    compiler = GResourceCompiler(args.internal,
                                 args.sourcedir,
                                 args.manual_register,
                                 args.external_data,
                                 args.c_name)
    compiler.parse(args.file, not args.generate_dependencies)
    if args.generate_dependencies:
        compiler.generate_dependencies()
    else:
        compiler.generate(args.target)
