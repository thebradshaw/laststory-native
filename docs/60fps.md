# 60 frames a second

Status: experimental (`TLS_FPS=60`). The default is the disc's 30.

## What the engine does

The engine steps by frames, 30 a second, and flips on every other retrace
(`atnGfx` `+0xCC`, set at each update by 80479D8C from the settings' rate, read by
`atnGfx_PreRetrace` and `atnGfx_PostRetrace`).

Its settings (`Config`, the object at `-0x6618(r13)`) hold a rate at `+0x30`, 30 on the disc.
Only gravity reads it: 51 places, each by its square (`mullw r0,r0,r0`).

## First try: the settings' rate (wrong)

Setting the rate to 60 makes the frames flip at every retrace, and nothing else follows:
on a GPU the game ran at twice its speed, with sound distortion on the title's confirm
(reported from the first Windows test build, 2026-11-11).

## Second try: the engine's slow motion

The engine has one global speed: `+0x3A4` of the object at `-0x6718(r13)`, 1.0, less in slow
motion. 119 places read it through that pointer and 47 store it, in 32 functions; gravity's 51
sites read it too, and so do the effects (`atnEff_Advance`).

With `TLS_FPS=60`:

- `tools/tls.cpp` clears `+0xCC` before the retrace callbacks read it: a flip at every retrace.
- `tools/speed60.py` patches the recompiled C++: the 107 reads of the speed outside the
  functions that store it, and the getter at 802F0988, multiply it by `g_tls_speed` (0.5).
  The world moves in half steps, twice as often.

What does not follow the speed will run twice as fast. Candidates, to be checked on a GPU:
menus and 2D animations, cutscenes (the event system has its own frame counters), timers,
the camera.

## Tested

Linux, software rendering, so no real frame rates: at 60 the game runs from boot into
Chapter 1's first tutorial without errors. Speeds are unverified until a run on a GPU.

## Attempt 3: frame interpolation (current)

The first two attempts changed the game's own stepping and drifted its
timing (animation and event timing, reported on hardware). The third leaves
the game at its own 30 fps and works in the renderer (`--interpolate`, in
`patches/wiikit-local.patch`): each frame's command record is kept, and once
the next frame is complete it is replayed a second time as the halfway
frame. Each draw is matched to the frame before by what it draws (primitive,
vertex format and count, bound textures, position matrix index; then the
same without the count), and gets its position, texture, normal and
projection matrices halfway between the two; vertices moved on the CPU
(skinning, sprites) are lerped too. Matrices that change too much (a cut)
and vertex jumps (a particle born again) are drawn as they are. The halfway
frame has its own EFB and EFB copies and restores the renderer state after,
so the real frame is untouched. It is shown one retrace before the real
frame: 17 ms more latency. Headless check in Chapter 1: 99% of draws matched,
the halfway frame equidistant from its neighbours. Debugging:
`WIIKIT_INTERPDUMP=DIR,FIRST,COUNT,STRIDE`, `WIIKIT_INTERPDEBUG=FRAME`.
The slow motion experiment is still there as `TLS_SLOWMO60=1`.

### Build 8: effect flicker

A camera-turn test with a halfway frame every frame showed torch fire, its
glow on the cave wall and its reflection in water dimmer in roughly one
halfway frame in six. Two causes, both fixed in `video.cpp`:

- Additive draws (blend on, destination factor ONE: fire, sparks) matched
  only by kind were often a different particle of the same effect. Their
  vertices are no longer lerped; they draw as in the real frame.
  Flickering frames in the test: 32 of 180 before, 0 of 120 after.
- A halfway frame read an EFB copy it had not made itself from the real
  frame's latest one, which may be a frame ahead. The first real copy over
  an address in a frame now keeps the old one aside (`start_copies`) for
  the halfway frame to read.

Also for lower-spec PCs: textures of 256x256 and up decode across threads
(`gxtex_decode_parallel`), and a halfway frame is left out while a whole
game frame already waits (so the game never slows to wait for them).
`WIIKIT_INTERP_ALWAYS=1` turns that off for tests; `WIIKIT_INTERPDEBUG=A,B`
logs each halfway draw's match, blend and textures for frames A to B.
