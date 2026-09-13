// Copyright Pax. All Rights Reserved.

#include "PaxGameState.h"
#include "Pax.h"
#include "Vehicle/F1Car.h"
#include "Vehicle/Components/ERSComponent.h"
#include "Vehicle/Components/FuelComponent.h"
#include "Track/TrackSpline.h"

APaxGameState::APaxGameState()
{
	PrimaryActorTick.bCanEverTick = true;
}

void APaxGameState::BeginPlay()
{
	Super::BeginPlay();
	Track = ATrackSpline::Get(GetWorld());
}

void APaxGameState::RegisterCar(AF1Car* Car, const FString& DriverName, int32 CarNumber, int32 GridPosition, bool bIsPlayer)
{
	if (!Car)
	{
		return;
	}

	FRaceEntry Entry;
	Entry.Car = Car;
	Entry.DriverName = DriverName;
	Entry.CarNumber = CarNumber;
	Entry.bIsPlayer = bIsPlayer;
	Entry.Position = GridPosition + 1;
	Entry.CurrentSectorTimes.Reset();

	if (!Track)
	{
		Track = ATrackSpline::Get(GetWorld());
	}
	if (Track)
	{
		Entry.LapDistanceCm = Track->GetDistanceAtLocation(Car->GetActorLocation());
		Entry.PreviousLapDistanceCm = Entry.LapDistanceCm;
	}

	Entries.Add(MoveTemp(Entry));
}

void APaxGameState::SetRaceState(ERaceState NewState)
{
	if (RaceState == NewState)
	{
		return;
	}

	RaceState = NewState;

	if (NewState == ERaceState::Racing)
	{
		// El cronómetro arranca con las luces: la vuelta de todos empieza aquí.
		RaceTime = 0.f;
		for (FRaceEntry& Entry : Entries)
		{
			Entry.LapStartRaceTime = 0.f;
			Entry.SectorStartRaceTime = 0.f;
			Entry.CurrentSector = 0;
			Entry.CurrentSectorTimes.Reset();
			Entry.bCurrentLapInvalid = false;
		}
	}

	OnRaceStateChanged.Broadcast(NewState);
}

void APaxGameState::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!Track)
	{
		Track = ATrackSpline::Get(GetWorld());
		if (!Track)
		{
			return;
		}
	}

	if (RaceState == ERaceState::Racing)
	{
		RaceTime += DeltaSeconds;
	}

	for (FRaceEntry& Entry : Entries)
	{
		if (!Entry.Car)
		{
			continue;
		}

		UpdateProgress(Entry, DeltaSeconds);

		if (RaceState == ERaceState::Racing && !Entry.bFinished)
		{
			Entry.CurrentLapTime = RaceTime - Entry.LapStartRaceTime;
			UpdateTrackLimits(Entry, DeltaSeconds);
		}
	}

	UpdatePositions();
}

void APaxGameState::UpdateProgress(FRaceEntry& Entry, float DeltaSeconds)
{
	const float Length = Track->GetTrackLength();
	if (Length <= 0.f)
	{
		return;
	}

	const float NewDistance = Track->GetDistanceAtLocation(Entry.Car->GetActorLocation());

	// Cruce de la línea de meta: se detecta por el salto de distancia y sólo
	// si el coche iba hacia delante, para que retroceder tras un trompo no
	// sume una vuelta.
	const float Delta = Track->GetSignedDistanceDelta(Entry.PreviousLapDistanceCm, NewDistance);
	const bool bWrapped = Entry.PreviousLapDistanceCm > Length * 0.75f && NewDistance < Length * 0.25f;

	if (bWrapped && Delta > 0.f && RaceState == ERaceState::Racing && !Entry.bFinished)
	{
		CompleteLap(Entry);
	}

	// Cambio de sector.
	if (RaceState == ERaceState::Racing && !Entry.bFinished)
	{
		const int32 Sector = Track->GetSectorAtDistance(NewDistance);
		if (Sector != Entry.CurrentSector && Sector == Entry.CurrentSector + 1)
		{
			Entry.CurrentSectorTimes.Add(RaceTime - Entry.SectorStartRaceTime);
			Entry.SectorStartRaceTime = RaceTime;
			Entry.CurrentSector = Sector;
		}
	}

	Entry.PreviousLapDistanceCm = NewDistance;
	Entry.LapDistanceCm = NewDistance;
	Entry.TotalProgressCm = Entry.LapsCompleted * Length + NewDistance;
}

void APaxGameState::UpdateTrackLimits(FRaceEntry& Entry, float DeltaSeconds)
{
	if (TrackLimitWarningsBeforePenalty <= 0)
	{
		return;
	}

	const bool bWithinLimits = Track->IsWithinTrackLimits(Entry.Car->GetActorLocation(), 100.f);

	if (bWithinLimits)
	{
		Entry.OffTrackTimer = 0.f;
		return;
	}

	Entry.OffTrackTimer += DeltaSeconds;

	// Sólo cuenta una vez por salida: el temporizador se pone en negativo para
	// no encadenar avisos mientras el coche sigue fuera.
	if (Entry.OffTrackTimer >= TrackLimitGraceSeconds)
	{
		Entry.OffTrackTimer = -100.f;
		Entry.bCurrentLapInvalid = true;
		++Entry.TrackLimitStrikes;

		if (Entry.TrackLimitStrikes % TrackLimitWarningsBeforePenalty == 0)
		{
			Entry.PenaltySeconds += TrackLimitPenaltySeconds;
			UE_LOG(LogPax, Log, TEXT("%s: %.0f s de sanción por límites de pista (aviso %d)."),
				*Entry.DriverName, TrackLimitPenaltySeconds, Entry.TrackLimitStrikes);
		}
	}
}

