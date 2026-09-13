"""Generador procedural de assets para Pax.

El paquete construye en Blender el monoplaza y el circuito, los riguea con el
esqueleto que espera Chaos Vehicles y los exporta a FBX listos para importar en
Unreal Engine 5.

Se ejecuta siempre desde Blender en modo consola::

    blender --background --python Tools/Blender/build_assets.py -- --all

Nada aquí depende de la interfaz de Blender, así que el mismo script vale para
regenerar los assets en una máquina de integración continua.
"""

__all__ = [
    "config",
    "meshkit",
    "materials",
    "rig",
    "car",
    "track",
    "exporter",
]
