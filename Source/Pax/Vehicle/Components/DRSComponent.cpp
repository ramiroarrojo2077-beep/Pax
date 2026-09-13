// Copyright Pax. All Rights Reserved.

#include "DRSComponent.h"
#include "Track/TrackSpline.h"

UDRSComponent::UDRSComponent()
{
	PrimaryComponentTick.bCanEverTick = false; // lo mueve el coche desde su Tick
}

void UDRSComponent::BeginPlay()
{
	Super::BeginPlay();

	Track = ATrackSpline::Get(GetWorld());
	if (Track)
	{
		ZoneEligibility.Init(false, Track->GetNumDRSZones());
	}
}

void UDRSComponent::RequestOpen()
{
	bRequested = true;
}

void UDRSComponent::Close()
{
	bRequested = false;
	if (State == EDRSState::Open)
	{
		State = EDRSState::Available;
	}
}

void UDRSComponent::UpdateModel(float DeltaTime, const FPaxVehicleTelemetry& Telemetry, float LapDistanceCm, float GapAheadSeconds)
{
	if (!Track)
	{
		Track = ATrackSpline::Get(GetWorld());
		if (!Track)
		{
			return;
		}
	}

	// --- Puntos de detección ----------------------------------------------
	// Se comprueba si en este tick se ha cruzado la detección de alguna zona.
	// El recorrido del tick es pequeño, así que basta con mirar si el punto
	// queda entre la posición anterior y la actual.
	if (ZoneEligibility.Num() != Track->GetNumDRSZones())
	{
		ZoneEligibility.Init(false, Track->GetNumDRSZones());
	}

	for (int32 ZoneIndex = 0; ZoneIndex < ZoneEligibility.Num(); ++ZoneIndex)
	{
		const float DetectionDistance = Track->GetDRSDetectionDistance(ZoneIndex);

		const float ToDetectionBefore = Track->GetSignedDistanceDelta(PreviousLapDistanceCm, DetectionDistance);
		const float ToDetectionNow = Track->GetSignedDistanceDelta(LapDistanceCm, DetectionDistance);

		// Cambio de signo hacia atrás = acabamos de pasar por el punto.
		if (ToDetectionBefore > 0.f && ToDetectionNow <= 0.f)
		{
			ZoneEligibility[ZoneIndex] = GapAheadSeconds <= ActivationGapSeconds;
		}
	}

	PreviousLapDistanceCm = LapDistanceCm;

	// --- Estado ------------------------------------------------------------
	const int32 CurrentZone = Track->GetActiveDRSZone(LapDistanceCm);
	const bool bZoneChanged = CurrentZone != PreviousZone;
	PreviousZone = CurrentZone;

	const bool bEligible = CurrentZone != INDEX_NONE
		&& ZoneEligibility.IsValidIndex(CurrentZone)
		&& ZoneEligibility[CurrentZone];

	// Frenar cierra el alerón de inmediato: es una condición de seguridad del
	// sistema real y además impide usarlo como ayuda en curva.
	const bool bBraking = Telemetry.BrakeInput > 0.05f;

	if (!bSystemEnabled || CurrentZone == INDEX_NONE || !bEligible)
	{
		State = EDRSState::Unavailable;
		bRequested = false;
	}
	else if (bBraking || bZoneChanged)
	{
		State = EDRSState::Available;
		bRequested = false;
	}
	else
	{
		State = bRequested ? EDRSState::Open : EDRSState::Available;
	}

	// --- Animación del flap -------------------------------------------------
	const float Target = (State == EDRSState::Open) ? 1.f : 0.f;
	const float Step = DeltaTime / FMath::Max(FlapTravelTime, 0.01f);
	FlapAlpha = FMath::Clamp(FlapAlpha + FMath::Sign(Target - FlapAlpha) * Step, 0.f, 1.f);
	if (FMath::Abs(Target - FlapAlpha) < Step)
	{
		FlapAlpha = Target;
	}
}
