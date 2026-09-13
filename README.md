# Pax

Juego de Fórmula 1 para **Android** (APK) hecho con **Unreal Engine 5**, con los
assets modelados de forma procedural en **Blender**. También se juega en PC con
teclado o mando.

El proyecto es código: no hay ni un solo asset binario en el repositorio. El
coche y el circuito se generan con scripts de Blender y se exportan a FBX; la
lógica de juego, la física, la IA, el input y la HUD están en C++. Eso hace que
todo el juego se pueda revisar en un diff y regenerar desde cero con un comando.

```
Pax.uproject
Config/            ajustes de motor, física, input y proyecto
Source/Pax/
  Core/            GameMode, GameState (cronometraje), PlayerController
  Vehicle/         el monoplaza, sus ruedas y sus sistemas
  Track/           descripción geométrica del circuito
  AI/              piloto artificial
  UI/              panel de información del piloto
Tools/Blender/     generador procedural de coche y circuito
Tools/Tests/       pruebas que no necesitan ni Unreal ni Blender
Config/Android/    ajustes de render, física y sesión para móvil
Docs/              puesta en marcha, empaquetado del APK, arquitectura, pipeline
```

## Puesta en marcha rápida

```bash
# 1. Generar coche y circuito (Blender 3.6 LTS o superior)
blender --background --python Tools/Blender/build_assets.py -- --all

# 2. Generar los proyectos de C++ y compilar
#    Windows: clic derecho en Pax.uproject -> Generate Visual Studio project files
#    Linux:   <Ruta_UE5>/GenerateProjectFiles.sh -project="$PWD/Pax.uproject" -game
```

```bash
# 3. Empaquetar el APK
<Ruta_UE5>/Engine/Build/BatchFiles/RunUAT.sh BuildCookRun \
  -project="$PWD/Pax.uproject" -platform=Android -cookflavor=ASTC \
  -clientconfig=Development -build -cook -stage -package -pak \
  -archive -archivedirectory="$PWD/Build"

adb install -r Build/Android/Pax-Android-Development-arm64.apk
```

Antes del paso 3 hay que hacer dos cosas una vez en el editor: importar el FBX
del coche y crear su Blueprint, y guardar un nivel (vale el template **Basic**
sin el suelo). El **circuito no hace falta colocarlo**: si el nivel no trae
ninguno, el juego crea uno y se construye su propia calzada con colisión.

Los pasos completos están en [Docs/SETUP.md](Docs/SETUP.md) y
[Docs/ANDROID.md](Docs/ANDROID.md).

## Qué está modelado

No es un arcade con un multiplicador de velocidad: la física de Chaos Vehicles
lleva motor, caja y contacto rueda-suelo, y encima van los sistemas que hacen
que un F1 se conduzca como un F1.

| Sistema | Qué aporta |
|---|---|
| **Aerodinámica** | Carga y resistencia cuadráticas con la velocidad, aplicadas en los dos ejes por separado. Efecto suelo, rebufo y aire sucio. |
| **Neumáticos** | Temperatura, ventana térmica, degradación no lineal y planos por bloqueo. Cinco compuestos con su compromiso ritmo/duración. |
| **ERS** | 120 kW de despliegue con un cupo de 4 MJ por vuelta, recuperación en frenada y tres modos. |
| **Combustible** | Tres mapas de motor y masa variable: el coche del final de carrera no es el de la salida. |
| **DRS** | Punto de detección, diferencia de un segundo y cierre automático al frenar. |
| **Cronometraje** | Vueltas, sectores, diferencias, vuelta rápida y sanciones por límites de pista. |
| **IA** | Calcula su propia velocidad de paso por curva y su punto de frenada; adelanta, defiende y gestiona su energía. |

## Controles

| Acción | Móvil | Teclado | Mando |
|---|---|---|---|
| Acelerar / Frenar | Pedales abajo a la derecha | `W` / `S` | Gatillos |
| Dirección | Volante flotante, mitad inferior izquierda | `A` / `D` | Stick izquierdo |
| Cambio arriba / abajo | Levas sobre los pedales | `E` / `Q` | `RB` / `LB` |
| DRS | Botón ancho de la fila | `Espacio` | `A` |
| Modo de ERS | `ERS` | `1` | `X` |
| Mezcla de combustible | `MIX` | `2` | `B` |
| Cámara | `CAM` | `C` | `Y` |
| Volver a pista | `REC` | `R` | Start |

En móvil el volante se ancla donde apoyas el pulgar, así no hay que mirar la
pantalla para encontrarlo. También se puede conducir inclinando el teléfono
(`SteeringMode=Tilt`). El mando en pantalla lo dibuja la propia HUD: no hay ni
un asset de interfaz en el proyecto.

## Pruebas

```bash
python3 Tools/Tests/test_track_geometry.py   # geometría del circuito y CSV
Tools/Tests/TouchLayout/run.sh               # mando en pantalla
```

Las dos verifican los archivos reales del juego sin necesidad de tener Unreal
ni Blender instalados. Detalles en [Tools/Tests/README.md](Tools/Tests/README.md).

## Licencia

Código propio, sin assets de terceros.
