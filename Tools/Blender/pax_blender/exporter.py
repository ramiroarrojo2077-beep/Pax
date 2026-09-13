"""Exportación a FBX con los ajustes que espera Unreal Engine 5.

Dos detalles deciden si el asset entra bien o hay que pelearse con él:

**Escala.** Blender trabaja en metros y Unreal en centímetros. Poner la escala
de unidad de la escena en 0.01 y exportar con ``apply_unit_scale=True`` hace
que un metro de Blender salga como 100 unidades de Unreal, que es lo correcto.
Es preferible a multiplicar la geometría por 100 porque el factor se aplica
también al esqueleto y a cualquier animación, sin descuadres.

**Ejes.** Blender es dextrógiro con -Y hacia delante; Unreal es levógiro con +X
hacia delante. Se exporta con los ejes por omisión del exportador FBX
(``-Z`` delante, ``Y`` arriba) y se deja que el importador de Unreal haga la
conversión con "Convert Scene" activado. Por eso el modelo se construye mirando
hacia -Y en Blender (ver ``config.bl``): al llegar a Unreal mira hacia +X.

``add_leaf_bones=False`` es obligatorio: si no, Blender añade un hueso hoja al
final de cada cadena y el esqueleto que llega a Unreal no coincide con los
nombres de ``WheelSetups``.
"""

from __future__ import annotations

import os
from typing import Iterable

import bpy


def reset_scene() -> None:
    """Vacía la escena. Un script de generación debe partir siempre de cero."""
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)

    for collection in (bpy.data.meshes, bpy.data.armatures, bpy.data.materials,
                       bpy.data.curves, bpy.data.objects):
        for datablock in list(collection):
            if datablock.users == 0:
                collection.remove(datablock)


def configure_units() -> None:
    """Escala de unidad 0.01: un metro de Blender = 100 unidades de Unreal."""
    scene = bpy.context.scene
    scene.unit_settings.system = "METRIC"
    scene.unit_settings.scale_length = 0.01
    scene.unit_settings.length_unit = "CENTIMETERS"


def _select_only(objects: Iterable[bpy.types.Object]) -> None:
    bpy.ops.object.select_all(action="DESELECT")
    objects = [obj for obj in objects if obj is not None]
    for obj in objects:
        obj.select_set(True)
    if objects:
        bpy.context.view_layer.objects.active = objects[0]


def export_fbx(path: str, objects: Iterable[bpy.types.Object], skeletal: bool) -> str:
    """Exporta los objetos indicados a un FBX listo para importar."""
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
    _select_only(objects)

    object_types = {"MESH"}
    if skeletal:
        object_types.add("ARMATURE")

    bpy.ops.export_scene.fbx(
        filepath=path,
        use_selection=True,
        object_types=object_types,

        # Escala y ejes (ver docstring del módulo).
        apply_unit_scale=True,
        apply_scale_options="FBX_SCALE_NONE",
        global_scale=1.0,
        axis_forward="-Z",
        axis_up="Y",
        bake_space_transform=False,

        # Geometría.
        use_mesh_modifiers=True,
        mesh_smooth_type="FACE",
        use_tspace=True,
        use_triangles=False,

        # Esqueleto.
        add_leaf_bones=False,
        primary_bone_axis="Y",
        secondary_bone_axis="X",
        armature_nodetype="NULL",
        bake_anim=False,

        path_mode="COPY",
        embed_textures=False,
    )

    return path


def save_blend(path: str) -> str:
    """Guarda el .blend fuente junto al FBX, para poder retocar a mano."""
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=path)
    return path
