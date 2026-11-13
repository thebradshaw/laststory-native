"""60 frames a second by the engine's own slow motion.

The engine has one global speed (the object at -0x6718(r13), +0x3A4): 1.0, less in slow motion.
118 places read it through that pointer. At 60 frames a second the game's update runs twice as
often, so each of those reads is handed half the speed (tls.cpp's g_tls_speed): the world moves
in half steps, as it does in slow motion, and twice as often.

This patches the recompiled C++ (build/recomp/recomp_*.cpp), after wiikit.recomp:
  - every `lfs fN, 0x3a4(rX)` whose rX was loaded from -0x6718(r13) just before,
  - except in a function that also stores the speed (it computes the next speed from it),
  - and the getter at 802F0988.

    python tools/speed60.py build/extract/sys/main.dol build/recomp
"""
import glob, re, struct, sys

GETTERS = {0x802F0988}


def main(dol_path, out_dir):
    d = open(dol_path, 'rb').read()
    off = struct.unpack('>18I', d[0:72]); addr = struct.unpack('>18I', d[72:144]); size = struct.unpack('>18I', d[144:216])
    o, base, n = off[1], addr[1], size[1]
    w = struct.unpack('>%dI' % (n // 4), d[o:o + n])

    def from_singleton(i, rb):
        for j in range(1, 20):
            y = w[i - j]
            if y >> 26 == 32 and (y >> 21) & 31 == rb and (y >> 16) & 31 == 13 and y & 0xFFFF == 0x98E8:
                return True
        return False

    reads, writes = [], []
    for i, x in enumerate(w):
        if x & 0xFFFF != 0x03A4 or x >> 26 not in (48, 52):
            continue
        if from_singleton(i, (x >> 16) & 31):
            (reads if x >> 26 == 48 else writes).append(base + 4 * i)

    funcs = []
    for line in open(out_dir + '/symbols.tsv'):
        p = line.split('\t')
        try:
            funcs.append((int(p[0], 16), int(p[1])))
        except ValueError:
            pass
    funcs.sort()

    def func_of(a):
        lo, hi = 0, len(funcs) - 1
        while lo < hi:
            mid = (lo + hi + 1) // 2
            if funcs[mid][0] <= a: lo = mid
            else: hi = mid - 1
        return funcs[lo][0]

    writers = {func_of(a) for a in writes}
    targets = {a for a in reads if func_of(a) not in writers} | GETTERS
    pat = re.compile(r'double v = \(double\)as_f32\(ld32\(ea\)\); (.*)//\s+([0-9A-F]{8}) lfs f\d+, 0x3a4\(')
    done = 0
    for path in sorted(glob.glob(out_dir + '/recomp_*.cpp')):
        s = open(path).read()
        if 'g_tls_speed' in s:
            sys.exit('%s is already patched: run wiikit.recomp again first' % path)
        hit = [0]

        def sub(m):
            if int(m.group(2), 16) not in targets:
                return m.group(0)
            hit[0] += 1
            return m.group(0).replace('as_f32(ld32(ea));', 'as_f32(ld32(ea)) * g_tls_speed;', 1)

        t = pat.sub(sub, s)
        if hit[0]:
            k = t.index('\n', t.rindex('#include')) + 1
            t = t[:k] + 'extern double g_tls_speed;\n' + t[k:]
            open(path, 'w').write(t)
            done += hit[0]
    print('speed60: %d reads of the speed, %d stores; %d functions store it; %d reads patched of %d wanted'
          % (len(reads), len(writes), len(writers), done, len(targets)))
    if done != len(targets):
        sys.exit('speed60: some reads were not found in the recompiled code')


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
