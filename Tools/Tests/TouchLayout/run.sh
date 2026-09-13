#!/usr/bin/env bash
# Compila y ejecuta PaxTouchLayout.cpp —el archivo real del juego— contra un
# shim mínimo de los tipos de Unreal, y comprueba que el mando en pantalla es
# usable en varias resoluciones de móvil y tableta.
#
# Sin esto, la única forma de ver que dos botones se solapan es empaquetar un
# APK, instalarlo y tocarlo con el dedo.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
UI="$HERE/../../../Source/Pax/UI"
OUT="${TMPDIR:-/tmp}/pax_touchlayout_test"

"${CXX:-clang++}" -std=c++17 -Wall \
    -I "$HERE/shim" -I "$UI" \
    -o "$OUT" \
    "$HERE/TestTouchLayout.cpp" "$UI/PaxTouchLayout.cpp"

"$OUT"
