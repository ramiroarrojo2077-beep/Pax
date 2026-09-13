"""Generación del circuito.

El trazado se define con unos pocos puntos de control y se interpola con una
spline de Catmull-Rom, que pasa exactamente por ellos: mover un punto cambia el
circuito donde uno espera, sin los desplazamientos que introduce una Bézier.

De la misma muestra salen dos cosas que deben coincidir sí o sí:

  * la malla que se ve (asfalto, pianos, escapatoria y muros), y
  * el CSV del eje de pista que importa ``ATrackSpline`` en Unreal.

Si se generasen por separado, el cronometraje y la detección de límites de
pista acabarían desalineados respecto a lo que ve el jugador, que es el fallo
más difícil de diagnosticar en un juego de carreras.
"""

from __future__ import annotations

import math
import os
from typing import Sequence

import bpy

from . import materials, meshkit
from .config import TRACK, TrackSpec


class Sample:
    """Un punto del eje de pista con su marco local."""

    __slots__ = ("x", "y", "z", "nx", "ny", "t")

    def __init__(self, x: float, y: float, z: float, nx: float, ny: float, t: float):
        self.x = x
        self.y = y
        self.z = z
        self.nx = nx  # normal horizontal, hacia la derecha del sentido de marcha
        self.ny = ny
        self.t = t    # posición normalizada en la vuelta [0, 1)

    def offset(self, lateral: float, height: float = 0.0) -> tuple:
        return (self.x + self.nx * lateral, self.y + self.ny * lateral, self.z + height)


def _catmull_rom(p0, p1, p2, p3, t: float) -> tuple:
    """Interpolación de Catmull-Rom uniforme entre p1 y p2."""
    t2 = t * t
    t3 = t2 * t
    return (
        0.5 * ((2 * p1[0]) + (-p0[0] + p2[0]) * t
               + (2 * p0[0] - 5 * p1[0] + 4 * p2[0] - p3[0]) * t2
               + (-p0[0] + 3 * p1[0] - 3 * p2[0] + p3[0]) * t3),
        0.5 * ((2 * p1[1]) + (-p0[1] + p2[1]) * t
               + (2 * p0[1] - 5 * p1[1] + 4 * p2[1] - p3[1]) * t2
               + (-p0[1] + 3 * p1[1] - 3 * p2[1] + p3[1]) * t3),
    )


def _elevation(t: float) -> float:
    """Perfil de altura de la vuelta, en metros.

    Suma de armónicos de periodo entero para que el circuito cierre a la misma
    altura por la que empezó: si no, el último tramo tendría un escalón.
    Se resta el valor en t=0 para que la línea de meta quede a cota cero, que
    es donde ATrackSpline coloca la parrilla si aún no se ha importado el CSV.
    """
    def harmonics(u: float) -> float:
        return (6.0 * math.sin(2.0 * math.pi * u)
                + 3.0 * math.sin(4.0 * math.pi * u + 1.1)
                + 1.5 * math.sin(6.0 * math.pi * u + 0.4))

    return harmonics(t) - harmonics(0.0)


def sample_centerline(spec: TrackSpec = TRACK) -> list[Sample]:
    """Discretiza el eje de pista en puntos con normal y altura."""
    control = list(spec.control_points)
    count = len(control)
    samples: list[Sample] = []

    raw: list[tuple] = []
    for index in range(count):
        p0 = control[(index - 1) % count]
        p1 = control[index]
        p2 = control[(index + 1) % count]
        p3 = control[(index + 2) % count]

        for step in range(spec.samples_per_segment):
            raw.append(_catmull_rom(p0, p1, p2, p3, step / spec.samples_per_segment))

    total = len(raw)
    for index, (x, y) in enumerate(raw):
        nxt = raw[(index + 1) % total]
        prv = raw[(index - 1) % total]

        # Tangente por diferencias centradas y normal girándola 90°.
        tx, ty = nxt[0] - prv[0], nxt[1] - prv[1]
        length = math.hypot(tx, ty) or 1.0
        tx, ty = tx / length, ty / length
        nx, ny = ty, -tx

        t = index / total
        samples.append(Sample(x, y, _elevation(t), nx, ny, t))

    return samples


# ---------------------------------------------------------------------------
# Mallas
# ---------------------------------------------------------------------------

def _ribbon(name: str, samples: Sequence[Sample], inner: float, outer: float,
            inner_height: float = 0.0, outer_height: float = 0.0) -> bpy.types.Object:
    """Cinta cerrada entre dos offsets laterales del eje."""
    verts: list[tuple] = []
    for sample in samples:
        verts.append(sample.offset(inner, inner_height))
        verts.append(sample.offset(outer, outer_height))

    faces: list[tuple] = []
    count = len(samples)
    for index in range(count):
        a = index * 2
        b = a + 1
        c = ((index + 1) % count) * 2
        d = c + 1
        faces.append((a, b, d, c))

    return meshkit.car_space_object(name, verts, faces)


