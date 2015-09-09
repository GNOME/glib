#!/usr/bin/python
# vim: encoding=utf-8
#expand *.in files
#this script is only intended for building from git, not for building from the released tarball, which already includes all necessary files
import os
import sys
import re
import string
import subprocess
import optparse

def get_version(srcroot):
    ver = {}
    RE_VERSION = re.compile(r'^m4_define\(\[(glib_\w+)\],\s*\[(\d+)\]\)')
    with open(os.path.join(srcroot, 'configure.ac'), 'r') as ac:
        for i in ac:
            mo = RE_VERSION.search(i)
            if mo:
                ver[mo.group(1).upper()] = int(mo.group(2))
    ver['GLIB_BINARY_AGE'] = 100 * ver['GLIB_MINOR_VERSION'] + ver['GLIB_MICRO_VERSION']
    ver['GLIB_VERSION'] = '%d.%d.%d' % (ver['GLIB_MAJOR_VERSION'],
                                        ver['GLIB_MINOR_VERSION'],
                                        ver['GLIB_MICRO_VERSION'])
    ver['LT_RELEASE'] = '%d.%d' % (ver['GLIB_MAJOR_VERSION'], ver['GLIB_MINOR_VERSION'])
    ver['LT_CURRENT'] = 100 * ver['GLIB_MINOR_VERSION'] + ver['GLIB_MICRO_VERSION'] - ver['GLIB_INTERFACE_AGE']
    ver['LT_REVISION'] = ver['GLIB_INTERFACE_AGE']
    ver['LT_AGE'] = ver['GLIB_BINARY_AGE'] - ver['GLIB_INTERFACE_AGE']
    ver['LT_CURRENT_MINUS_AGE'] = ver['LT_CURRENT'] - ver['LT_AGE']
    return ver

def process_in(src, dest, vars):
    RE_VARS = re.compile(r'@(\w+?)@')
    with open(src, 'r') as s:
        with open(dest, 'w') as d:
            for i in s:
                i = RE_VARS.sub(lambda x: str(vars[x.group(1)]), i)
                d.write(i)

def get_srcroot():
    if not os.path.isabs(__file__):
        path = os.path.abspath(__file__)
    else:
        path = __file__
    dirname = os.path.dirname(path)
    return os.path.abspath(os.path.join(dirname, '..', '..'))

def process_include(src, dest, includes):
    RE_INCLUDE = re.compile(r'^\s*#include\s+"(.*)"')
    with open(src, 'r') as s:
        with open(dest, 'w') as d:
            for i in s:
                mo = RE_INCLUDE.search(i)
                if mo:
                    target = ''
                    for j in includes:
                        #print "searching in ", j
                        if mo.group(1) in os.listdir(j):
                            target = os.path.join(j, mo.group(1))
                            break
                    if not target:
                        raise Exception("Couldn't fine include file %s" % mo.group(1))
                    else:
                        with open(target, 'r') as t:
                            for inc in t.readlines():
                                d.write(inc)
                else:
                    d.write(i)

def generate_libgio_sourcefiles(srcroot, dest, stype):
    vars = read_vars_from_AM(os.path.join(srcroot, 'gio', 'Makefile.am'),
                             vars = {'top_srcdir': srcroot},
                             conds = {'OS_WIN32': True},
                             filters = ['libgio_2_0_la_SOURCES', 'win32_more_sources_for_vcproj'])

    files = vars['libgio_2_0_la_SOURCES'].split() + \
            vars['win32_more_sources_for_vcproj'].split()

    sources = [i for i in files \
                    if i != 'gdesktopappinfo.c' and \
                       not (i.startswith('gunix') and i.endswith('.c')) \
                       and i.endswith('.c') ]
    if stype == '9':
       with open(dest, 'w') as d:
           for i in sources:
               d.write('\t\t\t<File RelativePath="..\\..\\..\\gio\\' + i.replace('/', '\\') + '"/>\n')
    elif stype == '10':
       with open(dest, 'w') as d:
           for i in sources:
               d.write('\t\t\t<ClCompile Include="..\\..\\..\\gio\\' + i.replace('/', '\\') + '"/>\n')
    elif stype == '10f':
       with open(dest, 'w') as d:
           for i in sources:
               d.write('\t\t\t<ClCompile Include="..\\..\\..\\gio\\' + i.replace('/', '\\') + '"><Filter>Source Files</Filter></ClCompile>\n')
    else:
       raise Exception("Must specify project type (9, 10 or 10f)")