void APaxGameState::CompleteLap(FRaceEntry& Entry)
{
	FLapTime Lap;
	Lap.TotalSeconds = RaceTime - Entry.LapStartRaceTime;
	Lap.SectorSeconds = Entry.CurrentSectorTimes;
	// Último sector: lo que va del corte anterior al cruce de meta.
	Lap.SectorSeconds.Add(RaceTime - Entry.SectorStartRaceTime);
	Lap.bInvalidated = Entry.bCurrentLapInvalid;

	++Entry.LapsCompleted;
	Entry.LastLap = Lap;
	Entry.CompletedLaps.Add(Lap);

	if (Lap.IsValid() && (!Entry.BestLap.IsValid() || Lap.TotalSeconds < Entry.BestLap.TotalSeconds))
	{
		Entry.BestLap = Lap;
	}

	if (Lap.IsValid() && (FastestLapSeconds <= 0.f || Lap.TotalSeconds < FastestLapSeconds))
	{
		FastestLapSeconds = Lap.TotalSeconds;
		FastestLapDriver = Entry.DriverName;
	}

	// Reinicio de la contabilidad de la vuelta.
	Entry.LapStartRaceTime = RaceTime;
	Entry.SectorStartRaceTime = RaceTime;
	Entry.CurrentSector = 0;
	Entry.CurrentSectorTimes.Reset();
	Entry.bCurrentLapInvalid = false;

	// Los sistemas del coche que razonan por vuelta se enteran aquí.
	if (Entry.Car)
	{
		if (UERSComponent* ERS = Entry.Car->GetERS())
		{
			ERS->OnLapCompleted();
		}
		if (UFuelComponent* FuelSystem = Entry.Car->GetFuel())
		{
			FuelSystem->OnLapCompleted();
		}
	}

	OnLapCompleted.Broadcast(Entry.Car, Lap);

	if (Entry.LapsCompleted >= TotalLaps)
	{
		Entry.bFinished = true;
		Entry.FinishTime = RaceTime + Entry.PenaltySeconds;

		// El primero en terminar cierra la carrera; el resto completa su vuelta.
		if (RaceState == ERaceState::Racing)
		{
			SetRaceState(ERaceState::Finished);
		}
	}
}

void APaxGameState::UpdatePositions()
{
	// Ordena por progreso total. Los que ya han terminado van por delante y
	// entre ellos manda el tiempo final, sanciones incluidas.
	Entries.Sort([](const FRaceEntry& A, const FRaceEntry& B)
	{
		if (A.bFinished != B.bFinished)
		{
			return A.bFinished;
		}
		if (A.bFinished && B.bFinished)
		{
			return A.FinishTime < B.FinishTime;
		}
		return A.TotalProgressCm > B.TotalProgressCm;
	});

	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		Entries[Index].Position = Index + 1;
	}
}

const FRaceEntry* APaxGameState::FindEntry(const AF1Car* Car) const
{
	return Entries.FindByPredicate([Car](const FRaceEntry& Entry) { return Entry.Car.Get() == Car; });
}

int32 APaxGameState::GetPosition(const AF1Car* Car) const
{
	const FRaceEntry* Entry = FindEntry(Car);
	return Entry ? Entry->Position : 0;
}

float APaxGameState::GetGapAheadSeconds(const AF1Car* Car) const
{
	const FRaceEntry* Entry = FindEntry(Car);
	if (!Entry || Entry->Position <= 1)
	{
		return TNumericLimits<float>::Max();
	}

	const int32 AheadIndex = Entry->Position - 2; // Entries está ordenado por posición
	if (!Entries.IsValidIndex(AheadIndex))
	{
		return TNumericLimits<float>::Max();
	}

	const float GapCm = Entries[AheadIndex].TotalProgressCm - Entry->TotalProgressCm;
	if (GapCm <= 0.f)
	{
		return 0.f;
	}

	// Se convierte a tiempo con la velocidad del perseguidor, que es como se
	// calcula el "gap" real: cuánto tardaría en llegar a donde está el otro.
	const float SpeedCms = FMath::Max(FMath::Abs(Entry->Car->GetSpeedKph()) / 0.036f, 100.f);
	return GapCm / SpeedCms;
}

float APaxGameState::GetGapToLeaderSeconds(const AF1Car* Car) const
{
	const FRaceEntry* Entry = FindEntry(Car);
	if (!Entry || Entries.Num() == 0 || Entry->Position <= 1)
	{
		return 0.f;
	}

	const float GapCm = Entries[0].TotalProgressCm - Entry->TotalProgressCm;
	const float SpeedCms = FMath::Max(FMath::Abs(Entry->Car->GetSpeedKph()) / 0.036f, 100.f);
	return FMath::Max(GapCm, 0.f) / SpeedCms;
}
