// Copyright Pax. All Rights Reserved.

#include "ERSComponent.h"

UERSComponent::UERSComponent()
{
	PrimaryComponentTick.bCanEverTick = false; // lo mueve el coche desde su Tick
	StoredMJ = BatteryCapacityMJ;
}

void UERSComponent::OnLapCompleted()
{
	DeployedThisLapMJ = 0.f;
}

void UERSComponent::CycleMode()
{
	switch (Mode)
	{
	case EERSMode::Harvest:  Mode = EERSMode::Balanced; break;
	case EERSMode::Balanced: Mode = EERSMode::Overtake; break;
	case EERSMode::Overtake: Mode = EERSMode::Harvest; break;
	}
}

float UERSComponent::GetModeDeployScale() const
{
	switch (Mode)
	{
	case EERSMode::Harvest:  return 0.f;
	// En modo equilibrado se raciona: media potencia estira los 4 MJ a lo
	// largo de toda la vuelta en lugar de fundirlos en la primera recta.
	case EERSMode::Balanced: return 0.5f;
	case EERSMode::Overtake: return 1.f;
	}
	return 0.f;
}

float UERSComponent::UpdateModel(float DeltaTime, const FPaxVehicleTelemetry& Telemetry)
{
	if (DeltaTime <= 0.f)
	{
		return 0.f;
	}

	// --- Recuperación ------------------------------------------------------
	float HarvestW = 0.f;
	if (Telemetry.BrakeInput > 0.05f && Telemetry.SpeedKph > 30.f)
	{
		HarvestW += MaxHarvestPowerW * FMath::Clamp(Telemetry.BrakeInput, 0.f, 1.f);
	}
	if (Telemetry.ThrottleInput > 0.5f && Telemetry.EngineRPM > 9000.f)
	{
		HarvestW += HeatHarvestPowerW * Telemetry.ThrottleInput;
	}

	if (HarvestW > 0.f)
	{
		const float HarvestedMJ = (HarvestW * DeltaTime) / 1000000.f;
		StoredMJ = FMath::Min(StoredMJ + HarvestedMJ, BatteryCapacityMJ);
	}

	// --- Despliegue --------------------------------------------------------
	LastDeployedW = 0.f;

	const bool bWantsDeploy = Telemetry.ThrottleInput > 0.35f
		&& Telemetry.SpeedKph > MinDeploySpeedKph
		&& !Telemetry.bAirborne;

	if (bWantsDeploy && StoredMJ > 0.f && GetLapDeploymentRemainingMJ() > 0.f)
	{
		const float RequestedW = MaxDeployPowerW * GetModeDeployScale() * Telemetry.ThrottleInput;

		// No se puede entregar más de lo que queda ni en batería ni en cupo.
		const float AvailableMJ = FMath::Min(StoredMJ, GetLapDeploymentRemainingMJ());
		const float MaxWThisTick = (AvailableMJ * 1000000.f) / DeltaTime;

		LastDeployedW = FMath::Min(RequestedW, MaxWThisTick);

		const float SpentMJ = (LastDeployedW * DeltaTime) / 1000000.f;
		StoredMJ = FMath::Max(0.f, StoredMJ - SpentMJ);
		DeployedThisLapMJ += SpentMJ;
	}

	return LastDeployedW;
}
