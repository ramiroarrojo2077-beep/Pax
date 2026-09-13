// Copyright Pax. All Rights Reserved.

#include "PaxGameMode.h"
#include "Pax.h"
#include "PaxGameState.h"
#include "PaxPlayerController.h"
#include "UI/PaxHUD.h"
#include "Vehicle/F1Car.h"
#include "Vehicle/Components/FuelComponent.h"
#include "Vehicle/Components/TyreComponent.h"
#include "AI/F1AIController.h"
#include "Track/TrackSpline.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	/** Nombres de relleno para la parrilla de IA. */
	const TCHAR* AIDriverNames[] = {
		TEXT("M. Varela"), TEXT("L. Ferrand"), TEXT("K. Nakano"), TEXT("R. Oduya"),
		TEXT("S. Bergmann"), TEXT("A. Ricci"), TEXT("T. Lindqvist"), TEXT("D. Okafor"),
		TEXT("P. Navarro"), TEXT("J. Halloran"), TEXT("E. Moreau"), TEXT("V. Sokolov"),
		TEXT("C. Delgado"), TEXT("H. Brandt"), TEXT("N. Alves"), TEXT("O. Kaimal"),
		TEXT("F. Bianchi"), TEXT("G. Mwangi"), TEXT("Z. Petrov")
	};
	constexpr int32 NumAIDriverNames = UE_ARRAY_COUNT(AIDriverNames);
}

APaxGameMode::APaxGameMode()
{
	GameStateClass = APaxGameState::StaticClass();
	PlayerControllerClass = APaxPlayerController::StaticClass();
	HUDClass = APaxHUD::StaticClass();
	DefaultPawnClass = AF1Car::StaticClass();
	CarClass = AF1Car::StaticClass();
	bStartPlayersAsSpectators = false;
}

void APaxGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	NumberOfLaps = FMath::Max(1, UGameplayStatics::GetIntOption(Options, TEXT("Laps"), NumberOfLaps));
	NumberOfOpponents = FMath::Clamp(UGameplayStatics::GetIntOption(Options, TEXT("Opponents"), NumberOfOpponents), 0, NumAIDriverNames);

	const FString DifficultyOption = UGameplayStatics::ParseOption(Options, TEXT("Difficulty"));
	if (!DifficultyOption.IsEmpty())
	{
		AIDifficulty = FMath::Clamp(FCString::Atof(*DifficultyOption), 0.f, 1.f);
	}

	// El jugador sale último: tiene que adelantar para ganar, que es lo que
	// hace interesante una carrera corta.
	PlayerGridPosition = NumberOfOpponents;
}

void APaxGameMode::StartPlay()
{
	Super::StartPlay();

	Track = ATrackSpline::Get(GetWorld());
	if (!Track)
	{
		UE_LOG(LogPax, Error, TEXT("No hay ningún ATrackSpline en el nivel: no se puede formar la parrilla."));
		return;
	}

	if (APaxGameState* RaceState = GetGameState<APaxGameState>())
	{
		RaceState->SetTotalLaps(NumberOfLaps);
		RaceState->SetTrackLimitRules(bTrackLimitsEnabled, TrackLimitWarningsBeforePenalty);
		RaceState->SetRaceState(ERaceState::Formation);
	}

	SpawnOpponents();

	// Semáforo: cinco luces, una por segundo.
	StartLightsOn = 0;
	GetWorldTimerManager().SetTimer(StartSequenceTimer, this, &APaxGameMode::AdvanceStartLights, LightIntervalSeconds, true, 2.f);
}

