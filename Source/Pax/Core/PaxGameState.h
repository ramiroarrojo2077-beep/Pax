// Copyright Pax. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "PaxTypes.h"
#include "PaxGameState.generated.h"

class AF1Car;
class ATrackSpline;

/** Todo lo que dirección de carrera sabe de un participante. */
USTRUCT(BlueprintType)
struct FRaceEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Race")
	TObjectPtr<AF1Car> Car = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Race")
	FString DriverName;

	UPROPERTY(BlueprintReadOnly, Category = "Race")
	int32 CarNumber = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Race")
	bool bIsPlayer = false;

	/** Posición en carrera, 1 = líder. */
	UPROPERTY(BlueprintReadOnly, Category = "Race")
	int32 Position = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Race")
	int32 LapsCompleted = 0;

	/** Distancia en la vuelta actual, en cm. */
	UPROPERTY(BlueprintReadOnly, Category = "Race")
	float LapDistanceCm = 0.f;

	/** Vueltas completas más vuelta actual: la magnitud que ordena la parrilla. */
	UPROPERTY(BlueprintReadOnly, Category = "Race")
	float TotalProgressCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Race")
	float CurrentLapTime = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Race")
	FLapTime BestLap;

	UPROPERTY(BlueprintReadOnly, Category = "Race")
	FLapTime LastLap;

	UPROPERTY(BlueprintReadOnly, Category = "Race")
	TArray<FLapTime> CompletedLaps;

	/** Avisos por límites de pista acumulados. */
	UPROPERTY(BlueprintReadOnly, Category = "Race")
	int32 TrackLimitStrikes = 0;

	/** Segundos de sanción a sumar al tiempo final. */
	UPROPERTY(BlueprintReadOnly, Category = "Race")
	float PenaltySeconds = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Race")
	bool bFinished = false;

	UPROPERTY(BlueprintReadOnly, Category = "Race")
	float FinishTime = 0.f;

	// --- Estado interno de cronometraje ------------------------------------
	float PreviousLapDistanceCm = 0.f;
	float LapStartRaceTime = 0.f;
	float SectorStartRaceTime = 0.f;
	int32 CurrentSector = 0;
	TArray<float> CurrentSectorTimes;
	float OffTrackTimer = 0.f;
	bool bCurrentLapInvalid = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRaceStateChanged, ERaceState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLapCompleted, AF1Car*, Car, FLapTime, Lap);

/**
 * Dirección de carrera: clasificación, cronometraje, sanciones.
 *
 * Todo se deduce de una sola magnitud por coche, el progreso total
 * (vueltas completas × longitud + distancia en la vuelta actual). Con ella
 * salen las posiciones sin ambigüedad, incluidos los doblados, y las
 * diferencias en segundos sin necesidad de sembrar el circuito de trigger
 * volumes que se pueden perder a 300 km/h entre dos frames.
 */
UCLASS()
class PAX_API APaxGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	APaxGameState();

	virtual void Tick(float DeltaSeconds) override;

	/** Añade un coche a la clasificación. GridPosition 0 = pole. */
	void RegisterCar(AF1Car* Car, const FString& DriverName, int32 CarNumber, int32 GridPosition, bool bIsPlayer);

	UFUNCTION(BlueprintCallable, Category = "Pax|Race")
	void SetRaceState(ERaceState NewState);

	UFUNCTION(BlueprintPure, Category = "Pax|Race")
	ERaceState GetRaceState() const { return RaceState; }

	UFUNCTION(BlueprintPure, Category = "Pax|Race")
	float GetRaceTime() const { return RaceTime; }

	UFUNCTION(BlueprintPure, Category = "Pax|Race")
	int32 GetTotalLaps() const { return TotalLaps; }

	void SetTotalLaps(int32 InLaps) { TotalLaps = InLaps; }

	/** Reglas de límites de pista, que fija el GameMode al abrir la sesión. */
	void SetTrackLimitRules(bool bEnabled, int32 WarningsBeforePenalty)
	{
		TrackLimitWarningsBeforePenalty = bEnabled ? FMath::Max(WarningsBeforePenalty, 1) : 0;
	}

	/** Entradas ordenadas por posición. */
	UFUNCTION(BlueprintPure, Category = "Pax|Race")
	const TArray<FRaceEntry>& GetEntries() const { return Entries; }

	/** Entrada de un coche concreto, o nullptr. */
	const FRaceEntry* FindEntry(const AF1Car* Car) const;

	UFUNCTION(BlueprintPure, Category = "Pax|Race")
	int32 GetPosition(const AF1Car* Car) const;

	/**
	 * Diferencia en segundos con el coche inmediatamente delante.
	 * TNumericLimits<float>::Max() si va líder o no hay datos.
	 */
	UFUNCTION(BlueprintPure, Category = "Pax|Race")
	float GetGapAheadSeconds(const AF1Car* Car) const;

	UFUNCTION(BlueprintPure, Category = "Pax|Race")
	float GetGapToLeaderSeconds(const AF1Car* Car) const;

	/** Vuelta rápida de la sesión y quién la tiene. */
	UFUNCTION(BlueprintPure, Category = "Pax|Race")
	float GetFastestLapSeconds() const { return FastestLapSeconds; }

	UFUNCTION(BlueprintPure, Category = "Pax|Race")
	FString GetFastestLapDriver() const { return FastestLapDriver; }

	UPROPERTY(BlueprintAssignable, Category = "Pax|Race")
	FOnRaceStateChanged OnRaceStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Pax|Race")
	FOnLapCompleted OnLapCompleted;

protected:
	virtual void BeginPlay() override;

	/** Avisos antes de que un corte de pista cueste cinco segundos. */
	UPROPERTY(EditDefaultsOnly, Category = "Pax|Race")
	int32 TrackLimitWarningsBeforePenalty = 3;

	UPROPERTY(EditDefaultsOnly, Category = "Pax|Race")
	float TrackLimitPenaltySeconds = 5.f;

	/** Tiempo fuera de los límites que hay que acumular para que cuente. */
	UPROPERTY(EditDefaultsOnly, Category = "Pax|Race")
	float TrackLimitGraceSeconds = 0.4f;

private:
	void UpdateProgress(FRaceEntry& Entry, float DeltaSeconds);
	void UpdateTrackLimits(FRaceEntry& Entry, float DeltaSeconds);
	void CompleteLap(FRaceEntry& Entry);
	void UpdatePositions();

	UPROPERTY(Transient)
	TArray<FRaceEntry> Entries;

	UPROPERTY(Transient)
	TObjectPtr<ATrackSpline> Track;

	UPROPERTY(Transient)
	ERaceState RaceState = ERaceState::Formation;

	float RaceTime = 0.f;
	int32 TotalLaps = 5;
	float FastestLapSeconds = 0.f;
	FString FastestLapDriver;
};
