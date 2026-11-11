# The USA disc (SLSEXJ)

The port started on the European disc (SLSP01, build 2350). The USA disc is build 2365,
`files/revision.txt` 29800, and its executable has the same layout:

| | Europe SLSP01 | USA SLSEXJ |
|---|---|---|
| `.text` | 800081C0-8072D2A0 | 800081C0-8072D2A0 |
| bss | 807C66C0 + 0xC2CF0 | 807C66C0 + 0xC2CF0 |
| wiikit units | 18 382 | 18 382 |
| instructions | 1 868 161 | 1 868 162 |
| names (manual / sel / string / signature) | 62 / 11 / 26 / 808 | 62 / 11 / 26 / 808 |

Every hand-named address in `tools/names-manual.tsv` holds the same code on both discs
(checked: `PPCHalt` at 805EAA10, `atnGfx_SwapFrame` at 80071AFC, the WPAD and VI functions),
so `tools/` works unchanged.

Tested on 2026-11-06 under Linux (Ubuntu 24.04, clang 18, SDL 3.2.24 built from source,
Mesa llvmpipe under Xvfb): the USA build boots through the strap screen to the title's
NEW GAME menu.
