#!/usr/bin/env python3
"""Make global tables that nothing writes const, so they move from .data to .rodata.

  tools/constdata.py run [--dry] [--prefix src/game/]
  tools/constdata.py list [--prefix src/game/]

A table can only be const once everything that points into it is a pointer to const, so
this infers const for six kinds of place at once:

  T  initialised file-scope variables (the tables): 'static const s32 tbl[]', or
     'T *const tbl[]' when the elements are pointers. Only those defined under --prefix.
  G  file-scope pointer variables: 'const struct x *g_Current'
  F  pointer fields of structs: 'const struct anim *anim;'
  R  functions returning a pointer: 'const struct x *getEntry(s32 i)'
  P  pointer parameters, as tools/constify.py
  L  local pointer variables

The run step puts const on every candidate, compiles US and EU with -fsyntax-only and
-Wcast-qual, takes const off whatever an error line names (T, G, F and R by name anywhere,
P and L by name inside their own function), and repeats until both regions build. What is
left compiles as const in both regions, and nothing casts the const away.

The tables moving to .rodata changes the memory layout, so test with the demos afterwards.
Everything else only changes types and leaves the code as it was.

CONSTDATA_DEBUG=name1,name2 prints the error lines that take const off those names.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import constify  # noqa: E402  (sets up libclang and the paths)
from constify import ci, ROOT, VERSIONS, CLANG_FLAGS, VFLAGS, c_files, in_repo, compile_cmds, build_errors  # noqa: E402

# Data copied out of its object byte for byte, or read by the boot code: leave alone.
EXCLUDE = {'src/c_data_filler.c', 'src/aspboot.c', 'src/gspboot.c', 'src/rspboot.c', 'src/rom_header.c',
           'src/game/asset/rzipboot.c', 'src/bootcode.c', 'src/game/chr/chraidata.c'}
DEBUG = set(os.environ.get('CONSTDATA_DEBUG', '').split(','))
SKIP_TOKENS = ('static', 'extern', 'inline', 'register', 'volatile', '__attribute__')


def table_const_pos(t):
    """Where const goes to make the object itself read-only: 'type', 'name' or None."""
    if t.is_const_qualified():
        return None
    inner = t
    while inner.kind in (ci.TypeKind.CONSTANTARRAY, ci.TypeKind.INCOMPLETEARRAY):
        inner = inner.element_type
        if inner.is_const_qualified():
            return None
    canon = inner.get_canonical()
    if canon.is_const_qualified():
        return None
    if canon.kind == ci.TypeKind.POINTER:
        return 'name'
    if canon.kind in (ci.TypeKind.FUNCTIONPROTO, ci.TypeKind.FUNCTIONNOPROTO):
        return None
    return 'type'


def pointee_candidate(t):
    """A one-level pointer to a non-const, non-pointer, non-function type, written as such."""
    return t.kind == ci.TypeKind.POINTER and constify.candidate_type(t)


def single_declarator(toks):
    depth = 0
    for t in toks:
        if t.spelling in '([{':
            depth += 1
        elif t.spelling in ')]}':
            depth -= 1
        elif t.spelling in (',', ';') and depth == 0:
            return False
    return True


def own_tokens(c, rel):
    toks = list(c.get_tokens())
    if not toks or toks[0].extent.start.file is None:
        return None
    if os.path.relpath(toks[0].extent.start.file.name, ROOT) != rel:
        return None  # declared through a macro or an #include'd fragment
    return toks


def type_start(toks):
    i = 0
    while i < len(toks) and toks[i].spelling in SKIP_TOKENS:
        if toks[i].spelling == '__attribute__':
            depth = 0
            i += 1
            while i < len(toks):
                depth += toks[i].spelling == '('
                depth -= toks[i].spelling == ')'
                i += 1
                if depth == 0:
                    break
            continue
        i += 1
    if i >= len(toks) or toks[i].spelling == 'const':
        return None
    return toks[i].extent.start.offset


class Cands:
    def __init__(self):
        self.c = {}

    def add(self, key, kind, name, rel, off, scope=None):
        e = self.c.setdefault(key, {'kind': kind, 'name': name, 'edits': [], 'scope': [], 'files': []})
        if [rel, off] not in e['edits']:
            e['edits'].append([rel, off])
        if scope and scope not in e['scope']:
            e['scope'].append(scope)


def scan(prefix):
    cands = Cands()
    table_defs = {}
    index = ci.Index.create()
    for v in VERSIONS:
        for path in c_files():
            tu = index.parse(path, CLANG_FLAGS + VFLAGS[v])
            if any(d.severity >= ci.Diagnostic.Error for d in tu.diagnostics):
                print(f'{path}: clang errors, skipped', file=sys.stderr)
                continue
            for c in tu.cursor.walk_preorder():
                if c.location.file is None:
                    continue
                ok, rel = in_repo(c.location.file.name)
                if not ok:
                    continue
                k = c.kind
                if k == ci.CursorKind.VAR_DECL and c.semantic_parent.kind == ci.CursorKind.TRANSLATION_UNIT:
                    toks = own_tokens(c, rel)
                    if not toks or not single_declarator([t for t in toks if t.spelling != ';']):
                        continue
                    usr = c.get_usr()
                    # T: the object itself.
                    pos = table_const_pos(c.type)
                    has_init = any(ch.kind.is_expression() or ch.kind == ci.CursorKind.INIT_LIST_EXPR
                                   for ch in c.get_children())
                    if pos:
                        if pos == 'name':
                            names = [t for t in toks if t.spelling == c.spelling]
                            off = names[0].extent.start.offset if names else None
                        else:
                            off = type_start(toks)
                        if off is not None:
                            cands.add('T' + usr, 'T', c.spelling, rel, off)
                            if has_init:
                                table_defs.setdefault('T' + usr, set()).add(rel)
                    # G: a pointer variable's target.
                    if pointee_candidate(c.type):
                        off = type_start(toks)
                        if off is not None:
                            cands.add('G' + usr, 'G', c.spelling, rel, off)
                elif k == ci.CursorKind.FIELD_DECL and pointee_candidate(c.type):
                    toks = own_tokens(c, rel)
                    if not toks or not single_declarator(toks):
                        continue
                    off = type_start(toks)
                    if off is not None:
                        cands.add('F' + (c.get_usr() or f'{rel}:{off}'), 'F', c.spelling, rel, off)
                elif k == ci.CursorKind.FUNCTION_DECL:
                    if pointee_candidate(c.result_type):
                        toks = own_tokens(c, rel)
                        if toks:
                            off = type_start(toks)
                            if off is not None:
                                scope = [rel, c.extent.start.line, c.extent.end.line] if c.is_definition() else None
                                cands.add('R' + c.get_usr(), 'R', c.spelling, rel, off, scope)
                    if c.is_definition() and rel.startswith('src/'):
                        scope = [rel, c.extent.start.line, c.extent.end.line]
                        for d in c.walk_preorder():
                            if d.kind != ci.CursorKind.VAR_DECL or not pointee_candidate(d.type):
                                continue
                            toks = own_tokens(d, rel)
                            if not toks or not single_declarator([t for t in toks if t.spelling != ';']):
                                continue
                            off = type_start(toks)
                            if off is not None:
                                cands.add('L' + (d.get_usr() or f'{rel}:{off}'), 'L', d.spelling, rel, off, scope)
    out = cands.c
    # Tables: only those defined (with an initialiser) under the prefix, and nowhere excluded.
    for key in [k for k, e in out.items() if e['kind'] == 'T']:
        files = table_defs.get(key)
        if not files or not all(f.startswith(prefix) and f not in EXCLUDE for f in files):
            del out[key]
        else:
            out[key]['files'] = sorted(files)
    # Parameters, from tools/constify.py's scan.
    for key, f in constify.scan().items():
        for i, p in f['params'].items():
            k = f'P{key}#{i}'
            out[k] = {'kind': 'P', 'name': p['name'], 'edits': [[ed[0], ed[3]] for ed in p['edits']],
                      'scope': f['defs'], 'files': [], 'func': f['name']}
    # A return type is only known to be complete when the function is defined in src/.
    for key in [k for k, e in out.items() if e['kind'] == 'R']:
        if not out[key]['scope'] or not all(s[0].startswith('src/') for s in out[key]['scope']):
            del out[key]
    return out


def apply(cands, enabled):
    edits = {}
    for key in enabled:
        for rel, off in cands[key]['edits']:
            edits.setdefault(rel, set()).add(off)
    orig = {}
    for rel, offs in edits.items():
        data = open(rel, 'rb').read()
        orig[rel] = data
        for off in sorted(offs, reverse=True):
            data = data[:off] + b'const ' + data[off:]
        open(rel, 'wb').write(data)
    return orig


def words_of(text):
    return set(re.findall(r'\b\w+\b', text))


def run(prefix, dry):
    cands = scan(prefix)
    kinds = {}
    for e in cands.values():
        kinds[e['kind']] = kinds.get(e['kind'], 0) + 1
    print('candidates:', ', '.join(f'{k} {n}' for k, n in sorted(kinds.items())))
    enabled = set(cands)
    cmds = {v: compile_cmds(v) for v in VERSIONS}
    baseline = set()
    for v in VERSIONS:
        baseline |= set(build_errors(cmds[v]))
    by_name, scoped_by_file, params_of, returns_in = {}, {}, {}, {}
    for k, e in cands.items():
        if e['kind'] in 'PL':
            for rel, l0, l1 in e['scope']:
                scoped_by_file.setdefault(rel, []).append((k, l0, l1))
            if e['kind'] == 'P':
                params_of.setdefault(e['func'], []).append(k)
        else:
            by_name.setdefault(e['name'], []).append(k)
        if e['kind'] == 'R':
            for rel, l0, l1 in e['scope']:
                returns_in.setdefault(rel, []).append((k, l0, l1))
    lines = {}
    it = 0
    while True:
        it += 1
        orig = apply(cands, enabled)
        try:
            errs = []
            for v in VERSIONS:
                errs += build_errors(cmds[v])
        finally:
            constify.restore(orig)
        errs = sorted(set(errs) - baseline)
        ntab = sum(1 for k in enabled if cands[k]['kind'] == 'T')
        print(f'pass {it}: {ntab} tables, {len(enabled)} enabled, {len(errs)} error lines')
        if not errs:
            break
        drop = set()
        unmatched = []
        for rel, ln, msg in errs:
            if rel not in lines:
                lines[rel] = open(rel, encoding='latin-1').read().split('\n')
            text = lines[rel][ln - 1] if ln - 1 < len(lines[rel]) else ''
            msgwords = set(re.findall(r"'(\w+)'", msg))
            here = [k for k, l0, l1 in scoped_by_file.get(rel, []) if l0 <= ln <= l1 and k in enabled]
            found = set()
            if msg.startswith('return discards'):
                found = {k for k, l0, l1 in returns_in.get(rel, []) if l0 <= ln <= l1 and k in enabled}
            if not found:
                for words in (words_of(constify.focus(text, msg)), words_of(text) | msgwords):
                    found = {k for w in words for k in by_name.get(w, []) if k in enabled}
                    found |= {k for k in here if cands[k]['name'] in words}
                    if found:
                        break
            if 'incompatible' in msg or 'conflicting' in msg:
                for w in words_of(text) | msgwords:
                    found |= {k for k in params_of.get(w, []) if k in enabled}
            if not found:
                found = set(here)  # a macro that uses a name without spelling it on the line
            if found:
                drop |= found
                for k in found:
                    if cands[k]['name'] in DEBUG:
                        print(f'  drop {cands[k]["kind"]} {cands[k]["name"]}: {rel}:{ln}: {msg} :: {text.strip()}')
            else:
                unmatched.append((rel, ln, msg))
        if not drop:
            for u in unmatched[:30]:
                print('unmatched:', *u)
            sys.exit('no progress')
        enabled -= drop
    tables = sorted((k for k in enabled if cands[k]['kind'] == 'T'), key=lambda k: (cands[k]['files'][0], cands[k]['name']))
    counts = {}
    for k in enabled:
        counts[cands[k]['kind']] = counts.get(cands[k]['kind'], 0) + 1
    print('const:', ', '.join(f'{k} {n}' for k, n in sorted(counts.items())))
    for k in tables:
        print('  ', cands[k]['files'][0], cands[k]['name'])
    if not dry:
        apply(cands, enabled)


if __name__ == '__main__':
    args = sys.argv[1:]
    prefix = 'src/game/'
    if '--prefix' in args:
        prefix = args[args.index('--prefix') + 1]
    if args and args[0] == 'list':
        c = scan(prefix)
        for k, e in sorted(c.items(), key=lambda x: (x[1]['kind'], x[1]['edits'][0])):
            print(e['kind'], e['edits'][0][0], e['name'])
    else:
        run(prefix, '--dry' in args)
