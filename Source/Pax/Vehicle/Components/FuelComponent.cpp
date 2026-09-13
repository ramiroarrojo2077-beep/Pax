// Copyright Pax. All Rights Reserved.

#include "FuelComponent.h"

UFuelComponent::UFuelComponent()
{
	PrimaryComponentTick.bCanEverTick = false; // lo mueve el coche desde su Tick
}

void UFuelComponent::ConfigureForTrack(float LapLengthMeters)
{
	if (LapLengthMeters > 100.f)
	{
		ExpectedKgPerLap = (LapLengthMeters / 1000.f) * ExpectedKgPerKm;
		MeasuredKgPerLap = ExpectedKgPerLap;
	}
}

void UFuelComponent::CycleMix()
{
	switch (Mix)
	{
	case EFuelMix::Lean:     Mix = EFuelMix::Standard; break;
	case EFuelMix::Standard: Mix = EFuelMix::Rich; break;
	case EFuelMix::Rich:     Mix = EFuelMix::Lean; break;
	}
}

void UFuelComponent::FillForRace(int32 NumLaps, float SafetyMarginLaps)
{
	FuelKg = FMath::Min(MaxFuelKg, (NumLaps + SafetyMarginLaps) * ExpectedKgPerLap);
	MeasuredKgPerLap = ExpectedKgPerLap;
	KgSinceLapStart = 0.f;
	DistanceSinceLapStartM = 0.f;
}

void UFuelComponent::OnLapCompleted()
{
	// Sólo se acepta la medición si la vuelta se ha rodado entera; si no, una
	// vuelta de salida de boxes falsearía el consumo a la baja.
	if (KgSinceLapStart > 0.f && DistanceSinceLapStartM > 500.f)
	{
		// Media móvil: suaviza una vuelta lenta tras bandera amarilla sin
		// ignorar un cambio real de ritmo.
		MeasuredKgPerLap = FMath::Lerp(MeasuredKgPerLap, KgSinceLapStart, 0.5f);
	}

	KgSinceLapStart = 0.f;
	DistanceSinceLapStartM = 0.f;
}

float UFuelComponent::GetLapsRemaining() const
{
	return FuelKg / FMath::Max(MeasuredKgPerLap, 0.01f);
}

float UFuelComponent::GetFuelDeltaLaps(int32 RaceLapsRemaining) const
{
	return GetLapsRemaining() - RaceLapsRemaining;
}

float UFuelComponent::UpdateModel(float DeltaTime, const FPaxVehicleTelemetry& Telemetry)
{
	if (DeltaTime <= 0.f)
	{
		return 1.f;
	}

	float TorqueScale = 1.f;
	float BurnScale = 1.f;

	switch (Mix)
	{
	// Ahorrar un 12% de gasolina cuesta un 6% de par: por eso levantar y
	// planear en las rectas es más rentable que ir a tope y quedarse corto.
	case EFuelMix::Lean:     TorqueScale = 0.94f; BurnScale = 0.88f; break;
	case EFuelMix::Standard: TorqueScale = 1.00f; BurnScale = 1.00f; break;
	case EFuelMix::Rich:     TorqueScale = 1.05f; BurnScale = 1.18f; break;
	}

	if (FuelKg <= 0.f)
	{
		FuelKg = 0.f;
		return 0.f; // sin gasolina no hay par
	}

	// El consumo sigue al acelerador, con un mínimo al ralentí.
	const float Throttle = FMath::Clamp(Telemetry.ThrottleInput, 0.f, 1.f);
	const float BurnKg = BurnRateKgPerSecond * BurnScale * (0.08f + 0.92f * Throttle) * DeltaTime;

	FuelKg = FMath::Max(0.f, FuelKg - BurnKg);

	// Consumo real medido por vuelta, que es lo que se muestra al piloto.
	KgSinceLapStart += BurnKg;
	DistanceSinceLapStartM += Telemetry.DistanceTravelledM;

	return TorqueScale;
}
