# -*- Mode: Python -*-
# GObject-Introspection - a framework for introspecting GObject libraries
# Copyright (C) 2010  Red Hat, Inc.
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
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.
#

from io import StringIO

from . import girast
from .codegen import CCodeGenerator

DEFAULT_C_VALUES = {girast.TYPE_ANY: 'NULL',
                    girast.TYPE_STRING: '""',
                    girast.TYPE_FILENAME: '""',
                    girast.TYPE_GTYPE: 'g_object_get_type ()'}


def get_default_for_typeval(typeval):
    default = DEFAULT_C_VALUES.get(typeval)
    if default:
        return default
    return "0"


def uscore_from_type(typeval):
    if typeval.target_fundamental:
        return typeval.target_fundamental.replace(' ', '_')
    elif typeval.target_giname:
        return typeval.target_giname.replace('.', '').lower()
    else:
        assert False, typeval


class EverythingCodeGenerator(object):

    def __init__(self,
                 out_h_filename,
                 out_c_filename,
                 function_decoration,
                 include_first_header,
                 include_last_header,
                 include_first_src,
                 include_last_src):
        self.namespace = girast.Namespace('Everything', '1.0')
        self.gen = CCodeGenerator(self.namespace,
                                  out_h_filename,
                                  out_c_filename,
                                  function_decoration,
                                  include_first_header,
                                  include_last_header,
                                  include_first_src,
                                  include_last_src)

    def write(self):
        types = [girast.TYPE_ANY]
        types.extend(girast.INTROSPECTABLE_BASIC)

        func = girast.Function('nullfunc',
                            girast.Return(girast.TYPE_NONE, transfer=girast.PARAM_TRANSFER_NONE),
                            [], False, self.gen.gen_symbol('nullfunc'))
        self.namespace.append(func)
        body = "  return;\n"
        self.gen.set_function_body(func, body)

        # First pass, generate constant returns
        prefix = 'const return '
        for typeval in types:
            name = prefix + uscore_from_type(typeval)
            sym = self.gen.gen_symbol(name)
            func = girast.Function(name,
                                girast.Return(typeval, transfer=girast.PARAM_TRANSFER_NONE),
                                [], False, sym)
            self.namespace.append(func)
            default = get_default_for_typeval(typeval)
            body = "  return %s;\n" % (default, )
            self.gen.set_function_body(func, body)

        # Void return, one parameter
        prefix = 'oneparam '
        for typeval in types:
            if typeval is girast.TYPE_NONE:
                continue
            name = prefix + uscore_from_type(typeval)
            sym = self.gen.gen_symbol(name)
            func = girast.Function(name,
                                girast.Return(girast.TYPE_NONE, transfer=girast.PARAM_TRANSFER_NONE),
                                [girast.Parameter('arg0', typeval, transfer=girast.PARAM_TRANSFER_NONE,
                                               direction=girast.PARAM_DIRECTION_IN)], False, sym)
            self.namespace.append(func)
            self.gen.set_function_body(func, "  return;\n")

        # Void return, one (out) parameter
        prefix = 'one_outparam '
        for typeval in types:
            if typeval is girast.TYPE_NONE:
                continue
            name = prefix + uscore_from_type(typeval)
            sym = self.gen.gen_symbol(name)
            func = girast.Function(name,
                                girast.Return(girast.TYPE_NONE, transfer=girast.PARAM_TRANSFER_NONE),
                                [girast.Parameter('arg0', typeval, transfer=girast.PARAM_TRANSFER_NONE,
                                               direction=girast.PARAM_DIRECTION_OUT)], False, sym)
            self.namespace.append(func)
            body = StringIO('w')
            default = get_default_for_typeval(func.retval)
            body.write("  *arg0 = %s;\n" % (default, ))
            body.write("  return;\n")
            self.gen.set_function_body(func, body.getvalue())

        # Passthrough one parameter
        prefix = 'passthrough_one '
        for typeval in types:
            if typeval is girast.TYPE_NONE:
                continue
            name = prefix + uscore_from_type(typeval)
            sym = self.gen.gen_symbol(name)
            func = girast.Function(name, girast.Return(typeval, transfer=girast.PARAM_TRANSFER_NONE),
                            [girast.Parameter('arg0', typeval, transfer=girast.PARAM_TRANSFER_NONE,
                                       direction=girast.PARAM_DIRECTION_IN)], False, sym)
            self.namespace.append(func)
            body = StringIO('w')
            default = get_default_for_typeval(func.retval)
            body.write("  return arg0;\n")
            self.gen.set_function_body(func, body.getvalue())

        self.gen.codegen()
