"""Utilidades de construcción de malla.

Todo se levanta con bmesh a partir de coordenadas explícitas en espacio de
coche. No se usan operadores de la interfaz (`bpy.ops.mesh.*`) porque dependen
del objeto activo y del modo, lo que los hace frágiles en un script sin
ventana; construir la malla a mano es más largo de escribir pero determinista.
"""

from __future__ import annotations

import math
from typing import Iterable, Sequence

import bmesh
import bpy

from .config import bl


# ---------------------------------------------------------------------------
# Creación de objetos
# ---------------------------------------------------------------------------

def new_mesh_object(name: str, verts: Sequence[tuple], faces: Sequence[Sequence[int]],
                    collection: bpy.types.Collection | None = None) -> bpy.types.Object:
    """Crea un objeto de malla a partir de vértices y caras ya convertidos."""
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(list(verts), [], [list(face) for face in faces])
    mesh.validate(verbose=False)
    mesh.update()

    obj = bpy.data.objects.new(name, mesh)
    target = collection or bpy.context.scene.collection
    target.objects.link(obj)
    return obj


def car_space_object(name: str, verts: Sequence[tuple], faces: Sequence[Sequence[int]],
                     collection: bpy.types.Collection | None = None) -> bpy.types.Object:
    """Igual que new_mesh_object pero traduciendo desde espacio de coche."""
    return new_mesh_object(name, [bl(*v) for v in verts], faces, collection)


# ---------------------------------------------------------------------------
# Primitivas
# ---------------------------------------------------------------------------

def box(name: str, center: tuple, size: tuple,
        collection: bpy.types.Collection | None = None) -> bpy.types.Object:
    """Caja alineada con los ejes. center y size en espacio de coche."""
    cx, cy, cz = center
    sx, sy, sz = (s * 0.5 for s in size)

    verts = [
        (cx - sx, cy - sy, cz - sz), (cx + sx, cy - sy, cz - sz),
        (cx + sx, cy + sy, cz - sz), (cx - sx, cy + sy, cz - sz),
        (cx - sx, cy - sy, cz + sz), (cx + sx, cy - sy, cz + sz),
        (cx + sx, cy + sy, cz + sz), (cx - sx, cy + sy, cz + sz),
    ]
    faces = [
        (0, 1, 2, 3),  # abajo
        (7, 6, 5, 4),  # arriba
        (0, 4, 5, 1),
        (1, 5, 6, 2),
        (2, 6, 7, 3),
        (3, 7, 4, 0),
    ]
    return car_space_object(name, verts, faces, collection)


def loft(name: str, sections: Sequence[Sequence[tuple]],
         cap_start: bool = True, cap_end: bool = True,
         collection: bpy.types.Collection | None = None) -> bpy.types.Object:
    """Une secciones transversales consecutivas formando un casco cerrado.

    Cada sección es una lista de puntos (x, y, z) en espacio de coche, todas
    con el mismo número de puntos y recorridas en el mismo sentido. Es la
    forma natural de describir un monocasco o un pontón: se dan los perfiles y
    la superficie sale sola.
    """
    if len(sections) < 2:
        raise ValueError("Un loft necesita al menos dos secciones")

    ring = len(sections[0])
    if any(len(section) != ring for section in sections):
        raise ValueError("Todas las secciones deben tener el mismo número de puntos")

    verts: list[tuple] = []
    for section in sections:
        verts.extend(section)

    faces: list[tuple] = []
    for s in range(len(sections) - 1):
        base_a = s * ring
        base_b = (s + 1) * ring
        for i in range(ring):
            j = (i + 1) % ring
            faces.append((base_a + i, base_a + j, base_b + j, base_b + i))

    if cap_start:
        faces.append(tuple(range(ring - 1, -1, -1)))
    if cap_end:
        base = (len(sections) - 1) * ring
        faces.append(tuple(base + i for i in range(ring)))

    return car_space_object(name, verts, faces, collection)


def ring_profile(half_width: float, z_bottom: float, z_top: float, segments: int = 12) -> list[tuple]:
    """Perfil ovalado de una sección del monocasco, en el plano YZ.

    Devuelve puntos (y, z) recorridos en sentido antihorario. El óvalo aplasta
    la parte inferior, que es como se ve de verdad la sección de un monocasco:
    plano abajo por el fondo y redondeado arriba.
    """
    cz = (z_top + z_bottom) * 0.5
    rz = (z_top - z_bottom) * 0.5

    points = []
    for i in range(segments):
        angle = 2.0 * math.pi * i / segments
        y = half_width * math.cos(angle)
        z = cz + rz * math.sin(angle)
        # Aplana la parte de abajo hacia el plano del fondo.
        if math.sin(angle) < 0.0:
            z = cz + rz * math.sin(angle) * 0.75
        points.append((y, z))
    return points