AF1Car* APaxGameMode::SpawnCarAtGrid(int32 GridPosition)
{
	// La parrilla la define el circuito, no los PlayerStart del nivel: así un
	// mapa nuevo sólo necesita su spline para funcionar.
	if (!Track)
	{
		Track = ATrackSpline::Get(GetWorld());
	}

	if (!Track || !CarClass)
	{
		return nullptr;
	}

	FTransform SpawnTransform = Track->GetGridSlotTransform(GridPosition);
	SpawnTransform.SetLocation(SpawnTransform.GetLocation() + FVector(0.f, 0.f, 40.f));

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.Owner = this;

	AF1Car* Car = GetWorld()->SpawnActor<AF1Car>(CarClass, SpawnTransform, SpawnParams);
	if (!Car)
	{
		return nullptr;
	}

	Car->SetControlsLocked(true);

	// Se configura aquí y no en el BeginPlay del coche porque la carga de
	// combustible depende de la longitud del trazado, y los coches en parrilla
	// pueden crearse antes de que el mundo arranque.
	const float LapLengthM = Track->GetTrackLength() * 0.01f;
	if (UTyreComponent* TyreSystem = Car->GetTyres())
	{
		TyreSystem->ConfigureForTrack(LapLengthM);
	}
	if (UFuelComponent* FuelSystem = Car->GetFuel())
	{
		FuelSystem->ConfigureForTrack(LapLengthM);
		FuelSystem->FillForRace(NumberOfLaps);
	}

	return Car;
}

APawn* APaxGameMode::SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot)
{
	AF1Car* Car = SpawnCarAtGrid(PlayerGridPosition);
	if (!Car)
	{
		return Super::SpawnDefaultPawnFor_Implementation(NewPlayer, StartSpot);
	}

	if (APaxGameState* RaceState = GetGameState<APaxGameState>())
	{
		RaceState->RegisterCar(Car, TEXT("Jugador"), 1, PlayerGridPosition, true);
	}

	return Car;
}

void APaxGameMode::SpawnOpponents()
{
	if (!Track || !CarClass)
	{
		return;
	}

	APaxGameState* RaceState = GetGameState<APaxGameState>();

	for (int32 Index = 0; Index < NumberOfOpponents; ++Index)
	{
		AF1Car* Car = SpawnCarAtGrid(Index);
		if (!Car)
		{
			continue;
		}

		AF1AIController* AI = GetWorld()->SpawnActor<AF1AIController>(AF1AIController::StaticClass());
		if (AI)
		{
			// Dispersión de nivel alrededor de la dificultad elegida: una
			// parrilla en la que todos van exactamente igual no es una carrera.
			const float Spread = FMath::FRandRange(-0.10f, 0.06f);
			AI->SetDifficulty(FMath::Clamp(AIDifficulty + Spread, 0.f, 1.f));
			AI->Possess(Car);
		}

		if (RaceState)
		{
			RaceState->RegisterCar(Car, AIDriverNames[Index % NumAIDriverNames], Index + 2, Index, false);
		}
	}

	UE_LOG(LogPax, Log, TEXT("Parrilla formada: %d rivales, %d vueltas, dificultad %.2f."),
		NumberOfOpponents, NumberOfLaps, AIDifficulty);
}

void APaxGameMode::AdvanceStartLights()
{
	++StartLightsOn;

	if (APaxGameState* RaceState = GetGameState<APaxGameState>())
	{
		RaceState->SetRaceState(ERaceState::Countdown);
	}

	if (StartLightsOn >= 5)
	{
		GetWorldTimerManager().ClearTimer(StartSequenceTimer);

		// El retardo aleatorio tras la quinta luz es lo que impide memorizar
		// la salida: hay que reaccionar, no contar.
		const float HoldSeconds = FMath::FRandRange(0.4f, 1.6f);
		GetWorldTimerManager().SetTimer(StartSequenceTimer, this, &APaxGameMode::StartRace, HoldSeconds, false);
	}
}

void APaxGameMode::StartRace()
{
	StartLightsOn = 0;

	for (TActorIterator<AF1Car> It(GetWorld()); It; ++It)
	{
		It->SetControlsLocked(false);
	}

	if (APaxGameState* RaceState = GetGameState<APaxGameState>())
	{
		RaceState->SetRaceState(ERaceState::Racing);
	}

	UE_LOG(LogPax, Log, TEXT("¡Luces fuera!"));
}
