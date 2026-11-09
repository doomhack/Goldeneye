#!/usr/bin/env python3
"""Make a PAL (P) text bank source from the English (E) one.

The PAL release uses British spelling and rewords a few lines; everything else
is the same text. Building P from E means a string only has to be added once.

usage: brittext.py LxxxE.c LxxxP.c
"""
import re
import sys

# American -> British word stems. Matching is on whole words and keeps the case
# of the first letter (and of all-caps words).
WORDS = {
    "analyzer": "analyser",
    "analyzing": "analysing",
    "armor": "armour",
    "armored": "armoured",
    "authorized": "authorised",
    "center": "centre",
    "defense": "defence",
    "defenses": "defences",
    "jeopardize": "jeopardise",
    "license": "licence",
    "licensed": "licenced",
    "minimize": "minimise",
    "minimized": "minimised",
    "neutralize": "neutralise",
    "rumored": "rumoured",
    "unlicensed": "unlicenced",
}

# Lines the PAL release reworded. Each pair is an exact substring of the E
# source and its PAL replacement. Both the mixed-case and lower-case copies of
# a briefing are listed, as the E bank holds both.
PHRASES = [
    # Facility: Q's briefing explains the mine placement instead of the arming rule.
    ("Now listen carefully, Bond. Those bombs will be armed as soon as the last one is set.",
     "Now listen here, Bond. Be sure to place the mines carefully, otherwise you won't take out all the tanks in the bottling room."),
    (" undamaged for once. Too much rough handling like the last mission and it might go wrong at a bad time.\\n",
     " undamaged for once.\\n"),
    ("now listen carefully, bond. those bombs will be armed as soon as the last one is set.",
     "now listen here, bond. be sure to place the mines carefully, otherwise you won't take out all the tanks in the bottling room."),
]


def _word(m):
    w = m.group(0)
    b = WORDS.get(w.lower())
    if b is None:
        return w
    if w.isupper() and len(w) > 1:
        return b.upper()
    if w[0].isupper():
        return b[0].upper() + b[1:]
    return b


def convert(src, name_e, name_p):
    for a, b in PHRASES:
        src = src.replace(a, b)
    src = re.sub(r"\b" + re.escape(name_e) + r"\b", name_p, src)
    # Only touch string literals, never identifiers or comments.
    return re.sub(r'"(?:[^"\\]|\\.)*"', lambda m: re.sub(r"[A-Za-z]+", _word, m.group(0)), src)


def main():
    src_path, dst_path = sys.argv[1], sys.argv[2]
    name_e = re.search(r"(L\w+)E\.c$", src_path).group(1) + "E"
    name_p = name_e[:-1] + "P"
    with open(src_path, encoding="latin-1") as f:
        src = f.read()
    with open(dst_path, "w", encoding="latin-1") as f:
        f.write(convert(src, name_e, name_p))


if __name__ == "__main__":
    main()
