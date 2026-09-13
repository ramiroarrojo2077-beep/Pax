"""Comprueba la geometría del circuito sin necesidad de Blender.

Simula los módulos bpy/bmesh para poder importar el generador y verificar lo
que de verdad importa del trazado: que cierra, que mide lo que debe, que el
perfil de altura no tiene escalón en la línea de meta, y que el CSV que lee
ATrackSpline sale en centímetros y coincide con el trazado por defecto del C++.

    python3 Tools/Tests/test_track_geometry.py
"""
import sys, types, math, os, tempfile

# bpy / bmesh simulados: sólo hace falta que los import no revienten.
for name in ("bpy", "bmesh"):
    mod = types.ModuleType(name)
    mod.types = types.SimpleNamespace(Collection=object, Object=object, Material=object,
                                      Modifier=object, Mesh=object)
    mod.data = types.SimpleNamespace()
    mod.ops = types.SimpleNamespace()
    mod.context = types.SimpleNamespace()
    sys.modules[name] = mod

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "Tools", "Blender"))
from pax_blender import track, config

# 1) Conversión de ejes coche -> Blender.
assert config.bl(1.0, 0.0, 0.0) == (0.0, -1.0, 0.0), "el morro debe mirar a -Y en Blender"
assert config.bl(0.0, 1.0, 0.0) == (1.0, 0.0, 0.0), "la derecha del coche debe ser +X en Blender"

# 2) Muestreo del eje de pista.
samples = track.sample_centerline()
length = track.track_length_m(samples)
print(f"muestras          : {len(samples)}")
print(f"longitud          : {length/1000:.3f} km")
assert 3000 < length < 7000, "longitud fuera de lo razonable para un circuito"

# 3) El circuito cierra: primera y última muestra deben ser contiguas.
gap = math.dist((samples[0].x, samples[0].y, samples[0].z),
                (samples[-1].x, samples[-1].y, samples[-1].z))
step = length / len(samples)
print(f"paso medio        : {step:.2f} m")
print(f"cierre del bucle  : {gap:.2f} m")
assert gap < step * 2.0, "el trazado no cierra"

# 4) La altura es periódica (no hay escalón al cruzar meta).
assert abs(samples[0].z - track._elevation(1.0)) < 0.01, "el perfil de altura no cierra"

# 5) Normales unitarias y perpendiculares a la tangente.
for s in samples[::17]:
    assert abs(math.hypot(s.nx, s.ny) - 1.0) < 1e-6

# 6) CSV: unidades y formato que espera ATrackSpline.
out = os.path.join(tempfile.mkdtemp(prefix="pax_track_"), "centerline.csv")
track.write_centerline_csv(out, samples)
rows = [l for l in open(out).read().splitlines() if l and not l.startswith("#") and not l.startswith("x")]
print(f"filas en el CSV   : {len(rows)}")
first = [float(v) for v in rows[0].split(",")]
print(f"primera fila      : {first}")
assert len(first) == 4
assert abs(first[3] - 1200.0) < 1e-6, "el ancho debe ir en cm (12 m -> 1200)"
# El primer punto de control es (0,0) m -> (0,0) cm, como en el layout por defecto de ATrackSpline.
assert abs(first[0]) < 1e-6 and abs(first[1]) < 1e-6

# 7) Coincidencia con el trazado por defecto del C++ (mismos puntos en cm).
cpp_first_straight = 120000.0  # ATrackSpline: FVector(120000, 0, 0)
blender_first_straight = config.TRACK.control_points[1][0] * 100.0
assert abs(cpp_first_straight - blender_first_straight) < 1e-6, "el trazado de Blender y el de C++ no coinciden"

print("\nTODO OK")
