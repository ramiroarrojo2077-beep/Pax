// Copyright Pax. All Rights Reserved.

#include "F1AIController.h"
#include "Pax.h"
#include "Vehicle/F1Car.h"
#include "Vehicle/Components/DRSComponent.h"
#include "Vehicle/Components/ERSComponent.h"
#include "Vehicle/Components/TyreComponent.h"
#include "Vehicle/Components/PaxPhysicsUnits.h"
#include "Core/PaxGameState.h"
#include "Track/TrackSpline.h"
#include "Engine/World.h"

AF1AIController::AF1AIController()
{
	PrimaryActorTick.bCanEverTick = true;
	bAttachToPawn = false;
	// Los coches no usan navmesh: la pista es la spline, y no hace falta
	// PlayerState para nada, la clasificación la lleva el GameState.
	bWantsPlayerState = false;
}

void AF1AIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	ControlledCar = Cast<AF1Car>(InPawn);
	Track = ATrackSpline::Get(GetWorld());

	// Cada piloto arranca con una fase distinta para que el ruido de volante
	// no esté sincronizado en toda la parrilla.
	SteeringNoisePhase = FMath::FRandRange(0.f, 2.f * PI);
}

void AF1AIController::SetDifficulty(float InDifficulty)
{
	Difficulty = FMath::Clamp(InDifficulty, 0.f, 1.f);
}

float AF1AIController::GetUsableGrip() const
{
	// Un piloto no usa el 100% del agarre teórico: el mejor se acerca al 98%,
	// uno flojo se queda en el 86%. Ese 12% es casi todo el margen entre el
	// primero y el último de la parrilla.
	return PeakFrictionCoefficient * FMath::Lerp(0.86f, 0.98f, Difficulty);
}

float AF1AIController::ComputeCornerSpeedMs(float RadiusM, float CurrentSpeedMs) const
{
	// v² = a_lat · r, donde a_lat depende a su vez de la velocidad porque la
	// carga aerodinámica crece con ella. Se resuelve iterando: basta con tres
	// pasadas para converger con sobra.
	const float Mu = GetUsableGrip();
	float Speed = FMath::Max(CurrentSpeedMs, 10.f);

	for (int32 Iteration = 0; Iteration < 3; ++Iteration)
	{
		const float DownforceAccel = (DownforceCoefficient * Speed * Speed) / ReferenceMassKg;
		const float LateralAccel = Mu * (PaxUnits::GravityMs2 + DownforceAccel);
		Speed = FMath::Sqrt(FMath::Max(LateralAccel * RadiusM, 1.f));
	}

	return Speed;
}

float AF1AIController::ComputeTargetSpeedMs(float LapDistanceCm, float CurrentSpeedMs) const
{
	if (!Track)
	{
		return 0.f;
	}

	// Se escanea hacia delante y se busca la velocidad más restrictiva: para
	// cada curva futura, qué velocidad puedo llevar *ahora* y llegar frenado.
	float TargetMs = 400.f;
	constexpr float SampleStepM = 10.f;

	for (float AheadM = 0.f; AheadM <= BrakingLookaheadM; AheadM += SampleStepM)
	{
		const float SampleDistanceCm = LapDistanceCm + AheadM * 100.f;
		const float CurvaturePerCm = Track->GetCurvatureAtDistance(SampleDistanceCm);

		// Radio en metros, acotado: recta casi infinita abajo, horquilla arriba.
		const float RadiusM = FMath::Clamp(1.f / FMath::Max(CurvaturePerCm, KINDA_SMALL_NUMBER) * 0.01f, 15.f, 3000.f);
		const float CornerSpeedMs = ComputeCornerSpeedMs(RadiusM, CurrentSpeedMs);

		// v_permitida² = v_curva² + 2·a·s
		const float Mu = GetUsableGrip();
		const float DownforceAccel = (DownforceCoefficient * CurrentSpeedMs * CurrentSpeedMs) / ReferenceMassKg;
		const float BrakingAccel = Mu * (PaxUnits::GravityMs2 + DownforceAccel);

		const float AllowedMs = FMath::Sqrt(CornerSpeedMs * CornerSpeedMs + 2.f * BrakingAccel * AheadM);
		TargetMs = FMath::Min(TargetMs, AllowedMs);
	}

	// El estado del neumático recorta el ritmo: con la goma muerta el piloto
	// va más despacio en lugar de tirar el coche fuera.
	if (ControlledCar && ControlledCar->GetTyres())
	{
		TargetMs *= FMath::Sqrt(FMath::Clamp(ControlledCar->GetTyres()->GetWheelGripMultiplier(0), 0.3f, 1.2f));
	}

	return TargetMs;
}

