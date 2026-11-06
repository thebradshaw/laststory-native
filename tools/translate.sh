#!/usr/bin/env bash
# Translate your own main.dol into C++ with Wiicompiled.
# Usage: tools/translate.sh /path/to/extracted/sys/main.dol
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WC="$ROOT/external/Wiicompiled"
DOL="$(realpath "${1:?path to main.dol}")"
mkdir -p "$WC/projects"
ln -sfn "$ROOT/projects/laststory" "$WC/projects/laststory"
dotnet build "$WC/translator/src/Translator.Cli/Translator.Cli.csproj" -c Release
export RECOMP_GENERIC_DOL="$DOL" DOTNET_ROLL_FORWARD=Major
dotnet "$WC/translator/src/Translator.Cli/bin/Release/net8.0/Translator.Cli.dll" \
  translate-recursive 0x80004050 --project "$WC/projects/laststory/recomp.yml"
echo "Output: $WC/build/laststory"
