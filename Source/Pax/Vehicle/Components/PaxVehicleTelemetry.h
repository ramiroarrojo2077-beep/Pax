// Copyright Pax. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/PaxTypes.h"
#include "PaxVehicleTelemetry.generated.h"

/**
 * Fotografía del estado dinámico del coche en un tick.
 *
 * El coche la construye una sola vez por frame y se la pasa a los sistemas
 * (neumáticos, ERS, combustible). Así los componentes no dependen de la API
 * interna de Chaos —que cambia entre versiones del motor— sino de un contrato
 * estable propio, y además el modelo queda testeable alimentándolo a mano.
 */
USTRUCT(BlueprintType)
struct FPaxVehicleTelemetry
{
	GENERATED_BODY()

	/** Velocidad longitudinal en km/h (negativa marcha atrás). */
	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	float SpeedKph = 0.f;

	/** Aceleración lateral en G. Positiva hacia la derecha. */
	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	float LateralG = 0.f;

	/** Aceleración longitudinal en G. Positiva acelerando. */
	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	float LongitudinalG = 0.f;

	/** Ángulo de deriva del chasis en grados. */
	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	float SlipAngleDeg = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	float ThrottleInput = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	float BrakeInput = 0.f;

	/** Dirección normalizada [-1, 1]. */
	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	float SteeringInput = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	int32 Gear = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	float EngineRPM = 0.f;

	/** Carga aerodinámica actual en Newtons. */
	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	float DownforceN = 0.f;

	/** Superficie bajo el coche. */
	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	ETrackSurface Surface = ETrackSurface::Asphalt;

	/** Distancia recorrida en este tick, en metros. */
	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	float DistanceTravelledM = 0.f;

	/** Todas las ruedas despegadas del suelo. */
	UPROPERTY(BlueprintReadOnly, Category = "Telemetry")
	bool bAirborne = false;
};
