// Copyright Pax. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PaxTypes.generated.h"

/** Fases por las que pasa una sesión de carrera. */
UENUM(BlueprintType)
enum class ERaceState : uint8
{
	/** Coches colocados en parrilla, cámara libre, motor al ralentí. */
	Formation,
	/** Secuencia de luces; el acelerador está bloqueado. */
	Countdown,
	/** Carrera en curso. */
	Racing,
	/** El líder ha cruzado la meta en la última vuelta. */
	Finished,
	/** Bandera roja / sesión abortada. */
	Aborted
};

/**
 * Compuestos de neumático. Los valores de agarre y desgaste están en
 * FTyreCompoundData; este enum sólo identifica la selección.
 */
UENUM(BlueprintType)
enum class ETyreCompound : uint8
{
	Soft,
	Medium,
	Hard,
	Intermediate,
	Wet
};

/** Superficie bajo cada rueda; determina agarre y penalizaciones. */
UENUM(BlueprintType)
enum class ETrackSurface : uint8
{
	Asphalt,
	Kerb,
	Gravel,
	Grass,
	PitLane
};

/** Modo de despliegue del ERS elegido por el piloto. */
UENUM(BlueprintType)
enum class EERSMode : uint8
{
	/** Sin despliegue: sólo se recupera energía. */
	Harvest,
	/** Despliegue automático limitado, reparte la batería a lo largo de la vuelta. */
	Balanced,
	/** Despliegue máximo mientras haya batería (botón de overtake). */
	Overtake
};

/** Estado del sistema de reducción de resistencia. */
UENUM(BlueprintType)
enum class EDRSState : uint8
{
	/** Fuera de zona o sin habilitar: alerón cerrado. */
	Unavailable,
	/** En zona y con gap suficiente: el piloto puede abrirlo. */
	Available,
	/** Alerón abierto. */
	Open
};

/** Tiempo de una vuelta completa con sus tres parciales. */
USTRUCT(BlueprintType)
struct FLapTime
{
	GENERATED_BODY()

	/** Tiempo total de vuelta en segundos. 0 si la vuelta no se ha cerrado. */
	UPROPERTY(BlueprintReadOnly, Category = "Timing")
	float TotalSeconds = 0.f;

	/** Parciales en segundos, índice = sector. */
	UPROPERTY(BlueprintReadOnly, Category = "Timing")
	TArray<float> SectorSeconds;

	/** Una vuelta con corte de pista no cuenta para la mejor vuelta. */
	UPROPERTY(BlueprintReadOnly, Category = "Timing")
	bool bInvalidated = false;

	bool IsValid() const { return TotalSeconds > 0.f && !bInvalidated; }
};

/** Formatea segundos como m:ss.mmm, el formato de cronometraje habitual. */
PAX_API FString FormatLapTime(float Seconds);

/** Formatea una diferencia con signo: +0.312 / -1.004. */
PAX_API FString FormatDelta(float Seconds);
