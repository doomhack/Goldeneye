#!/usr/bin/env python3
"""Add const to pointer parameters that are only read through.

  tools/constify.py scan                 list candidate parameters
  tools/constify.py run [--dry]          add const, compile, drop const where it fails, repeat

A candidate is a parameter written as a one-level pointer (or array) to a non-const,
non-pointer, non-function type, in a function defined in src/. The run step puts const on
every candidate (definition and every prototype), compiles every game C file with
-fsyntax-only -Wcast-qual for US and EU (so a cast can't hide a write), and takes const back off any parameter named on a line with an
error (inside its own function), or off every parameter of a function named on a line with
an incompatible-pointer error (a callback whose type must match). It repeats until both
builds are clean, so only parameters that compile as const in both regions keep it.

Needs the libclang Python bindings (pip install libclang) and the game toolchain in PATH.
Adding const to a parameter never changes the generated code; check with
tools/compare/compare.sh.
"""
import json
import os
import re
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

import clang.cindex as ci

for lib in ('/usr/lib/llvm-18/lib/libclang-18.so.1', '/usr/lib/llvm-18/lib/libclang.so.1'):
    if os.path.exists(lib):
        ci.Config.set_library_file(lib)
        break

ROOT = subprocess.check_output(['git', 'rev-parse', '--show-toplevel'], text=True).strip()
os.chdir(ROOT)
SKIP = {'src/game/chr/chraidata.c'}  # data only, built through the AI_PRINT sed pipe
VERSIONS = {'US': 'u', 'EU': 'e'}
CLANG_FLAGS = ['--target=mips-unknown-elf', '-nostdinc', '-isystem', '/usr/lib/llvm-18/lib/clang/18/include',
               '-fms-extensions', '-funsigned-char', '-std=gnu2x', '-D_LANGUAGE_C', '-D_MIPS_SZLONG=32',
               '-D_MIPS_SZINT=32', '-D_MIPS_SZPTR=32', '-DTARGET_N64', '-DBUILD_VERSION=VERSION_L', '-D_FINALROM',
               '-I.', '-Iinclude', '-Isrc', '-Ilib/ultralib/include', '-Ilib/ultralib/include/PR',
               '-DF3DEX_GBI_2', '-Wno-everything']
# Region defines as the Makefile sets them.
VFLAGS = {'US': ['-DVERSION_US'],
          'EU': ['-DVERSION_EU']}
STATE = os.path.join(ROOT, 'build', 'constify.json')


def c_files():
    out = []
    for d, _, fs in os.walk('src'):
        for f in fs:
            p = os.path.join(d, f)
            if f.endswith('.c') and p not in SKIP:
                out.append(p)
    return sorted(out)


def in_repo(path):
    rel = os.path.relpath(path, ROOT)
    return rel.startswith('src/') or rel.startswith('include/'), rel


def candidate_type(t):
    """True for 'T *' or 'T x[]' written in the declaration, T not const/pointer/function."""
    if t.kind == ci.TypeKind.POINTER:
        inner = t.get_pointee()
    elif t.kind in (ci.TypeKind.CONSTANTARRAY, ci.TypeKind.INCOMPLETEARRAY):
        inner = t.element_type
        while inner.kind == ci.TypeKind.CONSTANTARRAY:
            inner = inner.element_type
    else:
        return False
    if inner.is_const_qualified():
        return False
    canon = inner.get_canonical()
    if canon.kind in (ci.TypeKind.POINTER, ci.TypeKind.FUNCTIONPROTO, ci.TypeKind.FUNCTIONNOPROTO,
                      ci.TypeKind.BLOCKPOINTER):
        return False
    if canon.is_const_qualified():  # const hidden in a typedef
        return False
    return True


