# Pruebas

Dos comprobaciones que no necesitan ni Unreal ni Blender instalados, porque
verifican lógica pura sobre los archivos reales del juego.

```bash
python3 Tools/Tests/test_track_geometry.py   # geometría del circuito y CSV
Tools/Tests/TouchLayout/run.sh               # mando en pantalla
```

**`test_track_geometry.py`** simula los módulos `bpy` y `bmesh` para poder
importar el generador de circuito y comprobar que el trazado cierra, que mide
lo que debe, que el perfil de altura no deja un escalón en la línea de meta y
que el CSV sale en centímetros coincidiendo con el trazado por defecto de
`ATrackSpline`. Si la malla y la spline se desalinean, el cronometraje deja de
corresponderse con lo que ve el jugador, y ese fallo es muy difícil de ver
jugando.

**`TouchLayout/run.sh`** compila `Source/Pax/UI/PaxTouchLayout.cpp` —el archivo
real, no una copia— contra un shim mínimo de los tipos de Unreal, y comprueba
en seis resoluciones que ningún control se sale de la pantalla, que no se
solapan entre sí, que todos son lo bastante grandes para un dedo y que la zona
del volante no invade los botones. La alternativa es empaquetar un APK,
instalarlo y descubrirlo con el dedo.
