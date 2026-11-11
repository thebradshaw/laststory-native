# 60 frames a second

The engine already knows two frame rates. Its settings object (`-0x6618(r13)`) holds the rate at
`+0x30`, and the disc sets 30.

- 51 places read it: physics scale by its square (`mullw r0,r0,r0` at 8010FD20, 80198D70,
  80567C70), timers by it.
- The frame's update (80479D7C-80479D8C) tells the graphics device to flip on every other
  retrace only when the rate is 30 (`atnGfx` `+0xCC`, read by `atnGfx_PreRetrace`).
- The effects are the exception: `atnEff_Update` (8023686C) is handed the constant 1.0 frame at
  every frame by its one caller (80239BFC), and the effects' speed (`+0x3A4`) stays 1.0.

`tools/tls.cpp` writes the wanted rate into the settings at each frame's end and scales the
effects' step by 30 / rate. `TLS_FPS=30` restores the disc's rate; the default is 60.
`TLS_FPSLOG=1` logs the frames presented per second, the flip flag and the effects' speed.

## Tested

USA disc, Linux, Mesa llvmpipe (software rendering) under Xvfb, 2026-11-11:

- With 60 the flip flag reads 0 (every retrace); with 30 it reads 1.
- At 60 the game runs from boot through the title, New Game and into Chapter 1's first
  tutorial ("Movement and Camera") without errors: 2,455 frames in a 295 s run.

Not measured: a real 60. Software rendering holds about 7 frames a second in 3D scenes here at
either setting, so speed and smoothness need a run on a GPU. Things to watch on one: cutscene
timing and lip sync, `.motion` playback (authored at 30), THP movies, and anything that still
counts frames (the effects were one).

Seen once at 30 under this load: the game stopped after its third frame, the lost wake-up the
upstream notes describe (`docs/upstream/03-executable.md`). A second run was fine.
