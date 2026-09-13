# Empaquetar el APK

## Qué hace falta instalar

Unreal no compila para Android con lo que trae de serie: necesita el SDK, el
NDK y un JDK, y es muy quisquilloso con las versiones.

| Componente | Versión para UE 5.5 |
|---|---|
| Android Studio | Flamingo o superior (sirve sólo para instalar el SDK) |
| SDK Platform | API 34 |
| SDK Build-Tools | 34.0.0 |
| NDK | 25.1.8937393 (r25b) |
| JDK | 17 (el que incluye Android Studio) |
| CMake | 3.22.1 |

La forma fiable de dejarlo todo en su sitio es el script que trae el motor, que
instala exactamente las versiones que espera y escribe las rutas donde Unreal
las busca:

```bash
# Windows
Engine\Extras\Android\SetupAndroid.bat

# Linux / macOS
Engine/Extras/Android/SetupAndroid.sh
```

Después, en el editor: **Edit → Project Settings → Platforms → Android SDK** y
comprobar que las cuatro rutas están rellenas. Si están vacías, el empaquetado
falla con un error que no menciona el SDK.

## Antes de empaquetar

Dos cosas no se pueden generar desde código y hay que hacerlas una vez en el
editor:

1. **El coche.** Importar `Tools/Blender/Build/PaxF1Car.fbx` como malla
   esqueletal, crear un Blueprint derivado de `F1Car` con esa malla y su
   Animation Blueprint, y ponerlo en `CarClass` del GameMode. Los pasos están
   en [SETUP.md](SETUP.md).
2. **Un nivel.** Vale el template **Basic** tal cual: trae luz direccional,
   cielo, niebla y luz ambiental. Sólo hay que borrar el suelo (`Floor`) y
   guardarlo, por ejemplo, como `/Game/Pax/Maps/Circuit`.

El circuito **no** hay que ponerlo: si el nivel no trae ningún `ATrackSpline`,
el GameMode crea uno con el trazado por defecto y el actor se construye su
propia calzada, con pianos, escapatoria, muros y colisión. Si prefieres la
malla de Blender, importa `PaxTrack.fbx`, colócala en el origen y desactiva
`bBuildRuntimeMesh` en el actor del circuito.

Por último, en **Project Settings → Packaging**, añadir el nivel a *List of maps
to include in a packaged build* y ponerlo como *Game Default Map* en
**Project Settings → Maps & Modes**.

## Empaquetar

### Desde el editor

**Platforms → Android → ASTC → Package Project**, y elegir una carpeta de
salida. ASTC es el formato de textura de cualquier móvil moderno; ETC2 sólo
hace falta para dispositivos muy antiguos.

### Desde línea de comandos

```bash
<Ruta_UE5>/Engine/Build/BatchFiles/RunUAT.sh BuildCookRun \
  -project="$PWD/Pax.uproject" \
  -platform=Android \
  -cookflavor=ASTC \
  -clientconfig=Development \
  -build -cook -stage -package -pak \
  -archive -archivedirectory="$PWD/Build"
```

En Windows, `RunUAT.bat` con las mismas opciones.

El APK queda en `Build/Android/`. Para instalarlo:

```bash
adb install -r Build/Android/Pax-Android-Development-arm64.apk
```

## Firmar para distribución

Una build `Development` se firma con la clave de depuración y sirve para
probar. Para `Shipping` hace falta una clave propia:

```bash
keytool -genkey -v -keystore pax.keystore -alias pax \
        -keyalg RSA -keysize 2048 -validity 10000
```

La clave va en `Build/Android/` y la configuración en un ini **que no se
versiona** (`Config/Android/AndroidSigning.ini`, ya está en `.gitignore`):

```ini
[/Script/AndroidRuntimeSettings.AndroidRuntimeSettings]
KeyStore=pax.keystore
KeyAlias=pax
KeyStorePassword=...
KeyPassword=...
```

