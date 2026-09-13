// Copyright Pax. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/PaxTypes.h"
#include "PaxVehicleTelemetry.h"
#include "ERSComponent.generated.h"

/**
 * Sistema de recuperación de energía.
 *
 * Reproduce las dos reglas que gobiernan su uso en carrera: el MGU-K puede
 * entregar 120 kW, y por vuelta sólo se pueden desplegar 4 MJ. Con eso el
 * despliegue deja de ser un botón de turbo infinito y pasa a ser una decisión:
 * gastar la batería para atacar ahora o guardarla para la recta de meta.
 *
 * La recuperación viene de frenar (MGU-K) y, en menor medida, de los gases del
 * escape con el motor arriba de vueltas (MGU-H).
 */
UCLASS(ClassGroup = (Pax), meta = (BlueprintSpawnableComponent))
class PAX_API UERSComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UERSComponent();

	/** Avanza el modelo; devuelve la potencia entregada en vatios. */
	float UpdateModel(float DeltaTime, const FPaxVehicleTelemetry& Telemetry);

	/** Reinicia el presupuesto de despliegue al cruzar la línea de meta. */
	UFUNCTION(BlueprintCallable, Category = "Pax|ERS")
	void OnLapCompleted();

	UFUNCTION(BlueprintCallable, Category = "Pax|ERS")
	void SetMode(EERSMode NewMode) { Mode = NewMode; }

	UFUNCTION(BlueprintCallable, Category = "Pax|ERS")
	void CycleMode();

	UFUNCTION(BlueprintPure, Category = "Pax|ERS")
	EERSMode GetMode() const { return Mode; }

	/** Carga de batería normalizada [0,1]. */
	UFUNCTION(BlueprintPure, Category = "Pax|ERS")
	float GetChargeFraction() const { return StoredMJ / FMath::Max(BatteryCapacityMJ, KINDA_SMALL_NUMBER); }

	/** Energía que queda por desplegar en esta vuelta, en MJ. */
	UFUNCTION(BlueprintPure, Category = "Pax|ERS")
	float GetLapDeploymentRemainingMJ() const { return FMath::Max(0.f, MaxDeployPerLapMJ - DeployedThisLapMJ); }

	/** Potencia entregada en el último tick, en kW. */
	UFUNCTION(BlueprintPure, Category = "Pax|ERS")
	float GetDeployedPowerKw() const { return LastDeployedW / 1000.f; }

	UFUNCTION(BlueprintPure, Category = "Pax|ERS")
	bool IsDeploying() const { return LastDeployedW > 1000.f; }

protected:
	/** Capacidad de la batería en MJ. */
	UPROPERTY(EditDefaultsOnly, Category = "Pax|ERS")
	float BatteryCapacityMJ = 4.f;

	/** Tope de despliegue por vuelta en MJ. */
	UPROPERTY(EditDefaultsOnly, Category = "Pax|ERS")
	float MaxDeployPerLapMJ = 4.f;

	/** Potencia máxima del MGU-K en vatios (120 kW ≈ 161 CV). */
	UPROPERTY(EditDefaultsOnly, Category = "Pax|ERS")
	float MaxDeployPowerW = 120000.f;

	/** Potencia máxima de recuperación en frenada, en vatios. */
	UPROPERTY(EditDefaultsOnly, Category = "Pax|ERS")
	float MaxHarvestPowerW = 120000.f;

	/** Recuperación del MGU-H a pleno gas, en vatios. */
	UPROPERTY(EditDefaultsOnly, Category = "Pax|ERS")
	float HeatHarvestPowerW = 25000.f;

	/** Velocidad mínima para desplegar, en km/h: por debajo sólo haría patinar. */
	UPROPERTY(EditDefaultsOnly, Category = "Pax|ERS")
	float MinDeploySpeedKph = 60.f;

private:
	/** Fracción de potencia que permite el modo actual. */
	float GetModeDeployScale() const;

	UPROPERTY(Transient)
	EERSMode Mode = EERSMode::Balanced;

	float StoredMJ = 4.f;
	float DeployedThisLapMJ = 0.f;
	float LastDeployedW = 0.f;
};
