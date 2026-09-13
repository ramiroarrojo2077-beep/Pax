"""Construcción del monoplaza.

El coche se levanta pieza a pieza en espacio de coche (+X adelante, +Y a la
derecha, origen a ras de suelo en el centro de la batalla) y se agrupa en tres
conjuntos que Unreal necesita separados:

  * ``body``      — todo lo que va rígido al chasis.
  * ``wheels``    — cuatro objetos, uno por rueda, que giran cada uno con su hueso.
  * ``drs_flap``  — el flap móvil del alerón trasero, que rota al abrir el DRS.

Esa separación no es estética: es la que permite que el esqueleto tenga un
hueso por rueda —lo que exige Chaos Vehicles— y que el DRS se anime sin tocar
el resto de la carrocería.
"""

from __future__ import annotations

import math

import bpy

from . import materials, meshkit
from .config import CAR, CarSpec


# ---------------------------------------------------------------------------
# Chasis
# ---------------------------------------------------------------------------

def build_monocoque(spec: CarSpec) -> bpy.types.Object:
    """Monocasco completo, del morro a la salida de escape."""
    sections = []
    for x, half_width, z_bottom, z_top in spec.monocoque_sections:
        profile = meshkit.ring_profile(half_width, z_bottom, z_top, segments=14)
        sections.append(meshkit.section_at(x, profile))

    obj = meshkit.loft("Monocoque", sections)
    meshkit.bevel(obj, width=0.004, segments=2)
    meshkit.shade_smooth(obj, 50.0)
    materials.assign(obj, materials.livery_primary())
    return obj


def build_floor(spec: CarSpec) -> bpy.types.Object:
    """Fondo plano con difusor.

    El difusor es el que sube al final del fondo: la expansión del aire bajo el
    coche es de donde sale buena parte de la carga aerodinámica.
    """
    stations = (
        (spec.floor_front_x, spec.floor_half_width * 0.55, spec.floor_z, spec.floor_z + 0.04),
        (0.60, spec.floor_half_width, spec.floor_z, spec.floor_z + 0.05),
        (-1.00, spec.floor_half_width, spec.floor_z, spec.floor_z + 0.05),
        (spec.floor_rear_x, spec.floor_half_width * 0.95, spec.floor_z + 0.02, spec.floor_z + 0.08),
        (spec.diffuser_rear_x, spec.floor_half_width * 0.80,
         spec.floor_z + spec.diffuser_height * 0.55, spec.floor_z + spec.diffuser_height),
    )

    sections = []
    for x, half_width, z_bottom, z_top in stations:
        sections.append([
            (x, -half_width, z_bottom),
            (x, half_width, z_bottom),
            (x, half_width, z_top),
            (x, -half_width, z_top),
        ])

    obj = meshkit.loft("Floor", sections)
    meshkit.bevel(obj, width=0.005, segments=1)
    materials.assign(obj, materials.carbon())
    return obj


def build_sidepods(spec: CarSpec) -> list[bpy.types.Object]:
    """Pontones: entrada de aire alta y estrechamiento hacia atrás."""
    objects = []
    for side in (-1.0, 1.0):
        stations = (
            (spec.sidepod_front_x, 0.30, 0.62, 0.18, 0.58),
            (spec.sidepod_front_x - 0.45, 0.40, spec.sidepod_half_width, 0.14, spec.sidepod_top_z),
            (-0.30, 0.36, spec.sidepod_half_width * 0.95, 0.12, 0.54),
            (-0.95, 0.26, spec.sidepod_half_width * 0.60, 0.12, 0.44),
            (spec.sidepod_rear_x, 0.14, 0.30, 0.14, 0.34),
        )

        sections = []
        for x, inner, outer, z_bottom, z_top in stations:
            sections.append([
                (x, side * inner, z_bottom),
                (x, side * outer, z_bottom),
                (x, side * outer, z_top),
                (x, side * inner, z_top),
            ])

        name = "SidepodL" if side < 0 else "SidepodR"
        obj = meshkit.loft(name, sections)
        meshkit.bevel(obj, width=0.010, segments=2)
        meshkit.shade_smooth(obj, 45.0)
        materials.assign(obj, materials.livery_primary())
        objects.append(obj)

    return objects


