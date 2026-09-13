// Copyright Pax. All Rights Reserved.

#include "AeroComponent.h"
#include "PaxPhysicsUnits.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "GameFramework/Pawn.h"

UAeroComponent::UAeroComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// Las fuerzas deben entrar antes del paso de física del frame, si no se
	// aplican con un tick de retraso y el coche "flota" al cambiar de apoyo.
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UAeroComponent::Initialize(UPrimitiveComponent* InBody, float InFrontAxleOffsetCm, float InRearAxleOffsetCm)
{
	Body = InBody;
	FrontAxleOffsetCm = InFrontAxleOffsetCm;
	RearAxleOffsetCm = InRearAxleOffsetCm;
}

float UAeroComponent::GetDownforceKg() const
{
	return LastDownforceN / PaxUnits::GravityMs2;
}

float UAeroComponent::GetFrontGripBias() const
{
	// Sólo informativo para la IA y la telemetría: cuánta carga delantera
	// queda respecto a la nominal una vez descontado el aire sucio.
	return LastFrontShare / FMath::Max(AeroBalance, KINDA_SMALL_NUMBER);
}

void UAeroComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!Body || !Body->IsSimulatingPhysics())
	{
		return;
	}

	UpdateSlipstream();

	const FVector Velocity = Body->GetComponentVelocity();
	const FVector Forward = Body->GetForwardVector();
	const FVector Up = Body->GetUpVector();

	// Sólo cuenta la componente longitudinal: derrapando de lado el coche
	// pierde carga, que es exactamente lo que se busca reproducir.
	const float SpeedMs = FVector::DotProduct(Velocity, Forward) * PaxUnits::CmsToMs;
	const float SpeedSq = SpeedMs * SpeedMs;
	if (SpeedSq < 1.f)
	{
		LastDownforceN = 0.f;
		LastDragN = 0.f;
		return;
	}

	// Efecto suelo: la carga cae cuando el fondo se separa del asfalto.
	float GroundEffect = 1.f;
	if (UWorld* World = GetWorld())
	{
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(PaxAeroRideHeight), false, GetOwner());
		const FVector Start = Body->GetComponentLocation();
		const FVector End = Start - Up * 200.f;
		if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
		{
			const float RideHeightCm = Hit.Distance;
			GroundEffect = FMath::Clamp(GroundEffectReferenceHeightCm / FMath::Max(RideHeightCm, 1.f), 0.25f, 1.f);
		}
		else
		{
			GroundEffect = 0.25f; // en el aire apenas queda carga
		}
	}

	// --- Resistencia -------------------------------------------------------
	float DragScale = 1.f;
	DragScale *= (1.f - (bDRSOpen ? DRSDragReduction : 0.f));
	DragScale *= (1.f - SlipstreamFactor * SlipstreamDragReduction);

	LastDragN = DragCoefficient * SpeedSq * DragScale;
	Body->AddForce(-Forward * LastDragN * PaxUnits::NewtonsToUnreal);

	// --- Carga aerodinámica ------------------------------------------------
	const float TotalDownforceN = DownforceCoefficient * SpeedSq * GroundEffect;

	float FrontShare = AeroBalance;
	float RearShare = 1.f - AeroBalance;

	// Aire sucio: el coche que sigue pierde morro y subvira.
	FrontShare *= (1.f - SlipstreamFactor * DirtyAirFrontLoss);
	// DRS abierto: se descarga el tren trasero.
	if (bDRSOpen)
	{
		RearShare *= (1.f - DRSRearDownforceLoss);
	}

	const float FrontN = TotalDownforceN * FrontShare;
	const float RearN = TotalDownforceN * RearShare;
	LastDownforceN = FrontN + RearN;
	LastFrontShare = LastDownforceN > KINDA_SMALL_NUMBER ? FrontN / LastDownforceN : AeroBalance;

	const FVector FrontPoint = Body->GetComponentTransform().TransformPosition(FVector(FrontAxleOffsetCm, 0.f, 0.f));
	const FVector RearPoint = Body->GetComponentTransform().TransformPosition(FVector(RearAxleOffsetCm, 0.f, 0.f));

	Body->AddForceAtLocation(-Up * FrontN * PaxUnits::NewtonsToUnreal, FrontPoint);
	Body->AddForceAtLocation(-Up * RearN * PaxUnits::NewtonsToUnreal, RearPoint);
}

void UAeroComponent::UpdateSlipstream()
{
	UWorld* World = GetWorld();
	if (!World || !Body)
	{
		SlipstreamFactor = 0.f;
		return;
	}

	const FVector Start = Body->GetComponentLocation();
	const FVector End = Start + Body->GetForwardVector() * SlipstreamRangeCm;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(PaxSlipstream), false, GetOwner());
	FHitResult Hit;

	// Una esfera en vez de una línea: el rebufo se siente aunque no se esté
	// exactamente detrás, y evita perderlo por un pixel en curva.
	// La malla del monoplaza usa el perfil "Vehicle", así que se consulta por
	// tipo de objeto en lugar de por canal de traza.
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Vehicle);
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);

	const bool bHit = World->SweepSingleByObjectType(
		Hit, Start, End, FQuat::Identity, ObjectParams, FCollisionShape::MakeSphere(150.f), Params);

	float Target = 0.f;
	if (bHit && Hit.GetActor() && Hit.GetActor()->IsA(APawn::StaticClass()))
	{
		const float Distance = FMath::Max(Hit.Distance, 1.f);
		// Cae linealmente con la distancia; a menos de 10 m es casi total.
		Target = FMath::Clamp(1.f - (Distance - 1000.f) / FMath::Max(SlipstreamRangeCm - 1000.f, 1.f), 0.f, 1.f);
	}

	// Suavizado para que entrar y salir del rebufo no sea un escalón.
	SlipstreamFactor = FMath::FInterpTo(SlipstreamFactor, Target, World->GetDeltaSeconds(), 4.f);
}
