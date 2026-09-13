// Copyright Pax. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ChaosVehicleWheel.h"
#include "F1Wheel.generated.h"

/**
 * Base común de las ruedas de un monoplaza.
 *
 * Las cifras salen del reglamento técnico vigente de neumáticos de 18":
 * llanta de 18", diámetro exterior 720 mm (radio 36 cm), banda de 305 mm
 * delante y 405 mm detrás. Con el coche en 798 kg y ~5 G de frenada, el par
 * de freno por rueda queda en el entorno de los 4.500 Nm delante.
 *
 * La suspensión de un F1 es prácticamente rígida: 3-4 cm de recorrido total y
 * amortiguación casi crítica. Valores más blandos hacen que el coche cabecee
 * y que la carga aerodinámica oscile, que es el fallo típico al portar un
 * coche de calle a un monoplaza.
 */
UCLASS(Abstract)
class PAX_API UF1Wheel : public UChaosVehicleWheel
{
	GENERATED_BODY()

public:
	UF1Wheel();
};

/** Tren delantero: dirige y se lleva el 60% del par de freno. */
UCLASS()
class PAX_API UF1WheelFront : public UF1Wheel
{
	GENERATED_BODY()

public:
	UF1WheelFront();
};

/** Tren trasero: tracción, freno restante y freno de mano (sólo para boxes). */
UCLASS()
class PAX_API UF1WheelRear : public UF1Wheel
{
	GENERATED_BODY()

public:
	UF1WheelRear();
};
