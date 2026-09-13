"""Materiales PBR.

Se crean con nodos Principled BSDF y valores físicos razonables, de forma que
al importarlos en Unreal los materiales generados ya se parezcan a lo que
tienen que ser y sólo haya que sustituir las texturas.

La fibra de carbono desnuda es oscura pero no negra (un negro puro no existe en
la realidad y en pantalla se ve como un agujero) y bastante especular; la goma
del neumático es casi mate; la pintura de la carrocería lleva capa de barniz.
"""

from __future__ import annotations

import bpy


# Paleta del equipo. Cambiar estos tres colores es todo lo que hace falta para
# repintar el coche entero.
TEAM_PRIMARY = (0.63, 0.05, 0.08, 1.0)     # rojo
TEAM_SECONDARY = (0.02, 0.02, 0.03, 1.0)   # negro
TEAM_ACCENT = (0.95, 0.78, 0.10, 1.0)      # amarillo


def _principled(name: str, base_color, roughness: float, metallic: float = 0.0,
                clearcoat: float = 0.0, specular: float = 0.5) -> bpy.types.Material:
    """Crea (o reutiliza) un material Principled con los valores dados."""
    if name in bpy.data.materials:
        return bpy.data.materials[name]

    material = bpy.data.materials.new(name)
    material.use_nodes = True

    bsdf = material.node_tree.nodes.get("Principled BSDF")
    if bsdf is None:
        return material

    bsdf.inputs["Base Color"].default_value = base_color
    bsdf.inputs["Roughness"].default_value = roughness
    bsdf.inputs["Metallic"].default_value = metallic

    # Los nombres de los sockets de barniz y especular cambiaron en Blender 4.0.
    _set_input(bsdf, ("Coat Weight", "Clearcoat"), clearcoat)
    _set_input(bsdf, ("Specular IOR Level", "Specular"), specular)

    return material


def _set_input(node, names, value) -> None:
    for name in names:
        if name in node.inputs:
            node.inputs[name].default_value = value
            return


def carbon() -> bpy.types.Material:
    """Fibra de carbono vista, con barniz."""
    return _principled("PaxCarbon", (0.022, 0.022, 0.026, 1.0),
                       roughness=0.28, metallic=0.0, clearcoat=0.7, specular=0.6)


def livery_primary() -> bpy.types.Material:
    return _principled("PaxLiveryPrimary", TEAM_PRIMARY,
                       roughness=0.22, metallic=0.0, clearcoat=1.0, specular=0.55)


def livery_secondary() -> bpy.types.Material:
    return _principled("PaxLiverySecondary", TEAM_SECONDARY,
                       roughness=0.30, metallic=0.0, clearcoat=0.8)


def livery_accent() -> bpy.types.Material:
    return _principled("PaxLiveryAccent", TEAM_ACCENT,
                       roughness=0.25, metallic=0.0, clearcoat=0.9)


def rubber() -> bpy.types.Material:
    """Goma de slick: oscura, mate y sin reflejo metálico."""
    return _principled("PaxTyreRubber", (0.030, 0.030, 0.032, 1.0),
                       roughness=0.85, metallic=0.0, specular=0.25)


def rim() -> bpy.types.Material:
    """Llanta de magnesio mecanizada."""
    return _principled("PaxRim", (0.55, 0.56, 0.58, 1.0),
                       roughness=0.35, metallic=1.0)


def titanium() -> bpy.types.Material:
    """Halo y elementos estructurales."""
    return _principled("PaxTitanium", (0.42, 0.42, 0.45, 1.0),
                       roughness=0.42, metallic=1.0)


def brake_disc() -> bpy.types.Material:
    """Disco de carbono al rojo cuando se frena; aquí sólo el aspecto en frío."""
    return _principled("PaxBrakeDisc", (0.08, 0.075, 0.07, 1.0),
                       roughness=0.60, metallic=0.2)


def visor() -> bpy.types.Material:
    """Visera del casco, con reflejo dorado."""
    material = _principled("PaxVisor", (0.35, 0.26, 0.06, 1.0),
                           roughness=0.08, metallic=1.0)
    return material


def asphalt() -> bpy.types.Material:
    return _principled("PaxAsphalt", (0.055, 0.055, 0.060, 1.0),
                       roughness=0.82, metallic=0.0, specular=0.35)


def kerb() -> bpy.types.Material:
    """Piano. En Unreal se sustituye por un material con textura a rayas."""
    return _principled("PaxKerb", (0.72, 0.10, 0.10, 1.0), roughness=0.55)


def runoff() -> bpy.types.Material:
    return _principled("PaxRunoff", (0.28, 0.26, 0.23, 1.0), roughness=0.90)


def wall() -> bpy.types.Material:
    return _principled("PaxWall", (0.78, 0.78, 0.80, 1.0), roughness=0.65)


def assign(obj: bpy.types.Object, material: bpy.types.Material) -> None:
    """Asigna un único material a todas las caras del objeto."""
    obj.data.materials.clear()
    obj.data.materials.append(material)
    for polygon in obj.data.polygons:
        polygon.material_index = 0
