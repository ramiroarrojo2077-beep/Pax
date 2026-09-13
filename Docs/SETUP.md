# Puesta en marcha

## Requisitos

| Herramienta | Versión | Nota |
|---|---|---|
| Unreal Engine | 5.3 – 5.5 | 5.5 es la versión de referencia (`EngineAssociation` en `Pax.uproject`). |
| Blender | 3.6 LTS o superior | Sólo para regenerar los assets. |
| Compilador | Visual Studio 2022 (Windows) / Clang 16+ (Linux) | Con la carga de trabajo de C++ para juegos. |

Para generar el APK hacen falta además el SDK y el NDK de Android: eso está en
[ANDROID.md](ANDROID.md).

El módulo compila sin cambios en 5.1 y 5.2: `Pax.Build.cs` detecta la versión y
define `PAX_HAS_WHEEL_RUNTIME_API`, que elige entre el setter de fricción en
runtime (5.3+) y la ruta equivalente en versiones anteriores.

## 1. Generar los assets

```bash
blender --background --python Tools/Blender/build_assets.py -- --all
```

Deja en `Tools/Blender/Build/`:

```
PaxF1Car.fbx          monoplaza con esqueleto (6 huesos)
PaxF1Car.blend        fuente editable
PaxTrack.fbx          asfalto, pianos, escapatoria y muros
PaxTrack.blend
track_centerline.csv  eje de pista en cm, para la spline de juego
```

Se puede generar sólo una parte con `--car` o `--track`, y cambiar el destino
con `--out`.

## 2. Compilar

**Windows**: clic derecho en `Pax.uproject` → *Generate Visual Studio project
files*, abrir `Pax.sln` y compilar la configuración **Development Editor**.

**Linux**:

```bash
<Ruta_UE5>/GenerateProjectFiles.sh -project="$PWD/Pax.uproject" -game -engine
make PaxEditor
```

## 3. Importar en el editor

1. Abrir `Pax.uproject`.
2. Crear la carpeta `Content/Pax` e importar `PaxF1Car.fbx`:
   - *Skeletal Mesh*: **sí**
   - *Import Mesh*: **sí**, *Skeleton*: vacío (se crea uno nuevo)
   - *Convert Scene*: **sí**, *Force Front X Axis*: **no**
   - *Import Uniform Scale*: **1.0**
3. Crear un Blueprint derivado de `F1Car` (por ejemplo `/Game/Pax/BP_F1Car`),
   asignarle la malla esqueletal importada y su Animation Blueprint, y
   apuntarlo desde `Config/DefaultGame.ini`:

   ```ini
   [/Script/Pax.PaxGameMode]
   CarClass=/Game/Pax/BP_F1Car.BP_F1Car_C
   ```

   El sufijo `_C` es obligatorio: lo que se pide es la *clase* del Blueprint,
   no el asset. Sin esto el coche corre con su malla vacía —Chaos necesita los
   huesos de las ruedas para simular— y el log avisa al arrancar.
4. Crear un nivel. El template **Basic** vale tal cual: trae luz direccional,
   cielo, niebla y luz ambiental. Basta con borrar el suelo (`Floor`).

A partir de ahí, *Play* ya funciona: si el nivel no trae ningún `ATrackSpline`,
el GameMode crea uno con el trazado por defecto y el actor se construye su
propia calzada —asfalto, pianos, escapatoria, muros y colisión— a partir de la
spline. Después forma la parrilla, enciende el semáforo y arranca.

### Usar el circuito modelado en Blender

La calzada generada es geometría limpia pero desnuda. Para usar la del pipeline
de Blender:

1. Importar `PaxTrack.fbx` como *Static Mesh* con *Generate Collision*
   desactivado y, en el detalle de la malla, *Collision Complexity* →
   **Use Complex Collision As Simple**. Un circuito necesita colisión exacta;
   un casco convexo se comería los pianos.
2. Colocar la malla en el nivel, en el origen.
3. Añadir un actor **TrackSpline**, también en el origen, pulsar
   **Import Centerline From CSV** en su panel de detalles y desactivar
   `bBuildRuntimeMesh` para que no genere la suya encima.

## 4. Animation Blueprint del coche

El esqueleto exportado trae los huesos que Chaos necesita:

```
Root
└── Body
    ├── Wheel_FL   Wheel_FR   Wheel_RL   Wheel_RR
    ├── DRS_Flap
    └── Steering
```

Crear un Animation Blueprint sobre ese esqueleto con la clase padre
`VehicleAnimationInstance` y añadir el nodo **Wheel Controller for
WheeledVehicle**. Con eso las ruedas ya giran, dirigen y siguen la suspensión.

Para el DRS y el volante, encadenar dos nodos *Transform (Modify) Bone*:

| Hueso | Rotación | Fuente |
|---|---|---|
| `DRS_Flap` | Pitch de 0° a −50° | `GetDRSFlapAlpha()` del pawn |
| `Steering` | Roll proporcional | entrada de dirección |

## 5. Opciones de sesión

El GameMode lee opciones de la URL del mapa, así que se puede probar una
configuración sin tocar nada:

```
?Laps=20?Opponents=15?Difficulty=0.95
```

Los valores por defecto están en `Config/DefaultGame.ini`.

## Problemas frecuentes

**El coche entra en Unreal cien veces más pequeño (o más grande).**
La escala de unidad de la escena de Blender debe ser 0.01 al exportar; de eso
se encarga `exporter.configure_units()`. Si se exporta a mano desde la
interfaz, hay que ponerla antes.

**El coche mira hacia el lado equivocado.**
Se exporta con los ejes por omisión del FBX y *Convert Scene* activado en el
importador. Si se desactiva, el coche aparece girado 90°.

**Las ruedas no giran.**
Los nombres de hueso de `WheelSetups` en `F1Car.cpp` deben coincidir
exactamente con los del esqueleto importado: `Wheel_FL`, `Wheel_FR`,
`Wheel_RL`, `Wheel_RR`.

**El coche atraviesa el circuito o se hunde.**
La malla del circuito necesita *Use Complex Collision As Simple*.
