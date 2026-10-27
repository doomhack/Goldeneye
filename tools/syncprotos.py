#!/usr/bin/env python3
"""Copy parameter names from function definitions into their prototypes.

  tools/syncprotos.py [DIR...]     (default: src/game)

Only prototype parameters still named like decompiler output (arg0, sp34, temp_v0, var_a1,
phi_s0, param_1) are changed, and only when the definition's parameter has a real name.
Types are left alone. Prototypes are looked for in every .c and .h file under the directories.
"""
import os
import re
import sys

DECOMP = re.compile(r'^(sp[0-9A-F][0-9A-Fa-f]*|sp[0-9a-f]*[0-9][0-9a-f]*|temp_\w+|arg\d+|var_\w+|phi_\w+|param_\d+|new_var\d*)$')
DEF = re.compile(r'^[A-Za-z_][\w \t\*]*?\b(\w+)\s*\(([^;{]*?)\)\s*\{', re.M | re.S)
PROTO = re.compile(r'^([ \t]*[A-Za-z_][\w \t\*]*?\b)(\w+)(\s*\()([^;{()]*(?:\([^()]*\)[^;{()]*)*)(\)\s*;)', re.M | re.S)


def split_params(text):
    out, depth, cur = [], 0, ''
    for ch in text:
        if ch == ',' and depth == 0:
            out.append(cur)
            cur = ''
            continue
        depth += ch in '(['
        depth -= ch in ')]'
        cur += ch
    out.append(cur)
    return out


def param_name(param):
    # The last identifier, for plain "type name" and "type name[N]" parameters.
    if '(' in param:
        return None
    m = re.search(r'(\w+)\s*(\[[^\]]*\]\s*)*$', param.strip())
    if not m or m.start() == 0:
        return None
    return m.group(1)


def files(dirs):
    for d in dirs:
        for root, _, names in os.walk(d):
            for n in names:
                if n.endswith(('.c', '.h')):
                    yield os.path.join(root, n)


def main():
    dirs = sys.argv[1:] or ['src/game']
    paths = list(files(dirs))
    defs = {}
    for p in paths:
        if not p.endswith('.c'):
            continue
        for m in DEF.finditer(open(p, encoding='latin1').read()):
            names = [param_name(x) for x in split_params(m.group(2))]
            defs.setdefault(m.group(1), names)

    for p in paths:
        text = open(p, encoding='latin1').read()

        def fix(m):
            names = defs.get(m.group(2))
            params = split_params(m.group(4))
            if not names or len(names) != len(params):
                return m.group(0)
            out = []
            for param, new in zip(params, names):
                old = param_name(param)
                if old and new and DECOMP.match(old) and not DECOMP.match(new):
                    param = re.sub(r'\b' + old + r'\b(?=\s*(\[[^\]]*\]\s*)*$)', new, param.rstrip()) + \
                        param[len(param.rstrip()):]
                out.append(param)
            return m.group(1) + m.group(2) + m.group(3) + ','.join(out) + m.group(5)

        new = PROTO.sub(fix, text)
        if new != text:
            open(p, 'w', encoding='latin1').write(new)


if __name__ == '__main__':
    main()