def build_engine_cover(spec: CarSpec) -> list[bpy.types.Object]:
    """Airbox sobre la cabeza del piloto y tapa de motor en punta."""
    parts = []

    # Toma de aire: boca redonda encima del arco de seguridad.
    airbox_sections = []
    stations = (
        (0.10, 0.16, 0.68, 0.90),
        (-0.20, 0.19, 0.64, 0.86),
        (-0.70, 0.20, 0.58, 0.78),
        (-1.30, 0.17, 0.50, 0.66),
        (-1.90, 0.11, 0.42, 0.54),
        (-2.45, 0.05, 0.36, 0.44),
    )
    for x, half_width, z_bottom, z_top in stations:
        airbox_sections.append(meshkit.section_at(x, meshkit.ring_profile(half_width, z_bottom, z_top, 12)))

    cover = meshkit.loft("EngineCover", airbox_sections)
    meshkit.shade_smooth(cover, 50.0)
    materials.assign(cover, materials.livery_primary())
    parts.append(cover)

    # Aleta dorsal, la tira estrecha que va del airbox al alerón.
    fin = meshkit.box("SharkFin", center=(-1.60, 0.0, 0.60), size=(1.70, 0.02, 0.22))
    materials.assign(fin, materials.livery_secondary())
    parts.append(fin)

    return parts


# ---------------------------------------------------------------------------
# Aerodinámica
# ---------------------------------------------------------------------------

def build_front_wing(spec: CarSpec) -> list[bpy.types.Object]:
    """Alerón delantero: varios elementos escalonados más derivas."""
    parts = []

    for index in range(spec.front_wing_elements):
        # Cada elemento va un poco más arriba, más atrás y más inclinado que el
        # anterior: es la cascada característica del alerón delantero.
        fraction = index / max(spec.front_wing_elements - 1, 1)
        element = meshkit.wing_element(
            name=f"FrontWingElement{index}",
            x=spec.front_wing_x - fraction * 0.20,
            z=spec.front_wing_z + fraction * 0.12,
            half_span=spec.front_wing_half_width - fraction * 0.02,
            chord=spec.front_wing_chord * (1.0 - fraction * 0.45),
            thickness=0.030 - fraction * 0.005,
            angle_deg=-6.0 - fraction * 22.0,
            tip_drop=0.02,
        )
        meshkit.shade_smooth(element, 45.0)
        materials.assign(element, materials.carbon() if index % 2 else materials.livery_primary())
        parts.append(element)

    # Derivas: las placas verticales de los extremos que encauzan el aire por
    # fuera de la rueda delantera.
    for side in (-1.0, 1.0):
        endplate = meshkit.box(
            name=("FrontEndplateL" if side < 0 else "FrontEndplateR"),
            center=(spec.front_wing_x - 0.16, side * spec.front_wing_half_width, spec.front_wing_z + 0.10),
            size=(0.60, 0.022, spec.front_endplate_height),
        )
        meshkit.bevel(endplate, width=0.006, segments=2)
        materials.assign(endplate, materials.livery_accent())
        parts.append(endplate)

    # Pilones que cuelgan el alerón del morro.
    for side in (-1.0, 1.0):
        pylon = meshkit.tube(
            name=("NosePylonL" if side < 0 else "NosePylonR"),
            path=[(spec.front_wing_x - 0.10, side * 0.12, spec.front_wing_z + 0.06),
                  (spec.front_wing_x - 0.22, side * 0.09, 0.20),
                  (2.78, side * 0.05, 0.24)],
            radius=0.020,
            segments=8,
        )
        materials.assign(pylon, materials.carbon())
        parts.append(pylon)

    return parts