def section_at(x: float, profile: Sequence[tuple]) -> list[tuple]:
    """Coloca un perfil (y, z) en una estación x concreta."""
    return [(x, y, z) for (y, z) in profile]


def tube(name: str, path: Sequence[tuple], radius: float, segments: int = 8,
         collection: bpy.types.Collection | None = None) -> bpy.types.Object:
    """Tubo de sección circular siguiendo una polilínea.

    Se usa para el halo, los brazos de suspensión y los soportes de alerón:
    piezas que son tubos y quedan mal como cajas.
    """
    if len(path) < 2:
        raise ValueError("Un tubo necesita al menos dos puntos")

    sections: list[list[tuple]] = []
    for index, point in enumerate(path):
        # Dirección local del recorrido en este punto.
        if index == 0:
            direction = _sub(path[1], path[0])
        elif index == len(path) - 1:
            direction = _sub(path[-1], path[-2])
        else:
            direction = _sub(path[index + 1], path[index - 1])

        forward = _normalize(direction)
        right, up = _frame(forward)

        ring = []
        for s in range(segments):
            angle = 2.0 * math.pi * s / segments
            offset = _add(_scale(right, radius * math.cos(angle)),
                          _scale(up, radius * math.sin(angle)))
            ring.append(_add(point, offset))
        sections.append(ring)

    return loft(name, sections, cap_start=True, cap_end=True, collection=collection)


def wheel(name: str, center: tuple, radius: float, width: float,
          rim_radius: float, segments: int = 28,
          collection: bpy.types.Collection | None = None) -> bpy.types.Object:
    """Neumático con llanta, como un cilindro hueco con hombros redondeados.

    El eje de giro es Y (transversal), que es el que espera Unreal para una
    rueda cuyo hueso mira hacia delante.
    """
    cx, cy, cz = center
    half = width * 0.5

    # Cuatro anillos: hombro exterior, banda de rodadura y hombro interior.
    # El escalón entre hombro y banda es lo que da la silueta de un slick.
    stations = (
        (-half, rim_radius),
        (-half * 0.92, radius * 0.97),
        (-half * 0.75, radius),
        (half * 0.75, radius),
        (half * 0.92, radius * 0.97),
        (half, rim_radius),
    )

    sections = []
    for offset, ring_radius in stations:
        ring = []
        for i in range(segments):
            angle = 2.0 * math.pi * i / segments
            ring.append((cx + ring_radius * math.cos(angle),
                         cy + offset,
                         cz + ring_radius * math.sin(angle)))
        sections.append(ring)

    return loft(name, sections, cap_start=True, cap_end=True, collection=collection)


def wing_element(name: str, x: float, z: float, half_span: float, chord: float,
                 thickness: float, angle_deg: float, tip_drop: float = 0.0,
                 collection: bpy.types.Collection | None = None) -> bpy.types.Object:
    """Un elemento de alerón: perfil con curvatura, ángulo de ataque y caída en punta.

    No es un perfil aerodinámico exacto —para lo que se ve en pantalla no hace
    falta—, pero sí tiene borde de ataque redondeado, borde de fuga afilado y
    combadura, que es lo que hace que se lea como un alerón y no como una tabla.
    """
    angle = math.radians(angle_deg)

    # Perfil en el plano XZ, desde el borde de ataque al de fuga.
    # (fracción de cuerda, desplazamiento vertical relativo al espesor)
    camber = (
        (0.00, 0.00),
        (0.15, 0.55),
        (0.40, 1.00),
        (0.70, 0.80),
        (1.00, 0.10),
    )

    sections = []
    for side in (-1.0, 1.0):
        y = side * half_span
        drop = tip_drop * abs(side)

        ring = []
        # Cara superior de borde de ataque a borde de fuga...
        for frac, camber_scale in camber:
            local_x = -frac * chord
            local_z = thickness * camber_scale * 0.5
            ring.append(_rotate_xz(x, z - drop, local_x, local_z, angle, y))
        # ...y cara inferior de vuelta.
        for frac, camber_scale in reversed(camber[1:-1]):
            local_x = -frac * chord
            local_z = -thickness * camber_scale * 0.25
            ring.append(_rotate_xz(x, z - drop, local_x, local_z, angle, y))

        sections.append(ring)

    return loft(name, sections, cap_start=True, cap_end=True, collection=collection)


def _rotate_xz(origin_x: float, origin_z: float, local_x: float, local_z: float,
               angle: float, y: float) -> tuple:
    """Gira un punto local del perfil alrededor del borde de ataque."""
    rx = local_x * math.cos(angle) - local_z * math.sin(angle)
    rz = local_x * math.sin(angle) + local_z * math.cos(angle)
    return (origin_x + rx, y, origin_z + rz)


