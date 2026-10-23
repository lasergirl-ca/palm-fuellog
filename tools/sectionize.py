#!/usr/bin/env python3
"""Put a file's static functions in a named code section.

GCC places a function in a section only when the attribute is on a
declaration it has seen, so this inserts one prototype per static function
definition, tagged with the macro (SEC2, FORMS_SEC, EXPORT_SEC), just before
the first definition. Idempotent. Usage: sectionize.py MACRO file.c ..."""
import re, sys

DEF = re.compile(r'^(static\s+[\w\s\*]+?\b(\w+)\s*\(([^)]*)\))\s*(\{.*)?$')

def process(path, macro):
    lines = open(path).read().split('\n')
    protos, first = [], None
    have = set(re.findall(r'\b(\w+)\s*\([^;{]*\)\s*' + macro + r'\s*;', '\n'.join(lines)))
    for i, line in enumerate(lines):
        m = DEF.match(line)
        if not m:
            continue
        name = m.group(2)
        if name in have:
            continue
        # A definition: the next non-blank line opens a body, or it is a one-liner.
        nxt = lines[i + 1].strip() if i + 1 < len(lines) else ''
        if not (m.group(4) or nxt.startswith('{')):
            continue
        if first is None:
            first = i
        protos.append('%s %s;' % (m.group(1), macro))
        have.add(name)
    if not protos:
        print('%s: nothing to do' % path); return
    block = ['/* Section placement: see sections.h. */'] + protos + ['']
    lines[first:first] = block
    open(path, 'w').write('\n'.join(lines))
    print('%s: %d prototype(s) in %s' % (path, len(protos), macro))

if __name__ == '__main__':
    macro, files = sys.argv[1], sys.argv[2:]
    for f in files:
        process(f, macro)
