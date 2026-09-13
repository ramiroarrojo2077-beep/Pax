"""Dimensiones, convenios de ejes y rutas de salida.

Todas las medidas están en metros y en *espacio de coche*: +X hacia delante,
+Y hacia la derecha del piloto, +Z hacia arriba, origen a nivel del suelo en el
punto medio de la batalla. Es el mismo convenio que usa Unreal, así que los
números de este archivo se pueden comparar directamente con los de
Source/Pax/Vehicle/F1Car.cpp.

Las cotas siguen el reglamento técnico vigente: 5,6 m de largo, 2,0 m de ancho
máximo, 3,6 m de batalla y ruedas de 18 pulgadas con 720 mm de diámetro.
"""

from dataclasses import dataclass, field


# ---------------------------------------------------------------------------
# Conversión de ejes
# ---------------------------------------------------------------------------
# Blender es dextrógiro con -Y hacia delante; Unreal es levógiro con +X hacia
# delante. El exportador FBX con sus ajustes por omisión (axis_forward='-Z',
# axis_up='Y') más la opción "Convert Scene" del importador de Unreal resuelven
# la conversión correctamente *si* el modelo mira hacia -Y en Blender.
#
# Para no tener que pensar en eso al modelar, todo el código de geometría
# trabaja en espacio de coche y esta función hace la traducción al colocar cada
# vértice. Así las cotas del archivo se leen igual que en el reglamento.

def bl(x: float, y: float, z: float):
    """Pasa un punto de espacio de coche (X adelante, Y derecha) a Blender."""
    return (y, -x, z)


@dataclass(frozen=True)
class CarSpec:
    """Cotas del monoplaza."""

    # --- Conjunto ----------------------------------------------------------
    length: float = 5.60
    width: float = 2.00
    height: float = 0.95
    wheelbase: float = 3.60

    # --- Ruedas ------------------------------------------------------------
    wheel_radius: float = 0.360
    front_tyre_width: float = 0.305
    rear_tyre_width: float = 0.405
    rim_radius: float = 0.229  # 18" de llanta
    front_track: float = 1.60
    rear_track: float = 1.55

    # --- Chasis ------------------------------------------------------------
    # El monocasco se define como una serie de secciones a lo largo del eje X;
    # cada una es (x, semiancho, z_inferior, z_superior).
    monocoque_sections: tuple = (
        (2.80, 0.055, 0.20, 0.30),   # punta del morro
        (2.35, 0.090, 0.17, 0.34),
        (1.95, 0.150, 0.13, 0.42),
        (1.45, 0.240, 0.09, 0.52),
        (1.00, 0.310, 0.07, 0.60),   # zona del piloto
        (0.50, 0.340, 0.06, 0.64),
        (0.00, 0.330, 0.06, 0.78),   # arco de seguridad
        (-0.55, 0.300, 0.07, 0.72),
        (-1.10, 0.255, 0.09, 0.62),
        (-1.70, 0.195, 0.11, 0.50),
        (-2.20, 0.140, 0.13, 0.40),
        (-2.60, 0.095, 0.15, 0.32),  # salida de escape
    )

    # --- Pontones ----------------------------------------------------------
    sidepod_front_x: float = 0.85
    sidepod_rear_x: float = -1.45
    sidepod_half_width: float = 0.72
    sidepod_top_z: float = 0.60
    sidepod_bottom_z: float = 0.12

    # --- Fondo y difusor ---------------------------------------------------
    floor_half_width: float = 0.75
    floor_front_x: float = 1.55
    floor_rear_x: float = -2.15
    floor_z: float = 0.055
    diffuser_rear_x: float = -2.55
    diffuser_height: float = 0.35

    # --- Alerón delantero --------------------------------------------------
    front_wing_x: float = 2.55
    front_wing_half_width: float = 1.00
    front_wing_chord: float = 0.52
    front_wing_z: float = 0.10
    front_wing_elements: int = 4
    front_endplate_height: float = 0.30

    # --- Alerón trasero ----------------------------------------------------
    rear_wing_x: float = -2.35
    rear_wing_half_width: float = 0.525
    rear_wing_chord: float = 0.36
    rear_wing_z: float = 0.87
    rear_endplate_height: float = 0.45
    # El flap móvil es un objeto aparte: es el que gira al abrir el DRS.
    drs_flap_chord: float = 0.20
    drs_flap_z_offset: float = 0.12

    # --- Habitáculo y halo --------------------------------------------------
    cockpit_x: float = 0.75
    halo_x: float = 0.95
    halo_height: float = 0.95
    halo_tube_radius: float = 0.024

    # --- Suspensión ---------------------------------------------------------
    wishbone_radius: float = 0.022

    @property
    def front_axle_x(self) -> float:
        return self.wheelbase * 0.5

    @property
    def rear_axle_x(self) -> float:
        return -self.wheelbase * 0.5


@dataclass(frozen=True)
class TrackSpec:
    """Parámetros del circuito generado."""

    name: str = "Pax International"
    # Puntos de control del eje de pista, en metros y en plano XY.
    # Es el mismo trazado que lleva por defecto ATrackSpline, de modo que la
    # malla y la spline de juego coinciden sin ajustes.
    control_points: tuple = (
        (0.0, 0.0),
        (1200.0, 0.0),
        (1650.0, 180.0),
        (1720.0, 620.0),
        (1400.0, 880.0),
        (960.0, 800.0),
        (740.0, 1040.0),
        (880.0, 1400.0),
        (460.0, 1520.0),
        (140.0, 1260.0),
        (-220.0, 1320.0),
        (-460.0, 1000.0),
        (-300.0, 580.0),
        (-540.0, 260.0),
        (-300.0, -20.0),
    )
    half_width: float = 6.0
    kerb_width: float = 1.2
    runoff_width: float = 12.0
    # Muestras por segmento entre puntos de control: más muestras, curvas más
    # suaves y malla más pesada.
    samples_per_segment: int = 24
    kerb_height: float = 0.05
    wall_height: float = 1.2


@dataclass
class BuildPaths:
    """Dónde deja el pipeline lo que genera."""

    build_dir: str = "Tools/Blender/Build"
    car_fbx: str = "PaxF1Car.fbx"
    track_fbx: str = "PaxTrack.fbx"
    car_blend: str = "PaxF1Car.blend"
    track_blend: str = "PaxTrack.blend"
    centerline_csv: str = "track_centerline.csv"


CAR = CarSpec()
TRACK = TrackSpec()
PATHS = BuildPaths()

# Nombres de hueso. Deben coincidir exactamente con WheelSetups en F1Car.cpp.
BONE_ROOT = "Root"
BONE_BODY = "Body"
BONE_WHEELS = ("Wheel_FL", "Wheel_FR", "Wheel_RL", "Wheel_RR")
BONE_DRS = "DRS_Flap"
BONE_STEERING = "Steering"
