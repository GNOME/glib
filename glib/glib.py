import gdb

# This is not quite right, as local vars may override symname
def read_global_var (symname):
    return gdb.selected_frame().read_var(symname)

def g_quark_to_string (quark):
    if quark == None:
        return None
    quark = long(quark)
    if quark == 0:
        return None
    val = read_global_var ("g_quarks")
    max_q = long(read_global_var ("g_quark_seq_id"))
    if quark < max_q:
        return val[quark].string()
    return None

# We override the node printers too, so that node->next is not expanded
class GListNodePrinter:
    "Prints a GList node"

    def __init__ (self, val):
        self.val = val

    def to_string (self):
        return "{data=%s, next=0x%x, prev=0x%x}" % (str(self.val["data"]), long(self.val["next"]), long(self.val["prev"]))

class GSListNodePrinter:
    "Prints a GSList node"

    def __init__ (self, val):
        self.val = val

    def to_string (self):
        return "{data=%s, next=0x%x}" % (str(self.val["data"]), long(self.val["next"]))

class GListPrinter:
    "Prints a GList"

    class _iterator:
        def __init__(self, head, listtype):
            self.link = head
            self.listtype = listtype
            self.count = 0

        def __iter__(self):
            return self

        def next(self):
            if self.link == 0:
                raise StopIteration
            data = self.link['data']
            self.link = self.link['next']
            count = self.count
            self.count = self.count + 1
            return ('[%d]' % count, data)

    def __init__ (self, val, listtype):
        self.val = val
        self.listtype = listtype

    def children(self):
        return self._iterator(self.val, self.listtype)

    def to_string (self):
        return  "0x%x" % (long(self.val))

    def display_hint (self):
        return "array"

def pretty_printer_lookup (val):
    if is_g_type_instance (val):
        return GTypePrettyPrinter (val)

def pretty_printer_lookup (val):
    # None yet, want things like hash table and list

    type = val.type.unqualified()

    # If it points to a reference, get the reference.
    if type.code == gdb.TYPE_CODE_REF:
        type = type.target ()

    if type.code == gdb.TYPE_CODE_PTR:
        type = type.target().unqualified()
        t = str(type)
        if t == "GList":
            return GListPrinter(val, "GList")
        if t == "GSList":
            return GListPrinter(val, "GSList")
    else:
        t = str(type)
        if t == "GList":
            return GListNodePrinter(val)
        if t == "GSList *":
            return GListPrinter(val, "GSList")
    return None

def register (obj):
    if obj == None:
        obj = gdb

    obj.pretty_printers.append(pretty_printer_lookup)