def scan():
    """Returns {usr: {'name', 'params': {i: {'name', 'edits': [[file, line, col, offset]]}}, 'defs': [[file, l0, l1]]}}"""
    funcs = {}
    index = ci.Index.create()

    def visit(tu):
        for c in tu.cursor.walk_preorder():
            if c.kind != ci.CursorKind.FUNCTION_DECL:
                continue
            loc = c.location
            if loc.file is None:
                continue
            ok, rel = in_repo(loc.file.name)
            if not ok:
                continue
            key = c.get_usr()
            f = funcs.setdefault(key, {'name': c.spelling, 'params': {}, 'defs': []})
            if c.is_definition():
                # A function can have one definition per region (#if VERSION_EU ... #else).
                d = [rel, c.extent.start.line, c.extent.end.line]
                if d not in f['defs']:
                    f['defs'].append(d)
            params = [a for a in c.get_children() if a.kind == ci.CursorKind.PARM_DECL]
            for i, p in enumerate(params):
                if not candidate_type(p.type):
                    continue
                st = p.extent.start
                if st.file is None or os.path.relpath(st.file.name, ROOT) != rel:
                    continue  # declared through a macro
                # The parameter text must start with its type, not inside a macro expansion.
                toks = list(p.get_tokens())
                if not toks or toks[0].extent.start.offset != st.offset:
                    continue
                if toks[0].spelling == 'const':
                    continue
                e = f['params'].setdefault(i, {'name': p.spelling, 'edits': []})
                if c.is_definition() or not e['name']:
                    e['name'] = p.spelling  # the definition's name is the one used in the body
                ed = [rel, st.line, st.column, st.offset]
                if ed not in e['edits']:
                    e['edits'].append(ed)

    for v in VERSIONS:
        files = c_files()
        for path in files:
            tu = index.parse(path, CLANG_FLAGS + VFLAGS[v])
            errs = [d for d in tu.diagnostics if d.severity >= ci.Diagnostic.Error]
            if errs:
                print(f'{path}: clang errors, skipped: {errs[0]}', file=sys.stderr)
                continue
            visit(tu)
    # Only functions defined in src/, with every parameter edit location known.
    out = {}
    for key, f in funcs.items():
        if not f['defs'] or not all(d[0].startswith('src/') for d in f['defs']) or not f['params']:
            continue
        f['params'] = {str(i): p for i, p in f['params'].items()}
        out[key] = f
    return out


def apply(funcs, enabled):
    """Write the sources with const on every enabled (key, param) pair. Returns the originals."""
    edits = {}
    for key, idx in enabled:
        for rel, line, col, off in funcs[key]['params'][idx]['edits']:
            edits.setdefault(rel, set()).add(off)
    orig = {}
    for rel, offs in edits.items():
        data = open(rel, 'rb').read()
        orig[rel] = data
        for off in sorted(offs, reverse=True):
            data = data[:off] + b'const ' + data[off:]
        open(rel, 'wb').write(data)
    return orig


def restore(orig):
    for rel, data in orig.items():
        open(rel, 'wb').write(data)


def compile_cmds(v):
    """gcc command line for each C file, from make -n."""
    code = VERSIONS[v]
    objs = [f'build/{code}/' + p[:-2] + '.o' for p in c_files()]
    out = subprocess.run(['make', '-n', '-B', f'VERSION={v}', 'LTO=0'] + objs, capture_output=True, text=True).stdout
    cmds = []
    for line in out.splitlines():
        m = re.search(r'\s(src/\S+\.c)$', line)
        if 'gcc -c' in line and m:
            cmd = re.sub(r'\s-o\s+\S+', ' ', line)
            cmd = cmd.replace(' -MMD -MP', '').replace(' -c ', ' -fsyntax-only -ftrack-macro-expansion=0 -fmax-errors=0 -Werror=cast-qual ', 1)
            cmds.append(cmd)
    return cmds


def build_errors(cmds):
    def one(cmd):
        r = subprocess.run(cmd, shell=True, capture_output=True, text=True)
        return r.stderr
    errs = []
    with ThreadPoolExecutor(os.cpu_count()) as ex:
        for text in ex.map(one, cmds):
            for line in text.splitlines():
                m = re.match(r'([^:\s]+):(\d+):\d+: error: (.*)', line)
                if m:
                    errs.append((os.path.relpath(m.group(1), ROOT), int(m.group(2)), m.group(3)))
    return sorted(set(errs))


def split_args(text):
    """Top-level comma-separated arguments of the call whose '(' starts text, as far as the line goes."""
    out, depth, cur = [], 0, ''
    for ch in text:
        if ch in '([{':
            depth += 1
            if depth == 1:
                continue
        elif ch in ')]}':
            depth -= 1
            if depth == 0:
                out.append(cur)
                return out
        elif ch == ',' and depth == 1:
            out.append(cur)
            cur = ''
            continue
        cur += ch
    out.append(cur)
    return out


