// Copyright Pax. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PaxTypes.h"
#include "PaxGameMode.generated.h"

class AF1Car;
class ATrackSpline;

/**
 * Reglas de la sesión: forma la parrilla, saca los coches, controla el
 * semáforo y da la carrera por terminada.
 *
 * Acepta opciones por URL, así que se puede probar una configuración sin tocar
 * nada del nivel:
 *   ?Laps=20?Opponents=15?Difficulty=0.95
 */
UCLASS()
class PAX_API APaxGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	APaxGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void StartPlay() override;
	virtual APawn* SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot) override;

	/** Luces encendidas del semáforo, 0-5. La HUD las dibuja. */
	UFUNCTION(BlueprintPure, Category = "Pax|Race")
	int32 GetStartLightsOn() const { return StartLightsOn; }

protected:
	/** Crea un monoplaza en el puesto de parrilla indicado, listo para correr. */
	AF1Car* SpawnCarAtGrid(int32 GridPosition);

	/** Crea los rivales y los coloca en la parrilla. */
	void SpawnOpponents();

	/** Enciende una luz más; al llegar a cinco arranca la cuenta aleatoria. */
	void AdvanceStartLights();

	/** Luces fuera: empieza la carrera. */
	void StartRace();

	UPROPERTY(EditDefaultsOnly, Category = "Pax|Race", meta = (ClampMin = "1"))
	int32 NumberOfLaps = 5;

	UPROPERTY(EditDefaultsOnly, Category = "Pax|Race", meta = (ClampMin = "0", ClampMax = "19"))
	int32 NumberOfOpponents = 9;

	UPROPERTY(EditDefaultsOnly, Category = "Pax|Race", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AIDifficulty = 0.85f;

	UPROPERTY(EditDefaultsOnly, Category = "Pax|Race")
	bool bFormationLap = false;

	UPROPERTY(EditDefaultsOnly, Category = "Pax|Race")
	bool bTrackLimitsEnabled = true;

	UPROPERTY(EditDefaultsOnly, Category = "Pax|Race")
	int32 TrackLimitWarningsBeforePenalty = 3;

	/** Clase de monoplaza para el jugador y para la IA. */
	UPROPERTY(EditDefaultsOnly, Category = "Pax|Race")
	TSubclassOf<AF1Car> CarClass;

	/** Segundos entre luz y luz del semáforo. */
	UPROPERTY(EditDefaultsOnly, Category = "Pax|Race")
	float LightIntervalSeconds = 1.f;

private:
	/** Puesto de parrilla del jugador; los rivales ocupan el resto. */
	int32 PlayerGridPosition = 0;

	int32 StartLightsOn = 0;

	FTimerHandle StartSequenceTimer;

	UPROPERTY(Transient)
	TObjectPtr<ATrackSpline> Track;
};
