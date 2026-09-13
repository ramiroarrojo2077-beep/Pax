# Pax

Juego de Fórmula 1 para **Unreal Engine 5** con los assets modelados de forma
procedural en **Blender**.

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
Docs/              puesta en marcha, arquitectura y pipeline de arte
```

## Puesta en marcha rápida

```bash
# 1. Generar coche y circuito (Blender 3.6 LTS o superior)
blender --background --python Tools/Blender/build_assets.py -- --all

# 2. Generar los proyectos de C++ y compilar
#    Windows: clic derecho en Pax.uproject -> Generate Visual Studio project files
#    Linux:   <Ruta_UE5>/GenerateProjectFiles.sh -project="$PWD/Pax.uproject" -game
```

Después, en el editor: importar los dos FBX de `Tools/Blender/Build` en
`/Game/Pax`, colocar un actor `TrackSpline` en el nivel y pulsar
**Import Centerline From CSV**. Los pasos completos están en
[Docs/SETUP.md](Docs/SETUP.md).

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

| Acción | Teclado | Mando |
|---|---|---|
| Acelerar / Frenar | `W` / `S` | Gatillos |
| Dirección | `A` / `D` | Stick izquierdo |
| Cambio arriba / abajo | `E` / `Q` | `RB` / `LB` |
| DRS | `Espacio` | `A` |
| Modo de ERS | `1` | `X` |
| Mezcla de combustible | `2` | `B` |
| Cámara | `C` | `Y` |
| Volver a pista | `R` | Start |

## Licencia

Código propio, sin assets de terceros.
