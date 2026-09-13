# Pipeline de Blender

## Cómo está pensado

Los assets no se modelan a mano: se **describen** en `pax_blender/config.py`
—cotas, secciones del monocasco, puntos del trazado— y el código los construye.
Cambiar el ancho del alerón trasero o el radio de una curva es cambiar un
número y volver a ejecutar el script.

Eso permite algo que con un `.blend` binario no se puede hacer: ver en un diff
qué cambió del coche entre dos commits.

```
Tools/Blender/
  build_assets.py          punto de entrada (CLI)
  pax_blender/
    config.py              cotas del coche y del circuito, conversión de ejes
    meshkit.py             primitivas: loft, tubo, rueda, perfil de alerón
    materials.py           materiales PBR
    car.py                 monta el monoplaza pieza a pieza
    rig.py                 esqueleto y pesos
    track.py               trazado, malla y CSV del eje de pista
    exporter.py            exportación FBX con los ajustes de Unreal
```

## Ejes y escala

Las dos cosas que hacen que un FBX entre mal.

**Ejes.** Blender es dextrógiro con −Y hacia delante; Unreal es levógiro con +X
hacia delante. Todo el código de geometría trabaja en *espacio de coche* (+X
adelante, +Y a la derecha, +Z arriba, como Unreal) y `config.bl()` traduce cada
punto al colocarlo:

```python
def bl(x, y, z):
    return (y, -x, z)
```

Así las cotas del archivo se leen igual que en el reglamento técnico y el
modelo queda en Blender mirando hacia −Y, que es lo que el exportador FBX con
sus ajustes por omisión más *Convert Scene* de Unreal convierten en +X.

**Escala.** Se modela en metros y se pone la escala de unidad de la escena en
0.01 antes de exportar (`exporter.configure_units()`). Con
`apply_unit_scale=True`, un metro de Blender sale como 100 unidades de Unreal.
Es preferible a multiplicar la geometría por 100 porque el factor se aplica
también al esqueleto.

## Esqueleto

Chaos Vehicles necesita una malla **esqueletal** con un hueso por rueda cuyo
nombre coincida con `WheelSetups` en `Source/Pax/Vehicle/F1Car.cpp`:

```
Root
└── Body            toda la carrocería, peso 1
    ├── Wheel_FL    rueda delantera izquierda, peso 1
    ├── Wheel_FR
    ├── Wheel_RL
    ├── Wheel_RR
    ├── DRS_Flap    flap móvil del alerón trasero
    └── Steering    volante
```

Los pesos son binarios a propósito: aquí no hay deformación orgánica, hay
piezas rígidas. Interpolar entre huesos sólo produciría el estirado clásico de
la rueda al girar.

`add_leaf_bones=False` en la exportación es obligatorio: si no, Blender añade
un hueso hoja al final de cada cadena y los nombres dejan de cuadrar.

## El coche, pieza a pieza

| Función de `car.py` | Qué construye |
|---|---|
| `build_monocoque` | Monocasco por *lofting* de doce secciones del morro al escape. |
| `build_floor` | Fondo plano y difusor. |
| `build_sidepods` | Pontones con entrada alta y estrechamiento. |
| `build_engine_cover` | Airbox, tapa de motor y aleta dorsal. |
| `build_front_wing` | Cuatro elementos en cascada, derivas y pilones. |
| `build_rear_wing` | Plano principal, derivas, pilón y **flap del DRS aparte**. |
| `build_barge_boards` | Deflectores del borde del fondo. |
| `build_halo` | Halo en tubo y su pilón central. |
| `build_driver` | Casco, visera y reposacabezas. |
| `build_suspension` | Trapecios y barras de dirección, en tubo. |
| `build_wheels` | Neumático, llanta y disco de freno por esquina. |

Las cotas siguen el reglamento vigente: 5,6 m de largo, 2,0 m de ancho,
3,6 m de batalla y ruedas de 18" con 720 mm de diámetro exterior.

### Por qué el flap del DRS va separado

Porque tiene que rotar solo. Su origen está en el borde de ataque, que es su
eje de giro real, así que rotar el hueso `DRS_Flap` abre el alerón igual que en
el coche. Si fuese parte de la carrocería habría que deformar la malla.

## El circuito

El trazado se define con quince puntos de control y se interpola con
Catmull-Rom, que pasa exactamente por ellos —mover un punto cambia el circuito
donde uno espera—. De las mismas muestras salen dos cosas que **deben**
coincidir:

* la malla que se ve (asfalto, pianos, escapatoria, muros), y
* el CSV del eje de pista que importa `ATrackSpline`.

Generarlas por separado produce el peor fallo posible en un juego de carreras:
un cronometraje desalineado respecto a lo que ve el jugador.

Los puntos de control de `config.TRACK` son **los mismos** que el trazado por
defecto de `ATrackSpline` (en metros aquí, en centímetros allí), así que el
proyecto es jugable antes incluso de importar nada.

El perfil de altura es una suma de armónicos de periodo entero, para que el
circuito cierre exactamente a la altura por la que empezó, desplazado para que
la línea de meta quede a cota cero.

## Repintar el coche

Los tres colores de `materials.py`:

```python
TEAM_PRIMARY   = (0.63, 0.05, 0.08, 1.0)
TEAM_SECONDARY = (0.02, 0.02, 0.03, 1.0)
TEAM_ACCENT    = (0.95, 0.78, 0.10, 1.0)
```

## Regenerar

```bash
blender --background --python Tools/Blender/build_assets.py -- --all
blender --background --python Tools/Blender/build_assets.py -- --car --out /tmp/pruebas
blender --background --python Tools/Blender/build_assets.py -- --track --no-blend
```

El script no abre ventana, así que sirve igual en una máquina de integración
continua.
