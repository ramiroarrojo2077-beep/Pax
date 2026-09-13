"""Genera los assets de Pax desde Blender, sin interfaz.

Uso::

    blender --background --python Tools/Blender/build_assets.py -- --all
    blender --background --python Tools/Blender/build_assets.py -- --car
    blender --background --python Tools/Blender/build_assets.py -- --track --out /ruta/salida

Deja en ``Tools/Blender/Build`` (o donde indique ``--out``):

    PaxF1Car.fbx          malla esqueletal del monoplaza, con sus seis huesos
    PaxF1Car.blend        fuente, por si hay que retocar a mano
    PaxTrack.fbx          malla del circuito (asfalto, pianos, escapatoria, muros)
    PaxTrack.blend
    track_centerline.csv  eje de pista en cm, que importa ATrackSpline

Después, en Unreal: importar los FBX en /Game/Pax y pulsar
"Import Centerline From CSV" en el actor del circuito.
"""

from __future__ import annotations

import argparse
import os
import sys

# Blender no añade el directorio del script al path: hay que hacerlo a mano
# para poder importar el paquete pax_blender.
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
if SCRIPT_DIR not in sys.path:
    sys.path.insert(0, SCRIPT_DIR)

from pax_blender import car, config, exporter, rig, track  # noqa: E402


def parse_args(argv: list[str]) -> argparse.Namespace:
    # Blender se come sus propios argumentos; los del script van detrás de "--".
    if "--" in argv:
        argv = argv[argv.index("--") + 1:]
    else:
        argv = []

    parser = argparse.ArgumentParser(description="Generador de assets de Pax")
    parser.add_argument("--car", action="store_true", help="genera el monoplaza")
    parser.add_argument("--track", action="store_true", help="genera el circuito")
    parser.add_argument("--all", action="store_true", help="genera todo")
    parser.add_argument("--out", default=None, help="directorio de salida")
    parser.add_argument("--no-blend", action="store_true", help="no guardar los .blend fuente")
    return parser.parse_args(argv)


def resolve_output_dir(explicit: str | None) -> str:
    if explicit:
        return os.path.abspath(explicit)
    project_root = os.path.abspath(os.path.join(SCRIPT_DIR, "..", ".."))
    return os.path.join(project_root, config.PATHS.build_dir.replace("/", os.sep))


def build_car(out_dir: str, save_blend: bool) -> None:
    print("[pax] generando monoplaza…")
    exporter.reset_scene()
    exporter.configure_units()

    parts = car.build_car()
    armature = rig.rig_car(parts)

    objects = [armature, parts["body"], parts["drs_flap"]] + list(parts["wheels"].values())

    fbx_path = os.path.join(out_dir, config.PATHS.car_fbx)
    exporter.export_fbx(fbx_path, objects, skeletal=True)
    print(f"[pax] FBX del coche: {fbx_path}")

    if save_blend:
        blend_path = os.path.join(out_dir, config.PATHS.car_blend)
        exporter.save_blend(blend_path)
        print(f"[pax] fuente: {blend_path}")


def build_track(out_dir: str, save_blend: bool) -> None:
    print("[pax] generando circuito…")
    exporter.reset_scene()
    exporter.configure_units()

    pieces = track.build_track()
    samples = pieces["samples"]

    length_m = track.track_length_m(samples)
    print(f"[pax] longitud del trazado: {length_m / 1000.0:.3f} km ({len(samples)} muestras)")

    fbx_path = os.path.join(out_dir, config.PATHS.track_fbx)
    exporter.export_fbx(fbx_path, [pieces["surface"], pieces["walls"]], skeletal=False)
    print(f"[pax] FBX del circuito: {fbx_path}")

    csv_path = os.path.join(out_dir, config.PATHS.centerline_csv)
    track.write_centerline_csv(csv_path, samples)
    print(f"[pax] eje de pista: {csv_path}")

    if save_blend:
        blend_path = os.path.join(out_dir, config.PATHS.track_blend)
        exporter.save_blend(blend_path)
        print(f"[pax] fuente: {blend_path}")


def main() -> int:
    args = parse_args(sys.argv)

    if not (args.car or args.track or args.all):
        print("Nada que hacer: usa --car, --track o --all.", file=sys.stderr)
        return 1

    out_dir = resolve_output_dir(args.out)
    os.makedirs(out_dir, exist_ok=True)
    print(f"[pax] salida: {out_dir}")

    save_blend = not args.no_blend

    if args.car or args.all:
        build_car(out_dir, save_blend)
    if args.track or args.all:
        build_track(out_dir, save_blend)

    print("[pax] listo.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
