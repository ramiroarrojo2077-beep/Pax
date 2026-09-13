"""Esqueleto del monoplaza.

Chaos Vehicles no simula la carrocería a partir de la malla: necesita una malla
esqueletal con un hueso por rueda, cuyo nombre aparece en ``WheelSetups``
(Source/Pax/Vehicle/F1Car.cpp). El componente de movimiento coloca y gira esos
huesos, y la malla los sigue.

Por eso el rig es deliberadamente simple —seis huesos y pesos binarios— en
lugar de un skin suave: aquí no hay deformación orgánica que interpolar, hay
piezas rígidas que se mueven cada una con su hueso.

Los huesos se orientan con su eje largo hacia delante en espacio de coche, sin
"roll", para que coincidan con los ejes del componente en Unreal una vez hecha
la conversión de ejes del FBX.
"""

from __future__ import annotations

import bpy

from . import config
from .config import CAR, CarSpec, bl


def _add_bone(armature, name: str, head_car: tuple, length: float = 0.18,
              parent=None):
    """Crea un hueso que apunta hacia delante desde head_car."""
    bone = armature.edit_bones.new(name)
    bone.head = bl(*head_car)
    bone.tail = bl(head_car[0] + length, head_car[1], head_car[2])
    bone.roll = 0.0
    bone.use_connect = False
    if parent is not None:
        bone.parent = parent
    return bone


def build_armature(spec: CarSpec = CAR) -> bpy.types.Object:
    """Crea el esqueleto y lo deja en modo objeto."""
    armature_data = bpy.data.armatures.new("F1CarArmature")
    armature_object = bpy.data.objects.new("F1CarArmature", armature_data)
    bpy.context.scene.collection.objects.link(armature_object)

    bpy.context.view_layer.objects.active = armature_object
    bpy.ops.object.mode_set(mode="EDIT")

    root = _add_bone(armature_data, config.BONE_ROOT, (0.0, 0.0, 0.0), length=0.30)
    body = _add_bone(armature_data, config.BONE_BODY, (0.0, 0.0, 0.30), length=0.40, parent=root)

    wheel_positions = {
        "Wheel_FL": (spec.front_axle_x, -spec.front_track * 0.5, spec.wheel_radius),
        "Wheel_FR": (spec.front_axle_x, spec.front_track * 0.5, spec.wheel_radius),
        "Wheel_RL": (spec.rear_axle_x, -spec.rear_track * 0.5, spec.wheel_radius),
        "Wheel_RR": (spec.rear_axle_x, spec.rear_track * 0.5, spec.wheel_radius),
    }
    for name in config.BONE_WHEELS:
        _add_bone(armature_data, name, wheel_positions[name], length=0.20, parent=body)

    # El hueso del DRS tiene su origen en el borde de ataque del flap, que es
    # su eje de giro real: rotarlo sobre Y abre y cierra el alerón.
    _add_bone(armature_data, config.BONE_DRS,
              (spec.rear_wing_x - 0.16, 0.0, spec.rear_wing_z + spec.drs_flap_z_offset),
              length=0.15, parent=body)

    # Volante, para animar la dirección desde el Animation Blueprint.
    _add_bone(armature_data, config.BONE_STEERING,
              (spec.cockpit_x - 0.05, 0.0, 0.68), length=0.12, parent=body)

    bpy.ops.object.mode_set(mode="OBJECT")
    return armature_object


def bind(mesh_object: bpy.types.Object, armature_object: bpy.types.Object, bone_name: str) -> None:
    """Ata una malla entera a un solo hueso con peso 1.

    Peso binario a propósito: cada pieza es rígida y cualquier interpolación
    entre huesos produciría el estirado clásico de la rueda al girar.
    """
    group = mesh_object.vertex_groups.new(name=bone_name)
    group.add(range(len(mesh_object.data.vertices)), 1.0, "REPLACE")

    modifier = mesh_object.modifiers.new(name="Armature", type="ARMATURE")
    modifier.object = armature_object
    modifier.use_vertex_groups = True

    mesh_object.parent = armature_object
    mesh_object.matrix_parent_inverse = armature_object.matrix_world.inverted()


def rig_car(parts: dict, spec: CarSpec = CAR) -> bpy.types.Object:
    """Riguea el resultado de car.build_car y devuelve el objeto armadura."""
    armature_object = build_armature(spec)

    bind(parts["body"], armature_object, config.BONE_BODY)
    bind(parts["drs_flap"], armature_object, config.BONE_DRS)

    for bone_name, wheel_object in parts["wheels"].items():
        bind(wheel_object, armature_object, bone_name)

    return armature_object
