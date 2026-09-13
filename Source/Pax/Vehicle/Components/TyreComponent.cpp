// Copyright Pax. All Rights Reserved.

#include "TyreComponent.h"
#include "Pax.h"

namespace
{
	/** Índices de rueda tal y como los ordena el WheelSetups del coche. */
	constexpr int32 WheelFrontLeft = 0;
	constexpr int32 WheelFrontRight = 1;
	constexpr int32 WheelRearLeft = 2;
	constexpr int32 WheelRearRight = 3;
	constexpr int32 NumWheels = 4;
}

UTyreComponent::UTyreComponent()
{
	PrimaryComponentTick.bCanEverTick = false; // lo mueve el coche desde su Tick

	// Escalonado de compuestos al estilo del suministro real: cada paso más
	// blando da alrededor de medio segundo por vuelta y dura bastante menos.
	FTyreCompoundData Soft;
	Soft.PeakGrip = 1.06f;
	Soft.LifeLaps = 16.f;
	Soft.OptimalTempC = 105.f;
	Soft.TempWindowC = 20.f;
	Soft.WornGrip = 0.70f;
	Compounds.Add(ETyreCompound::Soft, Soft);

	FTyreCompoundData Medium;
	Medium.PeakGrip = 1.00f;
	Medium.LifeLaps = 26.f;
	Medium.OptimalTempC = 100.f;
	Medium.TempWindowC = 25.f;
	Medium.WornGrip = 0.74f;
	Compounds.Add(ETyreCompound::Medium, Medium);

	FTyreCompoundData Hard;
	Hard.PeakGrip = 0.95f;
	Hard.LifeLaps = 38.f;
	Hard.OptimalTempC = 95.f;
	Hard.TempWindowC = 30.f;
	Hard.WornGrip = 0.78f;
	Compounds.Add(ETyreCompound::Hard, Hard);

	FTyreCompoundData Inter;
	Inter.PeakGrip = 0.86f;
	Inter.LifeLaps = 22.f;
	Inter.OptimalTempC = 80.f;
	Inter.TempWindowC = 22.f;
	Inter.WornGrip = 0.68f;
	Compounds.Add(ETyreCompound::Intermediate, Inter);

	FTyreCompoundData Wet;
	Wet.PeakGrip = 0.78f;
	Wet.LifeLaps = 30.f;
	Wet.OptimalTempC = 70.f;
	Wet.TempWindowC = 25.f;
	Wet.WornGrip = 0.64f;
	Compounds.Add(ETyreCompound::Wet, Wet);
}

void UTyreComponent::BeginPlay()
{
	Super::BeginPlay();
	FitNewSet(CurrentCompound, AmbientTempC + 30.f);
}

void UTyreComponent::ConfigureForTrack(float LapLengthMeters)
{
	if (LapLengthMeters > 100.f)
	{
		ReferenceLapLengthM = LapLengthMeters;
	}
}

void UTyreComponent::FitNewSet(ETyreCompound Compound, float StartTemperatureC)
{
	CurrentCompound = Compound;
	SetAgeLaps = 0.f;

	Tyres.Reset(NumWheels);
	Tyres.SetNum(NumWheels);
	for (FTyreState& Tyre : Tyres)
	{
		Tyre.Life = 1.f;
		Tyre.FlatSpot = 0.f;
		Tyre.TemperatureC = StartTemperatureC;
		Tyre.GripMultiplier = 1.f;
	}
}

FTyreCompoundData UTyreComponent::GetCompoundData() const
{
	if (const FTyreCompoundData* Data = Compounds.Find(CurrentCompound))
	{
		return *Data;
	}
	return FTyreCompoundData();
}

float UTyreComponent::GetWheelGripMultiplier(int32 WheelIndex) const
{
	return Tyres.IsValidIndex(WheelIndex) ? Tyres[WheelIndex].GripMultiplier : 1.f;
}

float UTyreComponent::GetAverageLife() const
{
	if (Tyres.Num() == 0)
	{
		return 1.f;
	}

	float Sum = 0.f;
	for (const FTyreState& Tyre : Tyres)
	{
		Sum += Tyre.Life;
	}
	return Sum / Tyres.Num();
}

float UTyreComponent::GetAverageTemperature() const
{
	if (Tyres.Num() == 0)
	{
		return AmbientTempC;
	}

	float Sum = 0.f;
	for (const FTyreState& Tyre : Tyres)
	{
		Sum += Tyre.TemperatureC;
	}
	return Sum / Tyres.Num();
}

