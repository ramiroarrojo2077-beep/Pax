// Copyright Pax. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/PaxTypes.h"
#include "PaxVehicleTelemetry.h"
#include "TyreComponent.generated.h"

/** Parámetros que definen un compuesto. */
USTRUCT(BlueprintType)
struct FTyreCompoundData
{
	GENERATED_BODY()

	/** Agarre máximo relativo (1.0 = referencia del compuesto medio). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tyre")
	float PeakGrip = 1.f;

	/** Vueltas de vida útil aproximadas a ritmo de carrera. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tyre")
	float LifeLaps = 25.f;

	/** Temperatura de trabajo ideal en °C. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tyre")
	float OptimalTempC = 100.f;

	/** Semiancho de la ventana térmica: fuera de ella se pierde agarre. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tyre")
	float TempWindowC = 25.f;

	/** Agarre que queda con el neumático completamente gastado. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tyre")
	float WornGrip = 0.72f;
};

/** Estado de una de las cuatro ruedas. */
USTRUCT(BlueprintType)
struct FTyreState
{
	GENERATED_BODY()

	/** 1 = nuevo, 0 = agotado. */
	UPROPERTY(BlueprintReadOnly, Category = "Tyre")
	float Life = 1.f;

	UPROPERTY(BlueprintReadOnly, Category = "Tyre")
	float TemperatureC = 40.f;

	/** Plano por bloqueo de rueda: mete vibración y resta agarre. */
	UPROPERTY(BlueprintReadOnly, Category = "Tyre")
	float FlatSpot = 0.f;

	/** Multiplicador de fricción resultante que se aplica a la rueda. */
	UPROPERTY(BlueprintReadOnly, Category = "Tyre")
	float GripMultiplier = 1.f;
};

/**
 * Modelo de neumático: temperatura, degradación y agarre resultante.
 *
 * La energía que destruye un neumático es, en primera aproximación,
 * proporcional a la fuerza que transmite por el deslizamiento con el que la
 * transmite. Como ambas crecen con la aceleración combinada, el modelo usa el
 * módulo del vector de G (lateral y longitudinal) como entrada única, con un
 * reparto delante/detrás que depende de si se está frenando o acelerando.
 *
 * De ahí salen los comportamientos que interesan en carrera: los blandos van
 * un segundo por vuelta más rápido pero duran un tercio, empujar en las
 * primeras vueltas te deja sin neumático al final, y salir de boxes con goma
 * fría cuesta agarre durante la vuelta de calentamiento.
 */
UCLASS(ClassGroup = (Pax), meta = (BlueprintSpawnableComponent))
class PAX_API UTyreComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTyreComponent();

	virtual void BeginPlay() override;

	/**
	 * Ajusta el modelo a la longitud real del circuito.
	 * La vida de un compuesto se expresa en vueltas, así que sin esto un
	 * trazado de 7 km gastaría la goma al mismo ritmo que uno de 4 km.
	 */
	UFUNCTION(BlueprintCallable, Category = "Pax|Tyres")
	void ConfigureForTrack(float LapLengthMeters);

	/** Avanza el modelo un tick con el estado actual del coche. */
	void UpdateModel(float DeltaTime, const FPaxVehicleTelemetry& Telemetry);

	/** Monta un juego nuevo del compuesto indicado (parada en boxes). */
	UFUNCTION(BlueprintCallable, Category = "Pax|Tyres")
	void FitNewSet(ETyreCompound Compound, float StartTemperatureC = 80.f);

	UFUNCTION(BlueprintPure, Category = "Pax|Tyres")
	ETyreCompound GetCompound() const { return CurrentCompound; }

	/** Multiplicador de agarre de una rueda (0 = delantera izq, 3 = trasera der). */
	UFUNCTION(BlueprintPure, Category = "Pax|Tyres")
	float GetWheelGripMultiplier(int32 WheelIndex) const;

	/** Vida media del juego, 1 = nuevo. Es el número que ve el piloto. */
	UFUNCTION(BlueprintPure, Category = "Pax|Tyres")
	float GetAverageLife() const;

	UFUNCTION(BlueprintPure, Category = "Pax|Tyres")
	float GetAverageTemperature() const;

	UFUNCTION(BlueprintPure, Category = "Pax|Tyres")
	const TArray<FTyreState>& GetTyreStates() const { return Tyres; }

	/** Vueltas rodadas con el juego actual. */
	UFUNCTION(BlueprintPure, Category = "Pax|Tyres")
	float GetSetAgeLaps() const { return SetAgeLaps; }

	/** Datos del compuesto montado. */
	UFUNCTION(BlueprintPure, Category = "Pax|Tyres")
	FTyreCompoundData GetCompoundData() const;

protected:
	/** Tabla de compuestos, indexada por ETyreCompound. */
	UPROPERTY(EditDefaultsOnly, Category = "Pax|Tyres")
	TMap<ETyreCompound, FTyreCompoundData> Compounds;

	/** Temperatura ambiente de pista en °C. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pax|Tyres")
	float AmbientTempC = 32.f;

	/** °C ganados por segundo con 1 G de aceleración combinada. */
	UPROPERTY(EditDefaultsOnly, Category = "Pax|Tyres")
	float HeatingRate = 18.f;

	/** °C perdidos por segundo hacia el ambiente a velocidad de carrera. */
	UPROPERTY(EditDefaultsOnly, Category = "Pax|Tyres")
	float CoolingRate = 6.f;

	/** Longitud de vuelta de referencia en metros para convertir vida a vueltas. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pax|Tyres")
	float ReferenceLapLengthM = 4300.f;

private:
	float ComputeGrip(const FTyreState& Tyre, const FTyreCompoundData& Data, ETrackSurface Surface) const;

	UPROPERTY(Transient)
	TArray<FTyreState> Tyres;

	UPROPERTY(Transient)
	ETyreCompound CurrentCompound = ETyreCompound::Medium;

	float SetAgeLaps = 0.f;
};