def generate_libgio_enumtypes(srcroot, perl):
    vars = read_vars_from_AM(os.path.join(srcroot, 'gio', 'Makefile.am'),
                             vars = {'top_srcdir': srcroot},
                             conds = {'OS_WIN32': True},
                             filters = ['gio_headers'])
    cwd = os.getcwd()
    os.chdir(os.path.join(srcroot, 'gio'))
    for suffix in ['.c', '.h']:
        cmd = [perl, os.path.join(srcroot, 'gobject', 'glib-mkenums'),
               '--template', 'gioenumtypes' + suffix + '.template'] + vars['gio_headers'].split()
        with open('gioenumtypes' + suffix, 'w') as d:
            subprocess.Popen(cmd, stdout = d).communicate()
    os.chdir(cwd)
def generate_libglib_sourcefiles(srcroot, dest, stype):
    vars = read_vars_from_AM(os.path.join(srcroot, 'glib', 'Makefile.am'),
                             vars = {'top_srcdir': srcroot},
                             conds = {'OS_WIN32': True,
                                      'ENABLE_REGEX': True},
                             filters = ['libglib_2_0_la_SOURCES'])

    files = vars['libglib_2_0_la_SOURCES'].split()

    sources = [i for i in files \
                    if not (i.endswith('-gcc.c') or i.endswith('-unix.c')) \
                       and i.endswith('.c') ]
    if stype == '9':
        with open(dest, 'w') as d:
            for i in sources:
                d.write('\t\t\t<File RelativePath="..\\..\\..\\glib\\' + i.replace('/', '\\') + '"/>\n')
    elif stype == '10':
        with open(dest, 'w') as d:
            for i in sources:
                d.write('\t\t\t<ClCompile Include="..\\..\\..\\glib\\' + i.replace('/', '\\') + '"/>\n')
    elif stype == '10f':
        with open(dest, 'w') as d:
            for i in sources:
                d.write('\t\t\t<ClCompile Include="..\\..\\..\\glib\\' + i.replace('/', '\\') + '"><Filter>Source Files</Filter></ClCompile>\n')
    else:
        raise Exception("Must specify project type (9, 10 or 10f)")

def generate_libgobject_sourcefiles(srcroot, dest, stype):
    vars = read_vars_from_AM(os.path.join(srcroot, 'gobject', 'Makefile.am'),
                             vars = {'top_srcdir': srcroot},
                             conds = {'OS_WIN32': True},
                             filters = ['libgobject_2_0_la_SOURCES'])

    files = vars['libgobject_2_0_la_SOURCES'].split()

    sources = [i for i in files if i.endswith('.c') ]
    if stype == '9':
        with open(dest, 'w') as d:
            for i in sources:
                d.write('\t\t\t<File RelativePath="..\\..\\..\\gobject\\' + i.replace('/', '\\') + '"/>\n')
    elif stype == '10':
        with open(dest, 'w') as d:
            for i in sources:
                d.write('\t\t\t<ClCompile Include="..\\..\\..\\gobject\\' + i.replace('/', '\\') + '"/>\n')
    elif stype == '10f':
        with open(dest, 'w') as d:
            for i in sources:
                d.write('\t\t\t<ClCompile Include="..\\..\\..\\gobject\\' + i.replace('/', '\\') + '"><Filter>Source Files</Filter></ClCompile>\n')
    else:
        raise Exception("Must specify project type (9, 10 or 10f)")

