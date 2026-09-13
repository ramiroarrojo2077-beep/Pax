# Arquitectura

## La idea que sostiene todo: una sola magnitud de progreso

Casi todo lo que hace un juego de carreras —posiciones, diferencias, vueltas,
sectores, DRS, límites de pista, la IA— se puede deducir de un único número por
coche: **cuánto lleva recorrido a lo largo del eje del circuito**.

`ATrackSpline` convierte una posición del mundo en esa distancia
(`GetDistanceAtLocation`) y en un desplazamiento lateral respecto al eje
(`GetLateralOffsetAtLocation`). Con esas dos magnitudes:

* la posición en carrera es un `sort` por `vueltas × longitud + distancia`,
  y los doblados salen bien sin casos especiales;
* la diferencia con el de delante es una resta dividida por la velocidad;
* estar fuera de pista es que el desplazamiento lateral supere el ancho;
* el punto de frenada de la IA es un barrido de curvatura hacia delante.

La alternativa habitual —sembrar el circuito de volúmenes de checkpoint— falla
justo donde más duele: a 300 km/h un coche recorre 5 metros entre dos frames y
puede atravesar un trigger sin dispararlo.

## Módulos

```
Core/      APaxGameMode      forma la parrilla, semáforo, reglas de sesión
           APaxGameState     clasificación, cronometraje, sanciones
           APaxPlayerController  Enhanced Input construido en código
           PaxTypes          enums y estructuras compartidas

Track/     ATrackSpline      geometría del circuito y consultas sobre ella

Vehicle/   AF1Car            el monoplaza: física Chaos + sistemas propios
           UF1Wheel*         configuración de las ruedas
           Components/       aerodinámica, neumáticos, ERS, combustible, DRS

AI/        AF1AIController   piloto artificial

UI/        APaxHUD           panel de información sobre Canvas
```

## Por qué los sistemas no hablan con Chaos

`AF1Car` compone una vez por frame un `FPaxVehicleTelemetry` —velocidad,
aceleraciones, ángulo de deriva, entradas, marcha, superficie— y se lo pasa a
todos los componentes. Ninguno de ellos consulta el estado interno del
componente de movimiento.

Eso tiene tres consecuencias prácticas:

1. **Portabilidad entre versiones del motor.** La API interna de Chaos cambia
   entre versiones menores de Unreal; el contrato propio no.
2. **Los modelos se pueden probar.** Alimentar `UTyreComponent::UpdateModel`
   con una telemetría sintética no necesita ni mundo ni física.
3. **El orden es explícito.** El coche sabe en qué orden se actualizan los
   sistemas y qué depende de qué.

El sentido contrario —fuerzas— sí toca la física, y lo hace donde debe: los
componentes aplican fuerzas en `TG_PrePhysics`, antes del paso de simulación
del frame, mientras que la telemetría se compone en `TG_PostPhysics`, cuando el
estado del frame ya está resuelto.

## Unidades

Unreal trabaja en centímetros y kilogramos, así que una fuerza pasada a
`AddForce` está en kg·cm/s². Los modelos se escriben en Newtons, que es lo que
aparece en cualquier tabla de ingeniería, y se convierten justo al aplicarlas
(`PaxUnits::NewtonsToUnreal`). Mezclar ambas es la fuente número uno de coches
que salen volando.

## El modelo aerodinámico, en una línea

`F = k · v²`, con la carga repartida entre eje delantero y trasero y aplicada
en dos puntos distintos del chasis en lugar de en el centro de masas.

Aplicarla en dos puntos es lo que hace que el reparto aerodinámico sea una
magnitud real y no un número decorativo: mover el balance hacia delante mete
morro en las curvas rápidas y hace el coche nervioso al frenar, exactamente
como en el coche real.

Calibrado con dos puntos conocidos de un F1 moderno: unos 1.500 kg de carga a
200 km/h y unos 3.300 kg a 300 km/h.

## El modelo de neumático, en una línea

La energía que destruye un neumático es proporcional a la fuerza que transmite
multiplicada por el deslizamiento con el que la transmite. Ambas crecen con la
aceleración, así que el modelo usa el módulo del vector de G combinada como
entrada única, repartido entre las cuatro ruedas según transferencia de carga.

De ahí salen solos los comportamientos que importan en carrera: los blandos van
más rápido y duran menos, empujar pronto deja sin goma al final, y la ventana
térmica castiga tanto el neumático frío como el sobrecalentado.

## La IA no hace trampas

`AF1AIController` conduce con las mismas tres entradas que el jugador. Su
velocidad objetivo sale de resolver `v² = a_lat · r` teniendo en cuenta que
`a_lat` depende a su vez de `v` por la carga aerodinámica (se itera tres veces,
converge de sobra), y su punto de frenada de quedarse con la velocidad más
restrictiva de los 250 m siguientes.

La dificultad **no toca la física**: escala qué fracción del agarre disponible
se atreve a usar el piloto (del 86% al 98%) y cuánto ruido mete en el volante.
Ese 12% es aproximadamente la diferencia real entre el primero y el último de
una parrilla.

## Flujo de una sesión

```
APaxGameMode::InitGame        lee opciones de la URL
APaxGameMode::StartPlay       localiza el circuito, crea la parrilla
  └─ SpawnCarAtGrid           coche + combustible + neumáticos por trazado
  └─ AdvanceStartLights       cinco luces, una por segundo
  └─ StartRace                retardo aleatorio, luces fuera
APaxGameState::Tick           progreso, vueltas, sectores, posiciones, límites
AF1Car::Tick                  telemetría -> neumáticos -> combustible -> ERS -> DRS
```