Nunca metas ese archivo ni el `.keystore` en el repositorio: quien tenga la
clave puede publicar actualizaciones de tu aplicación.

## Qué cambia respecto a la versión de escritorio

Todo esto está en `Config/Android/`, así que la versión de PC no se toca.

**Render.** Se apagan Lumen, las sombras virtuales y Nanite —el camino de
render móvil no los soporta— y se activa el camino directo con MSAA 4x, que en
un móvil da mejor relación calidad/coste que el antialiasing temporal.

**Física.** Los subpasos bajan de ocho a cuatro. Es el mínimo con el que la
suspensión de un monoplaza sigue estable a 300 km/h; por debajo el coche
empieza a flotar en las curvas rápidas.

**Sesión.** Cinco rivales en vez de nueve y tres vueltas en vez de cinco. Cada
rival es un vehículo Chaos simulado de verdad, no un coche sobre raíles, así
que la parrilla es el coste dominante de la partida. Si tu dispositivo va
sobrado, súbelo en `Config/Android/AndroidGame.ini`.

**Resolución.** `r.MobileContentScaleFactor` es el primer sitio donde tocar si
va justo de frames: bajarlo a 0.8 recorta bastante coste de píxel sin que se
note mucho en una pantalla de seis pulgadas.

## Controles táctiles

El mando en pantalla lo dibuja la propia HUD y lo interpreta
`APaxPlayerController`; no usa el interfaz táctil del motor, que necesita un
asset de texturas y sólo ofrece dos joysticks virtuales.

| Zona | Qué hace |
|---|---|
| Mitad inferior izquierda | Volante. Se ancla donde apoyas el pulgar y giras arrastrando. |
| Abajo a la derecha | Acelerador. |
| A su izquierda | Freno. |
| Fila sobre los pedales | DRS (el ancho), ERS, mezcla, cámara y volver a pista. |
| Dos botones sobre la fila | Levas de cambio. La caja pasa a manual al usarlas. |

El volante flota a propósito: un volante fijo obliga a mirar dónde está antes
de tocarlo. El acelerador y el freno no son interruptores secos sino que suben
en unas décimas, porque pisar a fondo de golpe al salir de una curva lenta hace
patinar el coche.

### Dirección por inclinación

Alternativa al volante táctil, en `Config/Android/AndroidGame.ini`:

```ini
[/Script/Pax.PaxPlayerController]
SteeringMode=Tilt
TiltFullLockDegrees=28.0
TiltAxisIndex=1
bInvertTilt=False
```

`TiltAxisIndex` elige qué componente del acelerómetro se usa como volante.
Qué eje corresponde al balanceo depende del montaje del sensor y de la
orientación de la pantalla, así que está configurable en lugar de fijo: si al
inclinar el móvil el coche gira al revés, pon `bInvertTilt=True`; si no gira,
prueba con los índices 0 y 2.

## Probar el mando sin un móvil

`bUseMouseForTouch=True` (en `Config/DefaultInput.ini`) hace que el ratón
genere eventos táctiles, y `bForceTouchControls=True` en
`[/Script/Pax.PaxPlayerController]` de `Config/DefaultGame.ini` activa el mando
en pantalla fuera de móvil. Con las dos cosas se puede ajustar el reparto de
botones desde el editor sin desplegar un APK por cada cambio.

## Si algo falla

**"SDK not found" al empaquetar.** Las rutas de Project Settings → Android SDK
están vacías. Ejecuta `SetupAndroid` y reinicia el editor.

**El APK instala pero se cierra al arrancar.** Casi siempre es el mapa: el
nivel por defecto no está en la lista de mapas a empaquetar. Míralo con
`adb logcat -s UE`.

**No aparece el coche.** `CarClass` del GameMode apunta a `AF1Car` sin malla.
Hay que crear el Blueprint con la malla esqueletal importada.

**Va a tirones en la salida.** Son los coches de la parrilla juntos. Baja
`NumberOfOpponents` en `Config/Android/AndroidGame.ini`.