def build_rear_wing(spec: CarSpec) -> tuple[list[bpy.types.Object], bpy.types.Object]:
    """Alerón trasero. Devuelve (piezas fijas, flap móvil del DRS)."""
    parts = []

    main = meshkit.wing_element(
        name="RearWingMain",
        x=spec.rear_wing_x,
        z=spec.rear_wing_z,
        half_span=spec.rear_wing_half_width,
        chord=spec.rear_wing_chord,
        thickness=0.034,
        angle_deg=-16.0,
    )
    meshkit.shade_smooth(main, 45.0)
    materials.assign(main, materials.carbon())
    parts.append(main)

    for side in (-1.0, 1.0):
        endplate = meshkit.box(
            name=("RearEndplateL" if side < 0 else "RearEndplateR"),
            center=(spec.rear_wing_x - 0.10, side * spec.rear_wing_half_width, spec.rear_wing_z - 0.10),
            size=(0.70, 0.020, spec.rear_endplate_height),
        )
        meshkit.bevel(endplate, width=0.006, segments=2)
        materials.assign(endplate, materials.livery_primary())
        parts.append(endplate)

    # Pilón central de sujeción.
    pylon = meshkit.box(
        name="RearWingPylon",
        center=(spec.rear_wing_x - 0.08, 0.0, spec.rear_wing_z - 0.26),
        size=(0.34, 0.05, 0.40),
    )
    materials.assign(pylon, materials.carbon())
    parts.append(pylon)

    # El flap móvil se deja aparte y con su origen en el eje de giro, que es el
    # borde de ataque: así rotar el hueso abre el alerón como en el coche real.
    flap = meshkit.wing_element(
        name="DRSFlap",
        x=spec.rear_wing_x - 0.16,
        z=spec.rear_wing_z + spec.drs_flap_z_offset,
        half_span=spec.rear_wing_half_width - 0.01,
        chord=spec.drs_flap_chord,
        thickness=0.022,
        angle_deg=-34.0,
    )
    meshkit.shade_smooth(flap, 45.0)
    materials.assign(flap, materials.livery_accent())

    return parts, flap


def build_barge_boards(spec: CarSpec) -> list[bpy.types.Object]:
    """Deflectores del borde del fondo, delante de los pontones."""
    parts = []
    for side in (-1.0, 1.0):
        for index in range(3):
            fence = meshkit.box(
                name=f"FloorFence{'L' if side < 0 else 'R'}{index}",
                center=(1.15 - index * 0.22, side * (spec.floor_half_width - 0.04 - index * 0.05), 0.16),
                size=(0.36, 0.014, 0.20),
            )
            materials.assign(fence, materials.carbon())
            parts.append(fence)
    return parts


# ---------------------------------------------------------------------------
# Habitáculo
# ---------------------------------------------------------------------------

def build_halo(spec: CarSpec) -> list[bpy.types.Object]:
    """Halo: arco sobre la cabeza más el pilón central."""
    parts = []

    arc_points = []
    for step in range(13):
        angle = math.pi * step / 12.0
        # Arco achatado que rodea el habitáculo.
        x = spec.halo_x - 0.55 * (1.0 - math.cos(angle)) * 0.5 - 0.30 * math.sin(angle) * 0.0
        y = 0.36 * math.cos(angle)
        z = spec.halo_height - 0.10 * (1.0 - math.sin(angle))
        arc_points.append((x - 0.45 * math.sin(angle), y, z))

    halo = meshkit.tube("Halo", arc_points, radius=spec.halo_tube_radius, segments=8)
    meshkit.shade_smooth(halo, 60.0)
    materials.assign(halo, materials.titanium())
    parts.append(halo)

    pillar = meshkit.tube(
        "HaloPillar",
        path=[(spec.halo_x + 0.28, 0.0, 0.62), (spec.halo_x + 0.18, 0.0, spec.halo_height - 0.02)],
        radius=spec.halo_tube_radius * 1.2,
        segments=8,
    )
    materials.assign(pillar, materials.titanium())
    parts.append(pillar)

    return parts