def _wall(name: str, samples: Sequence[Sample], lateral: float, height: float) -> bpy.types.Object:
    """Muro vertical a un lado del trazado."""
    verts: list[tuple] = []
    for sample in samples:
        verts.append(sample.offset(lateral, 0.0))
        verts.append(sample.offset(lateral, height))

    faces: list[tuple] = []
    count = len(samples)
    for index in range(count):
        a = index * 2
        b = a + 1
        c = ((index + 1) % count) * 2
        d = c + 1
        faces.append((a, b, d, c))

    return meshkit.car_space_object(name, verts, faces)


def build_track(spec: TrackSpec = TRACK) -> dict:
    """Construye la geometría del circuito y la devuelve por piezas."""
    samples = sample_centerline(spec)

    asphalt = _ribbon("TrackAsphalt", samples, -spec.half_width, spec.half_width)
    materials.assign(asphalt, materials.asphalt())

    # Los pianos suben ligeramente hacia fuera: es lo que hace que subirse a
    # ellos desestabilice el coche en lugar de ser terreno libre.
    kerb_left = _ribbon("TrackKerbLeft", samples,
                        -spec.half_width, -(spec.half_width + spec.kerb_width),
                        0.0, spec.kerb_height)
    materials.assign(kerb_left, materials.kerb())

    kerb_right = _ribbon("TrackKerbRight", samples,
                         spec.half_width, spec.half_width + spec.kerb_width,
                         0.0, spec.kerb_height)
    materials.assign(kerb_right, materials.kerb())

    runoff_left = _ribbon("TrackRunoffLeft", samples,
                          -(spec.half_width + spec.kerb_width),
                          -(spec.half_width + spec.kerb_width + spec.runoff_width),
                          spec.kerb_height, 0.0)
    materials.assign(runoff_left, materials.runoff())

    runoff_right = _ribbon("TrackRunoffRight", samples,
                           spec.half_width + spec.kerb_width,
                           spec.half_width + spec.kerb_width + spec.runoff_width,
                           spec.kerb_height, 0.0)
    materials.assign(runoff_right, materials.runoff())

    edge = spec.half_width + spec.kerb_width + spec.runoff_width
    wall_left = _wall("TrackWallLeft", samples, -edge, spec.wall_height)
    materials.assign(wall_left, materials.wall())
    wall_right = _wall("TrackWallRight", samples, edge, spec.wall_height)
    materials.assign(wall_right, materials.wall())

    surface = meshkit.join_objects(
        [asphalt, kerb_left, kerb_right, runoff_left, runoff_right], "Track_Surface")
    walls = meshkit.join_objects([wall_left, wall_right], "Track_Walls")

    return {"surface": surface, "walls": walls, "samples": samples}


# ---------------------------------------------------------------------------
# Exportación del eje para Unreal
# ---------------------------------------------------------------------------

def write_centerline_csv(path: str, samples: Sequence[Sample], spec: TrackSpec = TRACK) -> str:
    """Escribe el eje de pista en centímetros, que es la unidad de Unreal.

    El formato es el que lee ATrackSpline::ImportCenterlineFromCSV:
    ``x,y,z,width`` con una fila por punto de control.
    """
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)

    # No hace falta un punto de spline por cada muestra de la malla: la spline
    # de Unreal interpola sola, y meterle cientos de puntos sólo la hace más
    # cara de evaluar. Uno de cada cuatro conserva de sobra la forma.
    stride = 4
    width_cm = spec.half_width * 2.0 * 100.0

    lines = ["# Eje de pista generado por Tools/Blender/build_assets.py",
             "# unidades: centímetros, espacio de Unreal (X adelante, Y derecha, Z arriba)",
             "x,y,z,width"]

    for index in range(0, len(samples), stride):
        sample = samples[index]
        lines.append(f"{sample.x * 100.0:.2f},{sample.y * 100.0:.2f},{sample.z * 100.0:.2f},{width_cm:.2f}")

    with open(path, "w", encoding="utf-8") as handle:
        handle.write("\n".join(lines) + "\n")

    return path


def track_length_m(samples: Sequence[Sample]) -> float:
    """Longitud del trazado, útil para comprobar que el circuito tiene sentido."""
    total = 0.0
    for index in range(len(samples)):
        a = samples[index]
        b = samples[(index + 1) % len(samples)]
        total += math.dist((a.x, a.y, a.z), (b.x, b.y, b.z))
    return total
