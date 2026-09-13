// Copyright Pax. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "F1AIController.generated.h"

class AF1Car;
class ATrackSpline;

/**
 * Piloto artificial.
 *
 * No sigue una trayectoria grabada ni hace trampas con la física: conduce el
 * mismo coche que el jugador a través de las mismas entradas (acelerador,
 * freno, volante). El comportamiento sale de tres cálculos encadenados:
 *
 *  1. Velocidad máxima de paso por curva a partir del radio del trazado y del
 *     agarre disponible, contando con que la carga aerodinámica crece con la
 *     velocidad: en una curva rápida se puede pasar mucho más deprisa que lo
 *     que daría el agarre mecánico solo.
 *  2. Punto de frenada, mirando hacia delante y quedándose con la velocidad
 *     más restrictiva que permita llegar frenando a cada curva.
 *  3. Dirección, apuntando a un punto de la trazada situado más adelante
 *     cuanto más rápido se vaya.
 *
 * Encima se superponen los adelantamientos: buscar hueco, defender el interior
 * y usar rebufo, DRS y ERS cuando toca.
 *
 * La dificultad no toca la física: escala el margen de agarre que se permite
 * usar el piloto y el ruido que mete en el volante, que es la diferencia real
 * entre un piloto rápido y uno lento.
 */
UCLASS()
class PAX_API AF1AIController : public AAIController
{
	GENERATED_BODY()

public:
	AF1AIController();

	virtual void OnPossess(APawn* InPawn) override;
	virtual void Tick(float DeltaTime) override;

	/** 0 = piloto de relleno, 1 = referencia del campeonato. */
	UFUNCTION(BlueprintCallable, Category = "Pax|AI")
	void SetDifficulty(float InDifficulty);

	UFUNCTION(BlueprintPure, Category = "Pax|AI")
	float GetDifficulty() const { return Difficulty; }

protected:
	/** Velocidad máxima de paso por una curva de radio RadiusM, en m/s. */
	float ComputeCornerSpeedMs(float RadiusM, float CurrentSpeedMs) const;

	/** Velocidad objetivo mirando hacia delante, en m/s. */
	float ComputeTargetSpeedMs(float LapDistanceCm, float CurrentSpeedMs) const;

	/** Decide el desplazamiento lateral respecto a la trazada, en cm. */
	void UpdateRacingLineOffset(float DeltaTime, float LapDistanceCm, float CurrentSpeedMs);

	/** Gestiona DRS y modo de ERS según la situación de carrera. */
	void UpdateRaceSystems(float GapAheadSeconds);

	/** Coeficiente de agarre efectivo que el piloto se atreve a usar. */
	float GetUsableGrip() const;

	UPROPERTY(EditDefaultsOnly, Category = "Pax|AI", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Difficulty = 0.85f;

	/** Agarre máximo teórico del neumático (mu). */
	UPROPERTY(EditDefaultsOnly, Category = "Pax|AI")
	float PeakFrictionCoefficient = 1.8f;

	/** Carga aerodinámica en N por (m/s)²; debe coincidir con UAeroComponent. */
	UPROPERTY(EditDefaultsOnly, Category = "Pax|AI")
	float DownforceCoefficient = 4.8f;

	/** Masa de referencia para el cálculo, en kg. */
	UPROPERTY(EditDefaultsOnly, Category = "Pax|AI")
	float ReferenceMassKg = 850.f;

	/** Distancia máxima de anticipación para frenar, en metros. */
	UPROPERTY(EditDefaultsOnly, Category = "Pax|AI")
	float BrakingLookaheadM = 250.f;

	/** Separación mínima que respeta al coche de delante, en cm. */
	UPROPERTY(EditDefaultsOnly, Category = "Pax|AI")
	float MinimumFollowingDistanceCm = 900.f;

private:
	UPROPERTY(Transient)
	TObjectPtr<AF1Car> ControlledCar;

	UPROPERTY(Transient)
	TObjectPtr<ATrackSpline> Track;

	/** Desplazamiento lateral actual respecto a la trazada, en cm. */
	float RacingLineOffsetCm = 0.f;
	float TargetOffsetCm = 0.f;

	/** Ruido de volante para que dos pilotos no tracen exactamente igual. */
	float SteeringNoisePhase = 0.f;

	/** Error de dirección del tick anterior, para el término derivativo. */
	float PreviousHeadingError = 0.f;

	/** Tiempo que lleva bloqueado detrás del mismo coche. */
	float BlockedTimer = 0.f;
};