void AF1AIController::UpdateRacingLineOffset(float DeltaTime, float LapDistanceCm, float CurrentSpeedMs)
{
	if (!Track || !ControlledCar)
	{
		return;
	}

	const APaxGameState* RaceState = GetWorld()->GetGameState<APaxGameState>();
	const float MaxOffsetCm = 400.f;

	// --- ¿Hay alguien delante? ---------------------------------------------
	const FVector Start = ControlledCar->GetActorLocation();
	const FVector Forward = ControlledCar->GetActorForwardVector();
	const float ScanRangeCm = FMath::Max(MinimumFollowingDistanceCm, CurrentSpeedMs * 100.f * 1.2f);

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PaxAIScan), false, ControlledCar);
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Vehicle);
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);

	const bool bCarAhead = GetWorld()->SweepSingleByObjectType(
		Hit, Start, Start + Forward * ScanRangeCm, FQuat::Identity,
		ObjectParams, FCollisionShape::MakeSphere(120.f), Params);

	if (bCarAhead && Hit.GetActor() && Hit.GetActor()->IsA(AF1Car::StaticClass()))
	{
		BlockedTimer += DeltaTime;

		// Elegir lado: el que tenga más pista libre. Con la trazada desplazada
		// hacia el interior, adelantar por fuera casi nunca sale bien, así que
		// se prefiere el lado hacia el que abre la curva siguiente.
		const float CurvatureAhead = Track->GetCurvatureAtDistance(LapDistanceCm + 8000.f);
		const FVector AheadPoint = Track->GetRacingLineLocationAtDistance(LapDistanceCm + 8000.f);
		const FVector ToAhead = (AheadPoint - Start).GetSafeNormal();
		const float TurnSign = FMath::Sign(FVector::CrossProduct(Forward, ToAhead).Z);

		// Se ataca por el interior de la curva que viene.
		const float PreferredSide = (CurvatureAhead > 1e-6f) ? TurnSign : (FMath::FRand() > 0.5f ? 1.f : -1.f);
		TargetOffsetCm = PreferredSide * MaxOffsetCm;

		// Si lleva demasiado tiempo bloqueado, prueba el otro lado.
		if (BlockedTimer > 4.f)
		{
			TargetOffsetCm = -TargetOffsetCm;
			BlockedTimer = 0.f;
		}
	}
	else
	{
		BlockedTimer = 0.f;

		// Sin nadie delante, vuelve a la trazada. Si le atacan por detrás y es
		// un piloto con oficio, cierra el interior una vez.
		float DefensiveOffset = 0.f;
		if (RaceState && Difficulty > 0.6f)
		{
			const float GapAhead = RaceState->GetGapAheadSeconds(ControlledCar);
			// Gap grande hacia delante y posición que defender: se coloca a un
			// lado para no dejar el interior libre en la frenada siguiente.
			if (GapAhead > 2.f)
			{
				const float CurvatureAhead = Track->GetCurvatureAtDistance(LapDistanceCm + 6000.f);
				if (CurvatureAhead > 1e-6f)
				{
					DefensiveOffset = 0.f; // en curva, trazada limpia
				}
			}
		}
		TargetOffsetCm = DefensiveOffset;
	}

	// Transición suave: cambiar de línea es un movimiento de un par de
	// segundos, no un salto lateral.
	RacingLineOffsetCm = FMath::FInterpTo(RacingLineOffsetCm, TargetOffsetCm, DeltaTime, 1.5f);
}

void AF1AIController::UpdateRaceSystems(float GapAheadSeconds)
{
	if (!ControlledCar)
	{
		return;
	}

	// DRS: si está disponible, se abre. No hay motivo para no hacerlo.
	if (UDRSComponent* DRS = ControlledCar->GetDRS())
	{
		if (DRS->GetState() == EDRSState::Available)
		{
			DRS->RequestOpen();
		}
	}

	// ERS: se gasta cuando sirve para algo —atacar a quien está a tiro o
	// defenderse— y se recupera cuando se rueda solo.
	if (UERSComponent* ERS = ControlledCar->GetERS())
	{
		if (GapAheadSeconds < 1.5f)
		{
			ERS->SetMode(EERSMode::Overtake);
		}
		else if (ERS->GetChargeFraction() < 0.3f)
		{
			ERS->SetMode(EERSMode::Harvest);
		}
		else
		{
			ERS->SetMode(EERSMode::Balanced);
		}
	}
}