def build_driver(spec: CarSpec) -> list[bpy.types.Object]:
    """Casco y reposacabezas: lo único del piloto que se ve desde fuera."""
    parts = []

    helmet = _sphere("Helmet", center=(spec.cockpit_x - 0.10, 0.0, 0.80), radius=0.135)
    meshkit.shade_smooth(helmet, 60.0)
    materials.assign(helmet, materials.livery_accent())
    parts.append(helmet)

    visor = meshkit.box("Visor", center=(spec.cockpit_x + 0.02, 0.0, 0.81), size=(0.06, 0.20, 0.07))
    materials.assign(visor, materials.visor())
    parts.append(visor)

    headrest = meshkit.box("Headrest", center=(spec.cockpit_x - 0.30, 0.0, 0.76), size=(0.26, 0.44, 0.18))
    meshkit.bevel(headrest, width=0.03, segments=2)
    materials.assign(headrest, materials.livery_secondary())
    parts.append(headrest)

    return parts


def build_mirrors(spec: CarSpec) -> list[bpy.types.Object]:
    parts = []
    for side in (-1.0, 1.0):
        mirror = meshkit.box(
            name=("MirrorL" if side < 0 else "MirrorR"),
            center=(spec.cockpit_x + 0.10, side * 0.42, 0.66),
            size=(0.09, 0.16, 0.07),
        )
        materials.assign(mirror, materials.livery_secondary())
        parts.append(mirror)

        stalk = meshkit.tube(
            name=("MirrorStalkL" if side < 0 else "MirrorStalkR"),
            path=[(spec.cockpit_x + 0.06, side * 0.30, 0.62), (spec.cockpit_x + 0.10, side * 0.40, 0.66)],
            radius=0.012,
            segments=6,
        )
        materials.assign(stalk, materials.carbon())
        parts.append(stalk)
    return parts


# ---------------------------------------------------------------------------
# Tren rodante
# ---------------------------------------------------------------------------

def build_wheels(spec: CarSpec) -> dict:
    """Las cuatro ruedas, cada una como objeto independiente.

    Clave = nombre del hueso que la mueve, para que el rigueo sea directo.
    """
    wheels = {}

    layout = (
        ("Wheel_FL", spec.front_axle_x, -spec.front_track * 0.5, spec.front_tyre_width),
        ("Wheel_FR", spec.front_axle_x, spec.front_track * 0.5, spec.front_tyre_width),
        ("Wheel_RL", spec.rear_axle_x, -spec.rear_track * 0.5, spec.rear_tyre_width),
        ("Wheel_RR", spec.rear_axle_x, spec.rear_track * 0.5, spec.rear_tyre_width),
    )

    for bone_name, x, y, width in layout:
        center = (x, y, spec.wheel_radius)

        tyre = meshkit.wheel(f"{bone_name}_Tyre", center, spec.wheel_radius, width,
                             spec.rim_radius, segments=30)
        meshkit.shade_smooth(tyre, 45.0)
        materials.assign(tyre, materials.rubber())

        rim = meshkit.wheel(f"{bone_name}_Rim", center, spec.rim_radius, width * 0.80,
                            spec.rim_radius * 0.35, segments=24)
        meshkit.shade_smooth(rim, 45.0)
        materials.assign(rim, materials.rim())

        # Disco de freno, algo más pequeño que la llanta y hacia el interior.
        disc_center = (x, y - math.copysign(width * 0.12, y), spec.wheel_radius)
        disc = meshkit.wheel(f"{bone_name}_Disc", disc_center, spec.rim_radius * 0.80,
                             0.032, spec.rim_radius * 0.30, segments=20)
        materials.assign(disc, materials.brake_disc())

        wheel_obj = meshkit.join_objects([tyre, rim, disc], bone_name)
        wheels[bone_name] = wheel_obj

    return wheels