# ---------------------------------------------------------------------------
# Acabado
# ---------------------------------------------------------------------------

def shade_smooth(obj: bpy.types.Object, angle_deg: float = 40.0) -> None:
    """Sombreado suave por cara, con corte por ángulo donde el motor lo soporte.

    Blender 4.1 retiró `use_auto_smooth` y lo sustituyó por un nodo opcional.
    Como el FBX se exporta con las normales ya calculadas, marcar las caras como
    suaves es suficiente en ambas versiones; el corte por ángulo sólo se aplica
    si la propiedad antigua sigue existiendo.
    """
    for polygon in obj.data.polygons:
        polygon.use_smooth = True

    mesh = obj.data
    if hasattr(mesh, "use_auto_smooth"):
        mesh.use_auto_smooth = True
        mesh.auto_smooth_angle = math.radians(angle_deg)


def bevel(obj: bpy.types.Object, width: float = 0.006, segments: int = 2) -> None:
    """Bisel de arista. Le da a la fibra de carbono el brillo que la delata."""
    modifier = obj.modifiers.new(name="Bevel", type="BEVEL")
    modifier.width = width
    modifier.segments = segments
    modifier.limit_method = "ANGLE"
    modifier.angle_limit = math.radians(45.0)


def apply_modifiers(obj: bpy.types.Object) -> None:
    """Aplica los modificadores: el FBX debe salir con la geometría definitiva."""
    depsgraph = bpy.context.evaluated_depsgraph_get()
    evaluated = obj.evaluated_get(depsgraph)
    mesh = bpy.data.meshes.new_from_object(evaluated)

    obj.modifiers.clear()
    old_mesh = obj.data
    obj.data = mesh
    if old_mesh.users == 0:
        bpy.data.meshes.remove(old_mesh)


def join_objects(objects: Iterable[bpy.types.Object], name: str) -> bpy.types.Object:
    """Funde varios objetos en uno solo conservando sus materiales."""
    objects = [obj for obj in objects if obj is not None]
    if not objects:
        raise ValueError("No hay nada que unir")

    target = objects[0]
    target.name = name

    bm = bmesh.new()
    material_remap: dict[str, int] = {}

    for index, material in enumerate(target.data.materials):
        material_remap[material.name] = index

    for obj in objects:
        mesh = obj.data
        # Índice de material de esta malla dentro del objeto destino.
        local_map = {}
        for local_index, material in enumerate(mesh.materials):
            if material.name not in material_remap:
                target.data.materials.append(material)
                material_remap[material.name] = len(target.data.materials) - 1
            local_map[local_index] = material_remap[material.name]

        temp = bmesh.new()
        temp.from_mesh(mesh)
        temp.transform(obj.matrix_world)

        # Mapa vértice original -> vértice nuevo. Usar .index no es fiable
        # después de from_mesh, así que se guarda la correspondencia directa.
        vertex_map = {vertex: bm.verts.new(vertex.co) for vertex in temp.verts}
        bm.verts.ensure_lookup_table()

        for face in temp.faces:
            try:
                new_face = bm.faces.new([vertex_map[vertex] for vertex in face.verts])
            except ValueError:
                # Cara duplicada tras la fusión: se descarta sin ruido.
                continue
            new_face.material_index = local_map.get(face.material_index, 0)
            new_face.smooth = face.smooth

        temp.free()

    bm.to_mesh(target.data)
    bm.free()
    target.data.update()

    for obj in objects[1:]:
        bpy.data.objects.remove(obj, do_unlink=True)

    target.matrix_world.identity()
    return target


# ---------------------------------------------------------------------------
# Vectores
# ---------------------------------------------------------------------------

def _sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def _add(a, b):
    return (a[0] + b[0], a[1] + b[1], a[2] + b[2])


def _scale(a, k):
    return (a[0] * k, a[1] * k, a[2] * k)


def _normalize(a):
    length = math.sqrt(a[0] ** 2 + a[1] ** 2 + a[2] ** 2)
    if length < 1e-9:
        return (1.0, 0.0, 0.0)
    return (a[0] / length, a[1] / length, a[2] / length)


def _frame(forward):
    """Dos vectores perpendiculares a `forward`, para barrer una sección."""
    reference = (0.0, 0.0, 1.0)
    if abs(forward[2]) > 0.95:
        reference = (1.0, 0.0, 0.0)

    right = _normalize(_cross(forward, reference))
    up = _cross(right, forward)
    return right, up


def _cross(a, b):
    return (a[1] * b[2] - a[2] * b[1],
            a[2] * b[0] - a[0] * b[2],
            a[0] * b[1] - a[1] * b[0])