void AF1AIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!ControlledCar)
	{
		ControlledCar = Cast<AF1Car>(GetPawn());
		if (!ControlledCar)
		{
			return;
		}
	}

	if (!Track)
	{
		Track = ATrackSpline::Get(GetWorld());
		if (!Track)
		{
			return;
		}
	}

	if (ControlledCar->AreControlsLocked())
	{
		// En parrilla: ni acelerar ni girar, sólo esperar.
		ControlledCar->SetThrottle(0.f);
		ControlledCar->SetBrake(0.f);
		ControlledCar->SetSteering(0.f);
		return;
	}

	const float LapDistanceCm = ControlledCar->GetLapDistanceCm();
	const float CurrentSpeedMs = FMath::Abs(ControlledCar->GetSpeedKph()) / 3.6f;

	UpdateRacingLineOffset(DeltaTime, LapDistanceCm, CurrentSpeedMs);

	// --- Dirección ---------------------------------------------------------
	// El punto al que se apunta se aleja con la velocidad: mirar cerca a 300
	// km/h produce el zigzag típico de una IA mal ajustada.
	const float LookaheadCm = FMath::Clamp(CurrentSpeedMs * 100.f * 0.65f, 1200.f, 9000.f);
	const FVector TargetPoint = Track->GetRacingLineLocationAtDistance(LapDistanceCm + LookaheadCm, RacingLineOffsetCm);

	const FVector LocalTarget = ControlledCar->GetActorTransform().InverseTransformPosition(TargetPoint);
	const float HeadingError = FMath::Atan2(LocalTarget.Y, FMath::Max(LocalTarget.X, 1.f));

	// PD sobre el error de rumbo. El término derivativo es el que impide que
	// el coche oscile alrededor de la trazada.
	const float Derivative = (HeadingError - PreviousHeadingError) / FMath::Max(DeltaTime, KINDA_SMALL_NUMBER);
	PreviousHeadingError = HeadingError;

	constexpr float SteeringP = 2.2f;
	constexpr float SteeringD = 0.12f;
	float Steering = SteeringP * HeadingError + SteeringD * Derivative;

	// Ruido lento: un piloto humano nunca traza dos vueltas idénticas, y los
	// pilotos menos capaces corrigen más.
	SteeringNoisePhase += DeltaTime * 1.7f;
	Steering += FMath::Sin(SteeringNoisePhase) * 0.02f * (1.f - Difficulty);

	ControlledCar->SetSteering(FMath::Clamp(Steering, -1.f, 1.f));

	// --- Acelerador y freno -------------------------------------------------
	const float TargetSpeedMs = ComputeTargetSpeedMs(LapDistanceCm, CurrentSpeedMs);
	const float SpeedErrorMs = TargetSpeedMs - CurrentSpeedMs;

	if (SpeedErrorMs > 1.f)
	{
		// Acelerar progresivamente: pisar a fondo saliendo de una curva lenta
		// es la forma más rápida de perder el tren trasero.
		const float TractionLimit = FMath::Clamp(CurrentSpeedMs / 25.f, 0.35f, 1.f);
		const float SlipPenalty = FMath::Clamp(1.f - FMath::Abs(ControlledCar->GetTelemetry().SlipAngleDeg) / 12.f, 0.2f, 1.f);
		ControlledCar->SetThrottle(FMath::Min(FMath::Clamp(SpeedErrorMs / 8.f, 0.f, 1.f), TractionLimit * SlipPenalty));
		ControlledCar->SetBrake(0.f);
	}
	else if (SpeedErrorMs < -0.5f)
	{
		ControlledCar->SetThrottle(0.f);
		// Frenada proporcional al exceso, saturando rápido: en un F1 la frenada
		// se ataca fuerte y se suelta al entrar en la curva.
		ControlledCar->SetBrake(FMath::Clamp(-SpeedErrorMs / 6.f, 0.f, 1.f));
	}
	else
	{
		ControlledCar->SetThrottle(0.25f);
		ControlledCar->SetBrake(0.f);
	}

	// --- Sistemas de carrera -------------------------------------------------
	float GapAhead = TNumericLimits<float>::Max();
	if (const APaxGameState* RaceState = GetWorld()->GetGameState<APaxGameState>())
	{
		GapAhead = RaceState->GetGapAheadSeconds(ControlledCar);
	}
	UpdateRaceSystems(GapAhead);
}
