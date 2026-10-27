#!/usr/bin/env python3
"""
Remove preprocessor branches that are decided the same way in every build.

    tools/compare/foldflags.py FILE...

The game is built as US or PAL (Makefile VERSION=US / EU). Each build defines a fixed set of
flags, and a few flags are never defined at all. An `#if` whose condition only uses those
flags and comes out the same in both builds is dead weight: this keeps the branch that is
compiled and deletes the directive and the other branches, like `unifdef` does for one
configuration. Conditions that differ between US and PAL, or that use any other macro, are
left alone.

The result builds byte-identical ROMs for both versions (only __LINE__ values can move), so
check it with tools/compare/romcmp.sh.
"""
import re, sys

US = {'VERSION_US', 'LANG_US', 'REFRESH_NTSC', 'BUGFIX_R0', 'BACKPORT_R1', 'TARGET_N64'}
EU = {'VERSION_EU', 'LANG_EU', 'REFRESH_PAL', 'BUGFIX_R1', 'BUGFIX_R2', 'TARGET_N64'}
# Defined by no build: leftovers from other decomps, editors and debug tools.
NEVER = {'ENABLE_LOG', 'XBLADEBUG', 'EXPAND_EXAMPLES', 'ALL_BONDS', 'ISGOLDFINGER', 'XBLA',
         'USECMDBUILDER', 'IS_PD', 'IS_NSNA', 'USERDB', 'USEEXPANSION', '__INTELLISENSE__',
         '_SYNHILITE'}
KNOWN = US | EU | NEVER

DIRECTIVE = re.compile(r'^\s*#\s*(if|ifdef|ifndef|elif|else|endif)\b(.*)$')


def strip_comment(s):
    s = re.sub(r'/\*.*?\*/', ' ', s)
    return s.split('//')[0].split('/*')[0].strip()


def evaluate(expr, defs):
    """Value of a #if expression under `defs`, or None if it uses anything else."""
    e = re.sub(r'\bdefined\s*\(\s*(\w+)\s*\)|\bdefined\s+(\w+)',
               lambda m: ' %d ' % ((m.group(1) or m.group(2)) in defs)
               if (m.group(1) or m.group(2)) in KNOWN else ' UNKNOWN ', expr)
    for name in re.findall(r'\b[A-Za-z_]\w*\b', e):
        if name in KNOWN:
            e = re.sub(r'\b%s\b' % name, ' %d ' % (name in defs), e)
        else:
            return None
    e = e.replace('&&', ' and ').replace('||', ' or ')
    e = re.sub(r'!(?!=)', ' not ', e)
    if not re.fullmatch(r'[\s()0-9a-z=!<>]*', e):
        return None
    try:
        return bool(eval(e, {}))
    except Exception:
        return None


def condition(kind, rest):
    rest = strip_comment(rest)
    if kind == 'ifdef':
        return 'defined(%s)' % rest
    if kind == 'ifndef':
        return '!defined(%s)' % rest
    return rest


def decide(kind, rest):
    """True/False if the branch is taken the same way in both builds, else None."""
    c = condition(kind, rest)
    us, eu = evaluate(c, US), evaluate(c, EU)
    return us if us is not None and us == eu else None


def process(lines, i, out, end_kinds):
    """Copy lines from i until a directive in end_kinds at this level; return its index."""
    while i < len(lines):
        m = DIRECTIVE.match(lines[i])
        if m and m.group(1) in end_kinds:
            return i
        if m and m.group(1) in ('if', 'ifdef', 'ifndef'):
            i = group(lines, i, out)
        else:
            out.append(lines[i])
            i += 1
    return i


def skip(lines, i):
    """Skip a branch body (including nested groups); return the index of the next
    #elif/#else/#endif at this level."""
    depth = 0
    while True:
        m = DIRECTIVE.match(lines[i])
        if m:
            k = m.group(1)
            if k in ('if', 'ifdef', 'ifndef'):
                depth += 1
            elif k == 'endif':
                if depth == 0:
                    return i
                depth -= 1
            elif depth == 0:
                return i
        i += 1


def group(lines, i, out):
    """Handle the conditional group starting at lines[i]; return the index after #endif."""
    emitted_directive = False   # has a kept branch been written with its directive?
    done = False                # a branch was found that is always taken
    while True:
        m = DIRECTIVE.match(lines[i])
        kind, rest = m.group(1), m.group(2)
        if kind == 'endif':
            if emitted_directive:
                out.append(lines[i])
            return i + 1
        if done:
            i = skip(lines, i + 1)
            continue
        if kind == 'else':
            value = True
        else:
            value = decide('if' if kind == 'elif' else kind, rest)
        body = i + 1
        if value is False:
            i = skip(lines, body)
            continue
        if value is True:
            if emitted_directive:
                out.append(re.sub(r'#\s*\w+.*', '#else', lines[i].rstrip('\n'), count=1) + '\n')
            i = process(lines, body, out, ('elif', 'else', 'endif'))
            done = True
            continue
        # Undecided: keep the directive (an #elif becomes #if if it is now first).
        line = lines[i]
        if not emitted_directive and kind == 'elif':
            line = re.sub(r'#\s*elif', '#if', line, count=1)
        out.append(line)
        emitted_directive = True
        i = process(lines, body, out, ('elif', 'else', 'endif'))


def main():
    for path in sys.argv[1:]:
        lines = open(path).readlines()
        out = []
        process(lines, 0, out, ())
        if out != lines:
            open(path, 'w').writelines(out)
            print(path)


if __name__ == '__main__':
    main()
