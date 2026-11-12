// The Last Story — the port's own layer over the wiikit runtime.
//
// Linked into wiiboot by tls.cmake; the functions it replaces are listed in
// tls-hooks.txt, which the recompiler takes with --hooks. What belongs here
// is what only this game needs.
#include "rt.h"
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <thread>
#include <vector>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

// Half at 60 frames a second: what the recompiled code multiplies the engine's speed by (speed60.py).
double g_tls_speed = [] { const char* e = std::getenv("TLS_FPS"); return e && std::atoi(e) == 60 ? 0.5 : 1.0; }();

namespace {

// The engine's frame ring (atn_graphics_device.cpp): 5 x {FIFO wptr, 0, XFB,
// token}, +0x50 the last written (5: empty), +0x52 the next to leave.
constexpr uint32_t kRing = 0x807C6F68, kRingQueue = 0x807C6F60;
constexpr uint32_t kOSDisableInterrupts = 0x805F1E80, kOSRestoreInterrupts = 0x805F1EC0, kOSSleepThread = 0x805F5C70,
                   kOSWakeupThread = 0x805F5D60;
uint32_t ring_count() {
    uint32_t w = ld16(kRing + 0x50), r = ld16(kRing + 0x52);
    return w == 5 ? 0 : (w >= r ? w - r : w + 5 - r) + 1;
}

// TLS_GFXTRACE=1: the ring traced.
PPCFunc o_swap, o_bp, o_pre, o_post;
bool gfxtrace = std::getenv("TLS_GFXTRACE") != nullptr;
uint32_t traced = 0;
void ring(const char* what, PPCContext& c) {
    if (!gfxtrace || traced > 20000) return;
    traced++;
    uint32_t gd = ld32(c.r[13] - 0x67E0);
    rt_log("gfx: %-5s r3 %08X  ring w %u r %u  flags bp %u flip %u act %u  tok %u  xfb 78 %08X 7c %08X",
           what, c.r[3], ld16(kRing + 0x50), ld16(kRing + 0x52), ld8(c.r[13] - 0x67E7), ld8(c.r[13] - 0x67E6),
           ld8(c.r[13] - 0x67E8), ld16(c.r[13] - 0x67E4), gd ? ld32(gd + 0x78) : 0, gd ? ld32(gd + 0x7C) : 0);
}

// TLS_HITCH=1: frames that take more than 60 ms, and where the guest
// threads are once one is 50 ms old.
bool hitch_on = std::getenv("TLS_HITCH") != nullptr;
std::atomic<int64_t> swap_at{0};
std::atomic<uint32_t> swaps{0};
int64_t now_us() {
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
void hitch_watch() {
    uint32_t seen = 0;
    bool told = false;
    for (;;) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        uint32_t n = swaps.load();
        if (n != seen) { seen = n; told = false; continue; }
        if (!n || told || now_us() - swap_at.load() < 50000) continue;
        told = true;
        rt_log("hitch: frame %u late (%.3f s)", n, os_tb_now() / 60750000.0);
        os_report_running(14);
    }
}

// The frame's end. The engine queues the frame and goes on while at most
// one is queued: the CPU a frame ahead, the GP held at the queued frame's
// end (its breakpoint) until the picture before it has been on screen.
// wiikit's GP keeps up with the CPU, so it waits at that breakpoint through
// most of the next frame, and so does everything the CPU asks of it then: a
// character's equipment textures are composed by the GP mid-frame, each
// with a GXDrawDone, which waited for the flip, up to 33 ms; a character
// came in with a stall of 80-130 ms, sound included. The CPU goes on here
// once the ring is empty: its next frame's commands meet no breakpoint.
// (TLS_AHEAD=1: the engine's own rule, a frame ahead.)
bool ahead = std::getenv("TLS_AHEAD") != nullptr;

// The frame rate. The engine steps by frames, 30 a second: its settings hold
// a rate (+0x30 of the object at -0x6618(r13)), but only gravity reads it
// (51 places, by its square); set to 60 alone, the game ran at twice its
// speed (the first build that tried). What the whole world does follow is
// the engine's one speed, the slow motion's (+0x3A4 of the object at
// -0x6718(r13)). TLS_FPS=60: the frames flip at every retrace (the graphics
// device's +0xCC cleared before its retrace callbacks read it) and every
// consumer of the speed is handed half of it (g_tls_speed, by
// tools/speed60.py in the recompiled code): half steps, twice as often.
// What does not follow the speed runs twice as fast: an experiment.
// TLS_FPSLOG=1 reports the frames presented per guest second.
constexpr uint32_t kSda = 0x808856C0;
uint32_t want_fps = [] { const char* e = std::getenv("TLS_FPS"); return e && std::atoi(e) == 60 ? 60u : 30u; }();
bool fpslog = std::getenv("TLS_FPSLOG") != nullptr;
uint64_t fps_t0 = 0;
uint32_t fps_n = 0;
void every_retrace() {
    if (want_fps != 60) return;
    if (uint32_t gd = ld32(kSda - 0x67E0)) st32(gd + 0xCC, 0);
}
void frame_rate() {
    if (!fpslog) return;
    uint64_t t = os_tb_now();
    if (!fps_t0) fps_t0 = t;
    fps_n++;
    if (t - fps_t0 >= 60750000ull * 5) {
        rt_log("tls: %.1f frames a second", fps_n / ((t - fps_t0) / 60750000.0));
        fps_t0 = t; fps_n = 0;
    }
}

// The wrist strap screen (StrapTask, its state at +0x48: 1 fading in, 0
// waiting for a button or 599 frames, 2 fading out over 15 frames, then the
// next sequence). Sent straight to the end of its fade out. TLS_STRAP=1
// keeps it.
bool strap = std::getenv("TLS_STRAP") != nullptr;
PPCFunc o_strap;
void strap_update(PPCContext& c) {
    uint32_t t = c.r[3], state = ld32(t + 0x48);
    if (!strap && state <= 1) { st32(t + 0x48, 2); st32(t + 0x4C, 14); }
    o_strap(c);
}

void swap_frame(PPCContext& c) {
    frame_rate();
    if (hitch_on) {
        int64_t now = now_us(), last = swap_at.exchange(now);
        uint32_t n = swaps.fetch_add(1) + 1;
        if (last && now - last > 60000) rt_log("hitch: frame %u took %.1f ms", n, (now - last) / 1000.0);
    }
    ring("swap", c);
    o_swap(c);
    ring("swap>", c);
    if (ahead) return;
    ppc_call_indirect(c, kOSDisableInterrupts);
    uint32_t level = c.r[3];
    while (ring_count() > 0) {                         // asleep with interrupts off, as the SDK means it
        c.r[3] = kRingQueue;
        ppc_call_indirect(c, kOSSleepThread);
    }
    c.r[3] = level;
    ppc_call_indirect(c, kOSRestoreInterrupts);
}

// The engine's own wait (at most one frame queued) enables interrupts and
// then sleeps: here a pending interrupt is taken right there, and a frame
// that leaves the ring then wakes no one. With nothing left to leave, the
// game slept for good (seen once under load, after its third frame, and at
// once with the wait above). The ring's queue is woken at every retrace too:
// a lost wake-up costs a retrace at most.
void post_retrace(PPCContext& c) {
    every_retrace();
    ring("post", c);
    o_post(c);
    c.r[3] = kRingQueue;
    ppc_call_indirect(c, kOSWakeupThread);
}

// The equipment's textures (the bodies') are made at load time: the GP
// composes them (atn::TextureComposite), GXCopyTex puts them in RAM as
// RGBA8, and the CPU turns that copy's tiles into rows in place
// (atnTex_Unswizzle), merges an alpha mask and compresses it to CMPR
// (atnTex_CompressCopy). wiikit keeps EFB copies on the host GPU: the copy's
// bytes are written into RAM before the game first reads them, or the
// bodies come out black. (The mask is a texture from the disc: no copy.)
PPCFunc o_unswizzle, o_composite;
void unswizzle(PPCContext& c) {                       // (data, format, w, h)
    gx_copy_to_ram(c.r[3] & 0x3FFFFFFF);
    o_unswizzle(c);
}

// The HOME menu, made for a PC: its "Wii Menu" button (which wiikit takes
// as leaving) reads "Exit Game", and its question "Return to your desktop?". The
// menu's font only has the glyphs its own messages use (no E, C or Q,
// for one), hence the wording. The menu's files are loaded by HBMRsoCreate (HBMDataInfo: +0 the layout
// archive, U8; +8 the messages, UTF-16 CSV), and the menu keeps pointing
// into them: both are rewritten in place once it returns, the text at the
// same length.
const uint8_t kExitLabel[] = {
#include "hbm-exit-label.inc"
};
PPCFunc o_hbm_create;

// A file's bytes in a U8 archive at guest address a, by its name.
uint32_t u8_file(uint32_t a, const char* name, uint32_t& size) {
    if (!a || ld32(a) != 0x55AA382Du) return 0;
    uint32_t root = a + ld32(a + 4), n = ld32(root + 8), names = root + n * 12;
    if (n > 0x10000) return 0;
    for (uint32_t i = 1; i < n; ++i) {
        uint32_t node = root + i * 12, tn = ld32(node);
        if (tn >> 24) continue;                                   // a directory
        uint32_t k = 0, at = names + (tn & 0xFFFFFF);
        while (name[k] && ld8(at + k) == (uint8_t)name[k]) ++k;
        if (name[k] || ld8(at + k)) continue;
        size = ld32(node + 8);
        return a + ld32(node + 4);
    }
    return 0;
}

void hbm_label(uint32_t arc) {
    uint32_t size = 0, tpl = u8_file(arc, "tx_btn_L_00.tpl", size);
    if (!tpl || ld32(tpl) != 0x0020AF30u || !ld32(tpl + 4)) return;
    // TPLBind may have made its offsets pointers already.
    auto at = [tpl](uint32_t v) { return v >= 0x80000000u ? v : tpl + v; };
    uint32_t img = at(ld32(at(ld32(tpl + 8))));                   // the first image's header
    uint32_t data = at(ld32(img + 8));
    if (ld16(img) != 64 || ld16(img + 2) != 240 || ld32(img + 4) != 0 || data + sizeof kExitLabel > tpl + size) return;
    std::memcpy(host(data), kExitLabel, sizeof kExitLabel);
    rt_log("tls: HOME menu: \"Wii Menu\" now reads \"Exit Game\"");
}

void hbm_text(uint32_t csv, uint32_t span) {
    static const char16_t from[] = u"Return to the Wii Menu?\r\n(Anything not saved will be lost.)",
                          to[] = u"Return to your desktop?\r\n(Anything not saved will be lost.)";
    static_assert(sizeof from == sizeof to, "the CSV is rewritten in place");
    constexpr uint32_t n = sizeof from / 2 - 1;
    int done = 0;
    for (uint32_t at = csv; at + n * 2 <= csv + span; at += 2) {
        uint32_t k = 0;
        while (k < n && ld16(at + k * 2) == from[k]) ++k;
        if (k < n) continue;
        for (k = 0; k < n; ++k) st16(at + k * 2, to[k]);
        ++done;
    }
    if (done) rt_log("tls: HOME menu: its question now reads \"Return to your desktop?\" (%d)", done);
}

void hbm_create(PPCContext& c) {
    uint32_t info = c.r[3];
    o_hbm_create(c);
    hbm_label(ld32(info));
    uint32_t csv = ld32(info + 8);
    if (csv && ld16(csv) == 0xFEFF) hbm_text(csv, 0x2000);       // home.csv: 4.5 KB
}

// The HOME menu's Reset (OSRestart): the game started again, where wiikit
// would only leave.
[[noreturn]] void restart(PPCContext&) {
    std::fflush(stdout);
    rt_log("tls: the game restarts");
    std::fflush(stderr);
#ifdef _WIN32
    STARTUPINFOW si{};
    si.cb = sizeof si;
    PROCESS_INFORMATION pi{};
    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    if (CreateProcessW(exe, GetCommandLineW(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
#else
    std::ifstream f("/proc/self/cmdline", std::ios::binary);
    std::string all((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    std::vector<char*> argv;
    for (size_t i = 0; i < all.size(); i += std::strlen(&all[i]) + 1) argv.push_back(&all[i]);
    argv.push_back(nullptr);
    if (argv.size() > 1) execv("/proc/self/exe", argv.data());
#endif
    std::_Exit(0);
}

void install() {
    o_unswizzle = ppc_hook("atnTex_Unswizzle", unswizzle);
    o_swap = ppc_hook("atnGfx_SwapFrame", swap_frame);
    o_strap = ppc_hook("StrapTask_Update", strap_update);
    o_hbm_create = ppc_hook("HBMRsoCreate", hbm_create);
    if (!std::getenv("TLS_RESET_QUITS")) ppc_hook("OSRestart", restart);
    o_pre = ppc_hook("atnGfx_PreRetrace", [](PPCContext& c) { every_retrace(); ring("pre", c); o_pre(c); });
    if (want_fps == 60) rt_log("tls: 60 frames a second (experimental), the speed halved");
    o_post = ppc_hook("atnGfx_PostRetrace", post_retrace);
    // The game plays with the Classic Controller (or the Remote and Nunchuk,
    // which wiikit has no Nunchuk for); it learns of controllers from KPAD's
    // connect callback, which wiikit calls for a game of the Classic.
    wpad_set_classic(true);
    if (hitch_on) {
        std::thread(hitch_watch).detach();
        o_composite = ppc_hook("atnTex_Composite", [](PPCContext& c) {
            int64_t t0 = now_us();
            o_composite(c);
            rt_log("hitch: an equipment texture composed in %.1f ms", (now_us() - t0) / 1000.0);
        });
    }
    if (!gfxtrace) return;
    o_bp = ppc_hook("atnGfx_BreakPtCallback", [](PPCContext& c) { ring("bp", c); o_bp(c); ring("bp>", c); });
}

RtGameLayer layer("The Last Story", install);

}  // namespace
