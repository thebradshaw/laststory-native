#!/usr/bin/env sh
# Extract, name, recompile and build The Last Story from your own disc (Linux/macOS/MSYS2).
# Usage: tools/build.sh GAME.wbfs|GAME.iso [DOLPHIN_SYS_DIR]
#   DOLPHIN_SYS_DIR holds Dolphin's totaldb.dsy, plus GC/ fonts and dsp_coef.bin (optional).
# Extra CMake args (for example -DCMAKE_PREFIX_PATH=/opt/sdl3) go in $CMAKE_ARGS.
set -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
DISC="${1:?path to your disc image}"
SYS="${2:-}"
B=build
mkdir -p $B
[ -f $B/extract/sys/main.dol ] || python3 -m wiikit.disc "$DISC" --extract $B/extract
if [ -n "$SYS" ]; then
  mkdir -p $B/fonts
  for f in dsp_coef.bin font_western.bin font_japanese.bin; do
    for p in "$SYS/GC/$f" "$SYS/$f"; do [ -f "$p" ] && cp "$p" $B/fonts/ && break; done
  done
  python3 tools/sigmatch.py $B/extract/sys/main.dol --dsy "$SYS/totaldb.dsy" --out $B/sig_guess.tsv
fi
# Local changes to wiikit (patches/), applied once.
for p in patches/wiikit-*.patch; do
  git -C wiikit apply --reverse --check "../$p" 2>/dev/null || git -C wiikit apply "../$p"
done
python3 tools/names.py $B/extract/sys/main.dol
python3 -m wiikit.recomp $B/extract/sys/main.dol --out $B/recomp \
  --symbols $B/names.tsv --hooks tools/tls-hooks.txt
python3 tools/speed60.py $B/extract/sys/main.dol $B/recomp
cmake -S $B/recomp -B $B/recomp-build -G Ninja -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS_RELEASE=-O1 -DWIIKIT_EXTRA="$ROOT/tools/tls.cmake" ${CMAKE_ARGS:-}
ninja -C $B/recomp-build wiiboot
mkdir -p $B/run1 $B/nand
echo "Run: cd $B/run1 && ../recomp-build/wiiboot ../extract --symbols ../recomp/symbols.tsv --nand ../nand --fonts ../fonts --keys $ROOT/config/keys-confirm-a.txt"