def read_vars_from_AM(path, vars = {}, conds = {}, filters = None):
    '''
    path: path to the Makefile.am
    vars: predefined variables
    conds: condition variables for Makefile
    filters: if None, all variables defined are returned,
             otherwise, it is a list contains that variables should be returned
    '''
    cur_vars = vars.copy()
    RE_AM_VAR_REF = re.compile(r'\$\((\w+?)\)')
    RE_AM_VAR = re.compile(r'^\s*(\w+)\s*=(.*)$')
    RE_AM_INCLUDE = re.compile(r'^\s*include\s+(\w+)')
    RE_AM_CONTINUING = re.compile(r'\\\s*$')
    RE_AM_IF = re.compile(r'^\s*if\s+(\w+)')
    RE_AM_ELSE = re.compile(r'^\s*else')
    RE_AM_ENDIF = re.compile(r'^\s*endif')
    def am_eval(cont):
        return RE_AM_VAR_REF.sub(lambda x: cur_vars.get(x.group(1), ''), cont)
    with open(path, 'r') as f:
        contents = f.readlines()
    #combine continuing lines
    i = 0
    ncont = []
    while i < len(contents):
        line = contents[i]
        if RE_AM_CONTINUING.search(line):
            line = RE_AM_CONTINUING.sub('', line)
            j = i + 1
            while j < len(contents) and RE_AM_CONTINUING.search(contents[j]):
                line += RE_AM_CONTINUING.sub('', contents[j])
                j += 1
            else:
                if j < len(contents):
                    line += contents[j]
            i = j
        else:
            i += 1
        ncont.append(line)

    #include, var define, var evaluation
    i = -1
    skip = False
    oldskip = []
    while i < len(ncont) - 1:
        i += 1
        line = ncont[i]
        mo = RE_AM_IF.search(line)
        if mo:
            oldskip.append(skip)
            skip = False if mo.group(1) in conds and conds[mo.group(1)] \
                         else True
            continue
        mo = RE_AM_ELSE.search(line)
        if mo:
            skip = not skip
            continue
        mo = RE_AM_ENDIF.search(line)
        if mo:
            skip = oldskip.pop()
            continue
        if not skip:
            mo = RE_AM_INCLUDE.search(line)
            if mo:
                cur_vars.update(read_vars_from_AM(am_eval(mo.group(1)), cur_vars, conds, None))
                continue
            mo = RE_AM_VAR.search(line)
            if mo:
                cur_vars[mo.group(1)] = am_eval(mo.group(2).strip())
                continue

    #filter:
    if filters != None:
        ret = {}
        for i in filters:
            ret[i] = cur_vars.get(i, '')
        return ret
    else:
        return cur_vars