def build_suspension(spec: CarSpec) -> list[bpy.types.Object]:
    """Trapecios y barras de dirección, en tubo.

    Van al cuerpo, no a las ruedas: en el modelo de Chaos la rueda gira y se
    desplaza sola, y unir los brazos a ella los haría girar con el neumático.
    """
    parts = []

    corners = (
        ("FL", spec.front_axle_x, -spec.front_track * 0.5, 0.24),
        ("FR", spec.front_axle_x, spec.front_track * 0.5, 0.24),
        ("RL", spec.rear_axle_x, -spec.rear_track * 0.5, 0.26),
        ("RR", spec.rear_axle_x, spec.rear_track * 0.5, 0.26),
    )

    for tag, x, y, chassis_half_width in corners:
        side = math.copysign(1.0, y)
        hub = (x, y - side * 0.14, spec.wheel_radius)

        # Triángulo superior e inferior, cada uno con dos brazos que van del
        # buje a dos puntos separados del chasis.
        for level, z_chassis, z_hub in (("Upper", 0.42, spec.wheel_radius + 0.10),
                                        ("Lower", 0.16, spec.wheel_radius - 0.12)):
            for offset in (0.22, -0.22):
                arm = meshkit.tube(
                    name=f"Wishbone{level}{tag}{'F' if offset > 0 else 'R'}",
                    path=[(x + offset, side * chassis_half_width, z_chassis),
                          (hub[0], hub[1], z_hub)],
                    radius=spec.wishbone_radius,
                    segments=6,
                )
                materials.assign(arm, materials.carbon())
                parts.append(arm)

        # Barra de dirección delante, tirante de convergencia detrás.
        track_rod = meshkit.tube(
            name=f"TrackRod{tag}",
            path=[(x - 0.18, side * chassis_half_width * 0.8, 0.28), (hub[0] - 0.16, hub[1], spec.wheel_radius)],
            radius=spec.wishbone_radius * 0.8,
            segments=6,
        )
        materials.assign(track_rod, materials.carbon())
        parts.append(track_rod)

    return parts


# ---------------------------------------------------------------------------
# Montaje
# ---------------------------------------------------------------------------

def _sphere(name: str, center: tuple, radius: float, rings: int = 10, segments: int = 16) -> bpy.types.Object:
    """Esfera por lofting de anillos de latitud."""
    cx, cy, cz = center
    sections = []
    for ring in range(1, rings):
        phi = math.pi * ring / rings
        ring_radius = radius * math.sin(phi)
        z = cz + radius * math.cos(phi)

        points = []
        for s in range(segments):
            theta = 2.0 * math.pi * s / segments
            points.append((cx + ring_radius * math.cos(theta),
                           cy + ring_radius * math.sin(theta),
                           z))
        sections.append(points)

    return meshkit.loft(name, sections, cap_start=True, cap_end=True)


def build_car(spec: CarSpec = CAR) -> dict:
    """Construye el coche completo.

    Devuelve ``{"body": Object, "wheels": {bone: Object}, "drs_flap": Object}``.
    """
    body_parts: list[bpy.types.Object] = []

    body_parts.append(build_monocoque(spec))
    body_parts.append(build_floor(spec))
    body_parts.extend(build_sidepods(spec))
    body_parts.extend(build_engine_cover(spec))
    body_parts.extend(build_front_wing(spec))
    body_parts.extend(build_barge_boards(spec))
    body_parts.extend(build_halo(spec))
    body_parts.extend(build_driver(spec))
    body_parts.extend(build_mirrors(spec))
    body_parts.extend(build_suspension(spec))

    rear_wing_parts, drs_flap = build_rear_wing(spec)
    body_parts.extend(rear_wing_parts)

    wheels = build_wheels(spec)

    # Los modificadores deben quedar aplicados antes de fundir las piezas:
    # el FBX exporta geometría, no historial.
    for part in body_parts + [drs_flap] + list(wheels.values()):
        meshkit.apply_modifiers(part)

    body = meshkit.join_objects(body_parts, "F1Car_Body")

    return {"body": body, "wheels": wheels, "drs_flap": drs_flap}