void UTyreComponent::UpdateModel(float DeltaTime, const FPaxVehicleTelemetry& Telemetry)
{
	if (Tyres.Num() != NumWheels || DeltaTime <= 0.f)
	{
		return;
	}

	const FTyreCompoundData Data = GetCompoundData();

	SetAgeLaps += Telemetry.DistanceTravelledM / FMath::Max(ReferenceLapLengthM, 1.f);

	// Aceleración combinada: es la magnitud que realmente castiga la goma.
	const float CombinedG = FMath::Sqrt(FMath::Square(Telemetry.LateralG) + FMath::Square(Telemetry.LongitudinalG));

	// Reparto por eje. Frenando la carga pasa al tren delantero; acelerando,
	// al trasero. Un 60/40 en cada sentido reproduce bien qué eje se muere
	// antes según el estilo de conducción.
	const float FrontBias = Telemetry.LongitudinalG < 0.f ? 0.60f : 0.40f;
	const float RearBias = 1.f - FrontBias;

	// En curva el neumático exterior soporta casi toda la carga lateral.
	const float LateralBias = FMath::Clamp(FMath::Abs(Telemetry.LateralG) / 4.f, 0.f, 1.f);
	const bool bTurningRight = Telemetry.LateralG > 0.f;

	// Vida consumida por segundo si se rodase todo el rato a 1 G combinado.
	const float LapSeconds = FMath::Max(ReferenceLapLengthM / 55.f, 1.f); // ~200 km/h de media
	const float BaseWearPerSecond = 1.f / FMath::Max(Data.LifeLaps * LapSeconds, 1.f);

	for (int32 Index = 0; Index < NumWheels; ++Index)
	{
		FTyreState& Tyre = Tyres[Index];

		const bool bFront = (Index == WheelFrontLeft || Index == WheelFrontRight);
		const bool bRight = (Index == WheelFrontRight || Index == WheelRearRight);

		const float AxleShare = (bFront ? FrontBias : RearBias) * 2.f; // 1.0 = reparto neutro
		// El exterior de la curva se lleva hasta el 80% de la carga del eje.
		const bool bOuter = (bTurningRight != bRight); // girando a la derecha carga la izquierda
		const float SideShare = FMath::Lerp(1.f, bOuter ? 1.6f : 0.4f, LateralBias);

		const float Load = AxleShare * SideShare;

		// --- Temperatura ---------------------------------------------------
		// Calienta con el trabajo que hace y se enfría con el aire que le pasa.
		const float SpeedCooling = CoolingRate * FMath::Clamp(FMath::Abs(Telemetry.SpeedKph) / 200.f, 0.15f, 1.5f);
		const float HeatIn = HeatingRate * CombinedG * Load;
		const float HeatOut = SpeedCooling * (Tyre.TemperatureC - AmbientTempC) * 0.05f;
		Tyre.TemperatureC = FMath::Clamp(Tyre.TemperatureC + (HeatIn - HeatOut) * DeltaTime, AmbientTempC, 180.f);

		// --- Desgaste ------------------------------------------------------
		// Fuera de ventana térmica la goma se degrada mucho más rápido:
		// sobrecalentada se ampolla, fría se granula.
		const float TempExcess = FMath::Max(0.f, Tyre.TemperatureC - (Data.OptimalTempC + Data.TempWindowC));
		const float ThermalPenalty = 1.f + TempExcess / 25.f;

		// El desgaste crece más que linealmente con la carga: exigir un 20%
		// más al neumático cuesta bastante más de un 20% de vida.
		const float WorkFactor = FMath::Pow(FMath::Max(CombinedG * Load, 0.f), 1.6f);

		float SurfacePenalty = 1.f;
		switch (Telemetry.Surface)
		{
		case ETrackSurface::Kerb:    SurfacePenalty = 1.8f; break;
		case ETrackSurface::Gravel:  SurfacePenalty = 3.0f; break;
		case ETrackSurface::Grass:   SurfacePenalty = 1.4f; break;
		default: break;
		}

		Tyre.Life = FMath::Clamp(
			Tyre.Life - BaseWearPerSecond * WorkFactor * ThermalPenalty * SurfacePenalty * DeltaTime,
			0.f, 1.f);

		// --- Planos por bloqueo --------------------------------------------
		// Frenada fuerte con poca carga en ese eje y el coche casi parado de
		// girar: la rueda se bloquea y deja un plano permanente.
		const bool bHardBraking = Telemetry.BrakeInput > 0.85f && Telemetry.LongitudinalG < -3.5f;
		if (bHardBraking && bFront && FMath::Abs(Telemetry.SpeedKph) > 60.f && Tyre.TemperatureC < Data.OptimalTempC - 15.f)
		{
			Tyre.FlatSpot = FMath::Min(Tyre.FlatSpot + 0.25f * DeltaTime, 1.f);
		}

		Tyre.GripMultiplier = ComputeGrip(Tyre, Data, Telemetry.Surface);
	}
}

float UTyreComponent::ComputeGrip(const FTyreState& Tyre, const FTyreCompoundData& Data, ETrackSurface Surface) const
{
	// Curva térmica: campana alrededor de la temperatura óptima. Fría no
	// agarra, pasada de temperatura tampoco.
	const float TempDelta = (Tyre.TemperatureC - Data.OptimalTempC) / FMath::Max(Data.TempWindowC, 1.f);
	const float ThermalFactor = FMath::Clamp(1.f - 0.35f * TempDelta * TempDelta, 0.55f, 1.f);

	// El agarre no cae linealmente: se mantiene casi entero hasta bien entrada
	// la vida del neumático y luego se desploma ("cliff").
	const float LifeFactor = FMath::Lerp(Data.WornGrip, 1.f, FMath::Pow(Tyre.Life, 0.45f));

	const float FlatSpotFactor = 1.f - Tyre.FlatSpot * 0.12f;

	float SurfaceFactor = 1.f;
	switch (Surface)
	{
	case ETrackSurface::Kerb:    SurfaceFactor = 0.85f; break;
	case ETrackSurface::Gravel:  SurfaceFactor = 0.45f; break;
	case ETrackSurface::Grass:   SurfaceFactor = 0.35f; break;
	case ETrackSurface::PitLane: SurfaceFactor = 0.95f; break;
	default: break;
	}

	return FMath::Max(Data.PeakGrip * ThermalFactor * LifeFactor * FlatSpotFactor * SurfaceFactor, 0.1f);
}