def main(argv):
    parser = optparse.OptionParser()
    parser.add_option('-p', '--perl', dest='perl', metavar='PATH', default='C:\\Perl\\bin\\perl.exe', action='store', help='path to the perl interpretor (default: C:\\Perl\\bin\\perl.exe)')
    opt, args = parser.parse_args(argv)
    srcroot = get_srcroot()
    #print 'srcroot', srcroot
    ver = get_version(srcroot)
    #print 'ver', ver
    config_vars = ver.copy()
    config_vars['GETTEXT_PACKAGE'] = 'Glib'
    process_in(os.path.join(srcroot, 'config.h.win32.in'),
               os.path.join(srcroot, 'config.h'),
               config_vars)
    glibconfig_vars = ver.copy()
    glibconfig_vars['GLIB_WIN32_STATIC_COMPILATION_DEFINE'] = ''
    process_in(os.path.join(srcroot, 'glib', 'glibconfig.h.win32.in'),
               os.path.join(srcroot, 'glib', 'glibconfig.h'),
               glibconfig_vars)

    for submodule in ['glib', 'gobject', 'gthread', 'gmodule', 'gio']:
        process_in(os.path.join(srcroot, submodule, submodule + '.rc.in'),
                   os.path.join(srcroot, submodule, submodule + '.rc'),
                   ver)

    #------------ submodule gobject -------------------
    generate_libglib_sourcefiles(srcroot,
                                os.path.join(srcroot, 'build', 'win32', 'libglib.sourcefiles'), '9')
    generate_libglib_sourcefiles(srcroot,
                                os.path.join(srcroot, 'build', 'win32', 'libglib.vs10.sourcefiles'), '10')
    generate_libglib_sourcefiles(srcroot,
                                os.path.join(srcroot, 'build', 'win32', 'libglib.vs10.sourcefiles.filters'), '10f')
    process_include(os.path.join(srcroot, 'build', 'win32', 'vs9', 'glib.vcprojin'),
                    os.path.join(srcroot, 'build', 'win32', 'vs9', 'glib.vcproj'),
                    includes = [os.path.join(srcroot, 'build', 'win32')])
    process_include(os.path.join(srcroot, 'build', 'win32', 'vs10', 'glib.vcxprojin'),
                    os.path.join(srcroot, 'build', 'win32', 'vs10', 'glib.vcxproj'),
                    includes = [os.path.join(srcroot, 'build', 'win32')])
    process_include(os.path.join(srcroot, 'build', 'win32', 'vs10', 'glib.vcxproj.filtersin'),
                    os.path.join(srcroot, 'build', 'win32', 'vs10', 'glib.vcxproj.filters'),
                    includes = [os.path.join(srcroot, 'build', 'win32')])
    os.unlink(os.path.join(srcroot, 'build', 'win32', 'libglib.sourcefiles'))
    os.unlink(os.path.join(srcroot, 'build', 'win32', 'libglib.vs10.sourcefiles'))
    os.unlink(os.path.join(srcroot, 'build', 'win32', 'libglib.vs10.sourcefiles.filters'))
    with open(os.path.join(srcroot, 'glib', 'gspawn-win32-helper-console.c'), 'w') as c:
        c.write('#define HELPER_CONSOLE\n')
        c.write('#include "gspawn-win32-helper.c"\n')
    with open(os.path.join(srcroot, 'glib', 'gspawn-win64-helper-console.c'), 'w') as c:
        c.write('#define HELPER_CONSOLE\n')
        c.write('#include "gspawn-win32-helper.c"\n')
    with open(os.path.join(srcroot, 'glib', 'gspawn-win64-helper.c'), 'w') as c:
        c.write('#include "gspawn-win32-helper.c"\n')
    #------------ end of submodule glib -------------------

    #------------ submodule gobject -------------------
    mkenums_vars = ver.copy()
    mkenums_vars.update({'PERL_PATH': opt.perl})
    process_in(os.path.join(srcroot, 'gobject', 'glib-mkenums.in'),
               os.path.join(srcroot, 'gobject', 'glib-mkenums'),
               mkenums_vars)

    #gmarshal.strings
    cwd = os.getcwd()
    os.chdir(os.path.join(srcroot, 'gobject'))
    with open(os.path.join(srcroot, 'gobject', 'gmarshal.strings'), 'w') as d:
        with open(os.path.join(srcroot, 'gobject', 'gmarshal.list'), 'r') as s:
            for i in s:
                if i[0] not in string.ascii_uppercase: #^[A-Z]
                    continue
                line = '"g_cclosure_marshal_' # s/^/"g_cclosure_marshal_/
                for c in i:
                    if c == ':':
                        line += '__'          # s/:/__
                    elif c == ',':
                        line += '_'           # s/,/_
                    elif c not in '\r\n':
                        line += c
                d.write(line + '",\n')
        #subprocess.Popen([opt.perl, 'marshal-genstrings.pl'], stdout=d).communicate()
    os.chdir(cwd)

    generate_libgobject_sourcefiles(srcroot,
                                os.path.join(srcroot, 'build', 'win32', 'libgobject.sourcefiles'), '9')
    generate_libgobject_sourcefiles(srcroot,
                                os.path.join(srcroot, 'build', 'win32', 'libgobject.vs10.sourcefiles'), '10')
    generate_libgobject_sourcefiles(srcroot,
                                os.path.join(srcroot, 'build', 'win32', 'libgobject.vs10.sourcefiles.filters'), '10f')
    process_include(os.path.join(srcroot, 'build', 'win32', 'vs9', 'gobject.vcprojin'),
                    os.path.join(srcroot, 'build', 'win32', 'vs9', 'gobject.vcproj'),
                    includes = [os.path.join(srcroot, 'build', 'win32')])
    process_include(os.path.join(srcroot, 'build', 'win32', 'vs10', 'gobject.vcxprojin'),
                    os.path.join(srcroot, 'build', 'win32', 'vs10', 'gobject.vcxproj'),
                    includes = [os.path.join(srcroot, 'build', 'win32')])
    process_include(os.path.join(srcroot, 'build', 'win32', 'vs10', 'gobject.vcxproj.filtersin'),
                    os.path.join(srcroot, 'build', 'win32', 'vs10', 'gobject.vcxproj.filters'),
                    includes = [os.path.join(srcroot, 'build', 'win32')])
    os.unlink(os.path.join(srcroot, 'build', 'win32', 'libgobject.sourcefiles'))
    os.unlink(os.path.join(srcroot, 'build', 'win32', 'libgobject.vs10.sourcefiles'))
    os.unlink(os.path.join(srcroot, 'build', 'win32', 'libgobject.vs10.sourcefiles.filters'))
    #------------ end of submodule gobject -------------------

    #------------ submodule gio -------------------
    #depends on glib-mkenums
    generate_libgio_sourcefiles(srcroot,
                                os.path.join(srcroot, 'build', 'win32', 'libgio.sourcefiles'), '9')
    generate_libgio_sourcefiles(srcroot,
                                os.path.join(srcroot, 'build', 'win32', 'libgio.vs10.sourcefiles'), '10')
    generate_libgio_sourcefiles(srcroot,
                                os.path.join(srcroot, 'build', 'win32', 'libgio.vs10.sourcefiles.filters'), '10f')
    process_include(os.path.join(srcroot, 'build', 'win32', 'vs9', 'gio.vcprojin'),
                    os.path.join(srcroot, 'build', 'win32', 'vs9', 'gio.vcproj'),
                    includes = [os.path.join(srcroot, 'build', 'win32')])
    process_include(os.path.join(srcroot, 'build', 'win32', 'vs10', 'gio.vcxprojin'),
                    os.path.join(srcroot, 'build', 'win32', 'vs10', 'gio.vcxproj'),
                    includes = [os.path.join(srcroot, 'build', 'win32')])
    process_include(os.path.join(srcroot, 'build', 'win32', 'vs10', 'gio.vcxproj.filtersin'),
                    os.path.join(srcroot, 'build', 'win32', 'vs10', 'gio.vcxproj.filters'),
                    includes = [os.path.join(srcroot, 'build', 'win32')])
    os.unlink(os.path.join(srcroot, 'build', 'win32', 'libgio.sourcefiles'))
    os.unlink(os.path.join(srcroot, 'build', 'win32', 'libgio.vs10.sourcefiles'))
    os.unlink(os.path.join(srcroot, 'build', 'win32', 'libgio.vs10.sourcefiles.filters'))
    generate_libgio_enumtypes(srcroot, opt.perl)
    #------------ end of submodule gio -------------------

    #------------ submodule gmodule -------------------
    #------------ end of submodule gmodule -------------------
    return 0

if __name__ == '__main__':
    sys.exit(main(sys.argv))
