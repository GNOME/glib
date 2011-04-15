# -*- Mode: Python -*-

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
