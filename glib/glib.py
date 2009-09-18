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

def pretty_printer_lookup (val):
    # None yet, want things like hash table and list
    return None

def register (obj):
    if obj == None:
        obj = gdb

    obj.pretty_printers.append(pretty_printer_lookup)
