// Copyright Pax. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PaxVehicleTelemetry.h"
#include "FuelComponent.generated.h"

/** Mapa de motor: cuánto se estira la mezcla. */
UENUM(BlueprintType)
enum class EFuelMix : uint8
{
	/** Ahorro: menos potencia, llegas a meta. */
	Lean,
	Standard,
	/** Máxima potencia, el consumo se dispara. */
	Rich
};

/**
 * Depósito y consumo.
 *
 * Importa por dos motivos y los dos están modelados: la mezcla cambia la
 * potencia disponible, y los kilos de gasolina son masa que el coche arrastra
 * —unos 0,03 s por vuelta y por kilo—. Salir con depósito lleno y terminar
 * con él vacío son dos coches distintos, y ahí está media estrategia.
 */
UCLASS(ClassGroup = (Pax), meta = (BlueprintSpawnableComponent))
class PAX_API UFuelComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFuelComponent();

	/**
	 * Ajusta el consumo estimado a la longitud del circuito.
	 * Se parte de un consumo por kilómetro constante, que es como se calcula
	 * la carga de salida en la realidad.
	 */
	UFUNCTION(BlueprintCallable, Category = "Pax|Fuel")
	void ConfigureForTrack(float LapLengthMeters);

	/** Consume combustible y devuelve el multiplicador de par del mapa actual. */
	float UpdateModel(float DeltaTime, const FPaxVehicleTelemetry& Telemetry);

	/** Masa actual de combustible en kg, para sumarla a la del coche. */
	UFUNCTION(BlueprintPure, Category = "Pax|Fuel")
	float GetFuelKg() const { return FuelKg; }

	/** Vueltas que quedan al consumo actual. */
	UFUNCTION(BlueprintPure, Category = "Pax|Fuel")
	float GetLapsRemaining() const;

	/**
	 * Delta de combustible: vueltas de margen respecto a lo necesario para
	 * terminar. Negativo significa que hay que levantar el pie.
	 */
	UFUNCTION(BlueprintPure, Category = "Pax|Fuel")
	float GetFuelDeltaLaps(int32 RaceLapsRemaining) const;

	UFUNCTION(BlueprintCallable, Category = "Pax|Fuel")
	void SetMix(EFuelMix NewMix) { Mix = NewMix; }

	UFUNCTION(BlueprintCallable, Category = "Pax|Fuel")
	void CycleMix();

	UFUNCTION(BlueprintPure, Category = "Pax|Fuel")
	EFuelMix GetMix() const { return Mix; }

	/**
	 * Cierra la contabilidad de la vuelta: el consumo real medido sustituye a
	 * la estimación, así el margen que se muestra al piloto refleja su ritmo y
	 * no una tabla teórica.
	 */
	UFUNCTION(BlueprintCallable, Category = "Pax|Fuel")
	void OnLapCompleted();

	/** Consumo real de la última vuelta cerrada, en kg. */
	UFUNCTION(BlueprintPure, Category = "Pax|Fuel")
	float GetMeasuredKgPerLap() const { return MeasuredKgPerLap; }

	/** Carga el depósito para una carrera de la longitud indicada. */
	UFUNCTION(BlueprintCallable, Category = "Pax|Fuel")
	void FillForRace(int32 NumLaps, float SafetyMarginLaps = 1.f);

	UFUNCTION(BlueprintPure, Category = "Pax|Fuel")
	bool IsOutOfFuel() const { return FuelKg <= 0.f; }

protected:
	/** Máximo reglamentario de carga de salida, en kg. */
	UPROPERTY(EditDefaultsOnly, Category = "Pax|Fuel")
	float MaxFuelKg = 110.f;

	/** Consumo de referencia a pleno gas, en kg/s. */
	UPROPERTY(EditDefaultsOnly, Category = "Pax|Fuel")
	float BurnRateKgPerSecond = 0.028f;

	/** Consumo de referencia por kilómetro, en kg. */
	UPROPERTY(EditDefaultsOnly, Category = "Pax|Fuel")
	float ExpectedKgPerKm = 0.44f;

	/** Consumo estimado por vuelta a ritmo de carrera, en kg. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pax|Fuel")
	float ExpectedKgPerLap = 1.9f;

private:
	UPROPERTY(Transient)
	EFuelMix Mix = EFuelMix::Standard;

	float FuelKg = 100.f;

	/** Media móvil del consumo real, en kg/vuelta. */
	float MeasuredKgPerLap = 1.9f;
	float KgSinceLapStart = 0.f;
	float DistanceSinceLapStartM = 0.f;
};