def focus(text, msg):
    """The part of an error line that the error is about, so that only the parameters named
    there lose their const (a call often passes several parameters, and only one fails)."""
    m = re.match(r"passing argument (\d+) of '(\w+)'", msg)
    if m:
        n, fn = int(m.group(1)), m.group(2)
        pos = [x.start() for x in re.finditer(r'\b' + fn + r'\s*\(', text)]
        if len(pos) == 1:
            args = split_args(text[text.index('(', pos[0]):])
            if n <= len(args):
                return args[n - 1]
        return text
    m = re.search(r"(?:assignment|increment|decrement) of read-only (?:variable|parameter) '(\w+)'", msg)
    if m:
        return m.group(1)
    m = re.match(r"(?:assignment|increment|decrement) of read-only location '(.*)'", msg)
    if m:
        return m.group(1)
    if re.match(r"(?:assignment|increment|decrement) of member '\w+' in read-only object", msg):
        # The left-hand side: 'p->a.member = x', 'p->arr[i].member += x', 'p->member++'.
        m = re.match(r'(.*?)(?:[-+*/%&|^]|<<|>>)?=(?!=)', text)
        return m.group(1) if m else text
    if re.match(r'(assignment|initialization of \'.*\') discards', msg) or msg.startswith('initialization discards'):
        if '=' in text:
            return text.split('=', 1)[1]
    if msg.startswith('return discards'):
        return text.split('return', 1)[-1]
    return text


def run(dry=False):
    funcs = scan()
    json.dump(funcs, open(STATE, 'w'), indent=0)
    enabled = {(k, i) for k, f in funcs.items() for i in f['params']}
    print(f'{len(enabled)} candidate parameters in {len(funcs)} functions')
    cmds = {v: compile_cmds(v) for v in VERSIONS}
    by_file = {}
    for k, f in funcs.items():
        for d in f['defs']:
            by_file.setdefault(d[0], []).append((k, d[1], d[2]))
    # Errors the unchanged tree already has with -Wcast-qual (strchr-style casts) don't count.
    baseline = set()
    for v in VERSIONS:
        baseline |= set(build_errors(cmds[v]))
    print(f'{len(baseline)} existing cast-qual error lines ignored')
    lines = {}
    it = 0
    while True:
        it += 1
        orig = apply(funcs, enabled)
        try:
            errs = []
            for v in VERSIONS:
                errs += build_errors(cmds[v])
        finally:
            restore(orig)
        errs = sorted(set(errs) - baseline)
        print(f'pass {it}: {len(enabled)} enabled, {len(errs)} error lines')
        if not errs:
            break
        drop = set()
        unmatched = []
        for rel, ln, msg in errs:
            if rel not in lines:
                lines[rel] = open(rel, encoding='latin-1').read().split('\n')
            text = lines[rel][ln - 1] if ln - 1 < len(lines[rel]) else ''
            words = set(re.findall(r'\b\w+\b', focus(text, msg)))
            found = False
            for k, l0, l1 in by_file.get(rel, []):
                f = funcs[k]
                if l0 <= ln <= l1:
                    for i, p in f['params'].items():
                        if (k, i) in enabled and p['name'] in words:
                            drop.add((k, i))
                            found = True
            if 'incompatible' in msg or 'conflicting' in msg:
                for k, f in funcs.items():
                    if f['name'] in words:
                        for i in f['params']:
                            if (k, i) in enabled:
                                drop.add((k, i))
                                found = True
            if not found:
                # Usually a macro that uses a parameter without naming it on the line: take
                # const off every parameter of the enclosing function.
                for k, l0, l1 in by_file.get(rel, []):
                    if l0 <= ln <= l1:
                        for i in funcs[k]['params']:
                            if (k, i) in enabled:
                                drop.add((k, i))
                                found = True
            if not found:
                unmatched.append((rel, ln, msg))
        if not drop:
            for u in unmatched[:20]:
                print('unmatched:', *u)
            sys.exit('no progress')
        enabled -= drop
    print(f'{len(enabled)} parameters can be const')
    if not dry:
        apply(funcs, enabled)
    json.dump(sorted(enabled), open(STATE + '.enabled', 'w'))


if __name__ == '__main__':
    cmd = sys.argv[1] if len(sys.argv) > 1 else 'run'
    if cmd == 'scan':
        f = scan()
        n = sum(len(x['params']) for x in f.values())
        print(f'{n} candidate parameters in {len(f)} functions')
    else:
        run('--dry' in sys.argv)
