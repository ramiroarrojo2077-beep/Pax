// Copyright Pax. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/PaxTypes.h"
#include "PaxVehicleTelemetry.h"
#include "DRSComponent.generated.h"

class ATrackSpline;

/**
 * Reglas del DRS.
 *
 * La parte interesante no es abrir el alerón, es la condición: el sistema mide
 * la diferencia con el coche de delante en un punto fijo del trazado —el punto
 * de detección— y sólo habilita la apertura en la zona siguiente si esa
 * diferencia era menor de un segundo. Medir la diferencia continuamente
 * rompería el juego: bastaría con acercarse dentro de la propia zona.
 *
 * Se cierra solo al frenar o al salir de la zona, igual que el sistema real.
 */
UCLASS(ClassGroup = (Pax), meta = (BlueprintSpawnableComponent))
class PAX_API UDRSComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDRSComponent();

	virtual void BeginPlay() override;

	/**
	 * Avanza la máquina de estados.
	 * @param LapDistanceCm  distancia del coche a lo largo del trazado.
	 * @param GapAheadSeconds diferencia con el coche de delante, TNumericLimits<float>::Max() si no hay nadie.
	 */
	void UpdateModel(float DeltaTime, const FPaxVehicleTelemetry& Telemetry, float LapDistanceCm, float GapAheadSeconds);

	/** El piloto pide abrir; sólo tiene efecto si el sistema está habilitado. */
	UFUNCTION(BlueprintCallable, Category = "Pax|DRS")
	void RequestOpen();

	UFUNCTION(BlueprintCallable, Category = "Pax|DRS")
	void Close();

	/** Dirección de carrera habilita el DRS tras dos vueltas, o lo corta en lluvia. */
	UFUNCTION(BlueprintCallable, Category = "Pax|DRS")
	void SetSystemEnabled(bool bEnabled) { bSystemEnabled = bEnabled; }

	UFUNCTION(BlueprintPure, Category = "Pax|DRS")
	EDRSState GetState() const { return State; }

	UFUNCTION(BlueprintPure, Category = "Pax|DRS")
	bool IsOpen() const { return State == EDRSState::Open; }

	/** Apertura del flap [0,1], para animar el alerón. */
	UFUNCTION(BlueprintPure, Category = "Pax|DRS")
	float GetFlapAlpha() const { return FlapAlpha; }

protected:
	/** Diferencia máxima en el punto de detección para habilitar el DRS. */
	UPROPERTY(EditDefaultsOnly, Category = "Pax|DRS")
	float ActivationGapSeconds = 1.f;

	/** Tiempo que tarda el flap en abrirse o cerrarse, en segundos. */
	UPROPERTY(EditDefaultsOnly, Category = "Pax|DRS")
	float FlapTravelTime = 0.25f;

private:
	UPROPERTY(Transient)
	TObjectPtr<ATrackSpline> Track;

	UPROPERTY(Transient)
	EDRSState State = EDRSState::Unavailable;

	/** Una entrada por zona: si en su detección el gap era suficiente. */
	TArray<bool> ZoneEligibility;

	bool bSystemEnabled = true;
	bool bRequested = false;
	float FlapAlpha = 0.f;
	float PreviousLapDistanceCm = 0.f;
	int32 PreviousZone = INDEX_NONE;
};
