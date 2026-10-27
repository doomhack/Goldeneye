# Text

The game's strings and drawing them.

| File | What it holds |
| --- | --- |
| `src/game/text/language.c` | Loading the language banks and looking up a string by id (`langGet`) |
| `src/game/text/text.c` | The two fonts, and measuring, wrapping and drawing strings (`textRender`, `textRenderOutlined`, `textMeasure`, `textWrap`) |

## Strings

Every string has an id made by `getStringID(bank, slot)`: the bank number times 1024
plus the slot. A bank is one text file from ROM (`LtitleP`, `LoptionsP`...), a table of
offsets followed by the strings. `langGet(id)` finds the bank in `g_LangBanks` and returns
the string, or NULL if that bank isn't loaded.

`g_LangFileNames` gives the ROM file for each bank id: the `E` files in the US build and
the `P` files in the PAL build. `langInit` loads the banks every level uses (guns, titles,
multiplayer menus, pickups, weapon sets, options, misc) once at boot. Each level has its own bank as well
(`langGetBankForStage`): `setup.c` loads it with the level (`langLoadToAddr`) and
`lvTick` frees it when the level ends (`langClearBank`). The briefing screens load a
level's bank into the front end's buffer (`langLoadToBank`).

## Fonts and drawing

There are two fonts, loaded from ROM into the stage memory pool each level
(`textLoadFonts`): Bank Gothic (`g_FontBankGothic`), small, used for the HUD and the
watch, and Zurich Bold (`g_FontZurich`), larger, used by the menus. Each has 94 glyphs,
for the printable ASCII characters.

`textRender` draws a string at `*x, *y` and moves them on: each glyph is a small
textured rectangle, clipped to a box. `textRenderOutlined` draws each glyph with an outline in a second colour.
`textMeasure` works out the size a string will be, and `textWrap` inserts line breaks to
fit a width. `textSetupDraw` sets the RDP state the glyphs need; call it before drawing
text. A few settings change how the next strings draw: `textSetSpacing`,
`textSetOrientation` (rotated 90 degrees), `textSetWordWrap`,
`textSetOverlapCorrection`.

## Traps

* There is no Japanese support any more. The fonts only have ASCII glyphs, and the
  drawing and measuring code skips any byte of 0x80 or above. The Japanese text banks
  (`L...J`), the kanji fonts and the `-j` boot switch were removed in phase 2b.
* `textSetSpacing` stores the negative of what it's given.
* `textUnused` and `textIdentity` are never called.
