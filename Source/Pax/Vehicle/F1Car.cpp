// Copyright Pax. All Rights Reserved.

#include "F1Car.h"
#include "Pax.h"
#include "F1Wheel.h"
#include "Components/AeroComponent.h"
#include "Components/TyreComponent.h"
#include "Components/ERSComponent.h"
#include "Components/FuelComponent.h"
#include "Components/DRSComponent.h"
#include "Components/PaxPhysicsUnits.h"
#include "Core/PaxGameState.h"
#include "Track/TrackSpline.h"

#include "ChaosWheeledVehicleMovementComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"

AF1Car::AF1Car(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	// Después de la física para que la telemetría del frame refleje el estado
	// ya resuelto; las fuerzas que dependen de ella se aplican en TG_PrePhysics
	// desde los propios componentes.
	PrimaryActorTick.TickGroup = TG_PostPhysics;

	ConfigurePowertrain();

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetMesh());
	CameraBoom->TargetArmLength = 750.f;
	CameraBoom->SocketOffset = FVector(0.f, 0.f, 180.f);
	CameraBoom->bUsePawnControlRotation = false;
	CameraBoom->bInheritPitch = false;
	CameraBoom->bInheritRoll = false;
	// Un poco de retardo en la cámara transmite la velocidad; demasiado marea.
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 12.f;
	CameraBoom->bEnableCameraRotationLag = true;
	CameraBoom->CameraRotationLagSpeed = 9.f;

	ChaseCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ChaseCamera"));
	ChaseCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	ChaseCamera->FieldOfView = 95.f;

	CockpitCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("CockpitCamera"));
	CockpitCamera->SetupAttachment(GetMesh());
	// A la altura del casco, justo detrás del halo.
	CockpitCamera->SetRelativeLocation(FVector(20.f, 0.f, 95.f));
	CockpitCamera->FieldOfView = 90.f;
	CockpitCamera->SetActive(false);

	Aero = CreateDefaultSubobject<UAeroComponent>(TEXT("Aero"));
	Tyres = CreateDefaultSubobject<UTyreComponent>(TEXT("Tyres"));
	ERS = CreateDefaultSubobject<UERSComponent>(TEXT("ERS"));
	Fuel = CreateDefaultSubobject<UFuelComponent>(TEXT("Fuel"));
	DRS = CreateDefaultSubobject<UDRSComponent>(TEXT("DRS"));
}

void AF1Car::ConfigurePowertrain()
{
	UChaosWheeledVehicleMovementComponent* Movement =
		Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent());
	if (!Movement)
	{
		return;
	}

	Movement->bMechanicalSimEnabled = true;

	// --- Motor -------------------------------------------------------------
	// V6 turbo híbrido: unos 750 CV térmicos con pico de par sobre 10.500 rpm
	// y corte en 15.000. El empuje eléctrico no va aquí: lo añade el ERS como
	// fuerza aparte, porque su disponibilidad depende de la batería.
	Movement->EngineSetup.MaxTorque = 520.f;
	Movement->EngineSetup.MaxRPM = 15000.f;
	Movement->EngineSetup.EngineIdleRPM = 4000.f;
	Movement->EngineSetup.EngineBrakeEffect = 0.15f;
	Movement->EngineSetup.EngineRevUpMOI = 3.f;
	Movement->EngineSetup.EngineRevDownRate = 900.f;

	if (FRichCurve* TorqueCurve = Movement->EngineSetup.TorqueCurve.GetRichCurve())
	{
		TorqueCurve->Reset();
		TorqueCurve->AddKey(4000.f, 0.55f);
		TorqueCurve->AddKey(7000.f, 0.78f);
		TorqueCurve->AddKey(9000.f, 0.94f);
		TorqueCurve->AddKey(10500.f, 1.00f);
		TorqueCurve->AddKey(12000.f, 0.97f);
		TorqueCurve->AddKey(13500.f, 0.88f);
		TorqueCurve->AddKey(15000.f, 0.72f);
	}

	// --- Caja de cambios ---------------------------------------------------
	// Ocho marchas, cambio casi instantáneo. El desarrollo está calculado para
	// topar sobre 325 km/h con 15.000 rpm y ruedas de 36 cm de radio.
	Movement->TransmissionSetup.bUseAutomaticGears = true;
	Movement->TransmissionSetup.bUseAutoReverse = false;
	Movement->TransmissionSetup.FinalRatio = 6.0f;
	Movement->TransmissionSetup.ForwardGearRatios = { 3.60f, 2.85f, 2.35f, 1.98f, 1.70f, 1.48f, 1.28f, 1.03f };
	Movement->TransmissionSetup.ReverseGearRatios = { 4.0f };
	Movement->TransmissionSetup.ChangeUpRPM = 14500.f;
	Movement->TransmissionSetup.ChangeDownRPM = 9500.f;
	Movement->TransmissionSetup.GearChangeTime = 0.04f;
	Movement->TransmissionSetup.TransmissionEfficiency = 0.95f;

	// --- Transmisión y dirección -------------------------------------------
	Movement->DifferentialSetup.DifferentialType = EVehicleDifferential::RearWheelDrive;

	Movement->SteeringSetup.SteeringType = ESteeringType::AngleRatio;
	Movement->SteeringSetup.AngleRatio = 0.7f;
	if (FRichCurve* SteeringCurve = Movement->SteeringSetup.SteeringCurve.GetRichCurve())
	{
		// Limitar el ángulo con la velocidad es lo que evita que un toque de
		// volante a 300 km/h tire el coche. Es el equivalente digital de la
		// desmultiplicación variable de la cremallera.
		SteeringCurve->Reset();
		SteeringCurve->AddKey(0.f, 1.00f);
		SteeringCurve->AddKey(60.f, 0.80f);
		SteeringCurve->AddKey(120.f, 0.55f);
		SteeringCurve->AddKey(200.f, 0.35f);
		SteeringCurve->AddKey(320.f, 0.22f);
	}

	// --- Chasis y ruedas ---------------------------------------------------
	// La carga aerodinámica la lleva UAeroComponent, que reparte por ejes;
	// el coeficiente propio de Chaos se deja a cero para no duplicarla.
	Movement->DragCoefficient = 0.f;
	Movement->DownforceCoefficient = 0.f;
	Movement->ChassisWidth = 200.f;
	Movement->ChassisHeight = 95.f;
	// Un monoplaza es largo y estrecho: cuesta más girarlo sobre su eje
	// vertical que hacerlo cabecear.
	Movement->InertiaTensorScale = FVector(1.0f, 1.25f, 1.35f);

	// Los nombres de hueso deben coincidir con el rig generado en Blender.
	Movement->WheelSetups.SetNum(4);
	Movement->WheelSetups[0].WheelClass = UF1WheelFront::StaticClass();
	Movement->WheelSetups[0].BoneName = FName("Wheel_FL");
	Movement->WheelSetups[1].WheelClass = UF1WheelFront::StaticClass();
	Movement->WheelSetups[1].BoneName = FName("Wheel_FR");
	Movement->WheelSetups[2].WheelClass = UF1WheelRear::StaticClass();
	Movement->WheelSetups[2].BoneName = FName("Wheel_RL");
	Movement->WheelSetups[3].WheelClass = UF1WheelRear::StaticClass();
	Movement->WheelSetups[3].BoneName = FName("Wheel_RR");
}

void AF1Car::BeginPlay()
{
	Super::BeginPlay();

	Track = ATrackSpline::Get(GetWorld());

	if (USkeletalMeshComponent* Body = GetMesh())
	{
		if (!Body->GetSkeletalMeshAsset())
		{
			UE_LOG(LogPax, Warning,
				TEXT("'%s' no tiene malla esqueletal asignada: exporta el coche con Tools/Blender/build_assets.py e impórtalo antes de correr."),
				*GetName());
		}

		Body->SetMassOverrideInKg(NAME_None, DryMassKg, true);
		// Centro de gravedad bajo y ligeramente retrasado, como el real.
		Body->SetCenterOfMass(FVector(-20.f, 0.f, -8.f));

		if (Aero)
		{
			Aero->Initialize(Body, FrontAxleOffsetCm, RearAxleOffsetCm);
		}
	}

	// Los modelos que razonan por vuelta necesitan saber cuánto mide la vuelta:
	// el mismo compuesto dura la mitad de vueltas en un trazado el doble de largo.
	if (Track && Track->GetTrackLength() > 0.f)
	{
		const float LapLengthM = Track->GetTrackLength() * 0.01f;
		if (Tyres)
		{
			Tyres->ConfigureForTrack(LapLengthM);
		}
		if (Fuel)
		{
			Fuel->ConfigureForTrack(LapLengthM);
		}
	}

	if (Fuel)
	{
		CurrentFuelKg = Fuel->GetFuelKg();
	}

	PreviousVelocity = GetVelocity();
}

void AF1Car::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (DeltaTime <= 0.f)
	{
		return;
	}

	UpdateTelemetry(DeltaTime);

	if (Tyres)
	{
		Tyres->UpdateModel(DeltaTime, Telemetry);
		ApplyTyreGrip();
	}

	if (Fuel)
	{
		const float TorqueScale = Fuel->UpdateModel(DeltaTime, Telemetry);
		CurrentFuelKg = Fuel->GetFuelKg();
		UpdateMassFromFuel();

		// Sin gasolina el motor se apaga: se corta el acelerador en seco.
		if (TorqueScale <= 0.f)
		{
			if (UChaosWheeledVehicleMovementComponent* Movement =
				Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent()))
			{
				Movement->SetThrottleInput(0.f);
			}
		}
	}

	if (ERS)
	{
		ApplyERSBoost(ERS->UpdateModel(DeltaTime, Telemetry));
	}

	if (DRS)
	{
		float GapAhead = TNumericLimits<float>::Max();
		if (const APaxGameState* RaceState = GetWorld()->GetGameState<APaxGameState>())
		{
			GapAhead = RaceState->GetGapAheadSeconds(this);
		}

		DRS->UpdateModel(DeltaTime, Telemetry, LapDistanceCm, GapAhead);

		if (Aero)
		{
			Aero->SetDRSOpen(DRS->IsOpen());
		}
	}

	UpdateRecovery(DeltaTime);
}

void AF1Car::UpdateTelemetry(float DeltaTime)
{
	UChaosWheeledVehicleMovementComponent* Movement =
		Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent());

	const FVector Velocity = GetVelocity();
	const FTransform& Frame = GetActorTransform();
	const FVector LocalVelocity = Frame.InverseTransformVectorNoScale(Velocity);

	Telemetry.SpeedKph = LocalVelocity.X * PaxUnits::CmsToKph;

	// Aceleración en G a partir de la variación de velocidad en el frame.
	const FVector Acceleration = (Velocity - PreviousVelocity) / DeltaTime; // cm/s²
	const FVector LocalAcceleration = Frame.InverseTransformVectorNoScale(Acceleration);
	// 1 G = 980 cm/s²
	Telemetry.LongitudinalG = LocalAcceleration.X / 980.f;
	Telemetry.LateralG = LocalAcceleration.Y / 980.f;
	PreviousVelocity = Velocity;

	Telemetry.SlipAngleDeg = FMath::Abs(LocalVelocity.X) > 50.f
		? FMath::RadiansToDegrees(FMath::Atan2(LocalVelocity.Y, FMath::Abs(LocalVelocity.X)))
		: 0.f;

	Telemetry.ThrottleInput = ThrottleInput;
	Telemetry.BrakeInput = BrakeInput;
	Telemetry.SteeringInput = SteeringInput;
	Telemetry.Gear = Movement ? Movement->GetCurrentGear() : 0;
	Telemetry.EngineRPM = Movement ? Movement->GetEngineRotationSpeed() : 0.f;
	Telemetry.DownforceN = Aero ? Aero->GetDownforceNewtons() : 0.f;
	Telemetry.DistanceTravelledM = FMath::Abs(LocalVelocity.X) * PaxUnits::CmsToMs * DeltaTime;

	if (Track)
	{
		LapDistanceCm = Track->GetDistanceAtLocation(GetActorLocation());
		Telemetry.Surface = Track->GetSurfaceAtLocation(GetActorLocation());
	}

	// Con las cuatro ruedas en el aire no hay ni agarre ni efecto suelo.
	if (UWorld* World = GetWorld())
	{
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(PaxAirborne), false, this);
		const FVector Start = GetActorLocation();
		const FVector End = Start - GetActorUpVector() * 120.f;
		Telemetry.bAirborne = !World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
	}
}

void AF1Car::ApplyERSBoost(float PowerWatts)
{
	if (PowerWatts <= 0.f)
	{
		return;
	}

	USkeletalMeshComponent* Body = GetMesh();
	if (!Body || !Body->IsSimulatingPhysics())
	{
		return;
	}

	// Potencia a fuerza: F = P / v. Por debajo de unos 50 km/h se satura para
	// no generar un empujón irreal (y para no hacer patinar las ruedas, que en
	// el coche real es justo lo que limita el despliegue a baja velocidad).
	const float SpeedMs = FMath::Max(FMath::Abs(Telemetry.SpeedKph) / 3.6f, 14.f);
	const float ForceN = PowerWatts / SpeedMs;

	const FVector RearAxle = GetActorTransform().TransformPosition(FVector(RearAxleOffsetCm, 0.f, 0.f));
	Body->AddForceAtLocation(GetActorForwardVector() * ForceN * PaxUnits::NewtonsToUnreal, RearAxle);
}

void AF1Car::ApplyTyreGrip()
{
	UChaosWheeledVehicleMovementComponent* Movement =
		Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent());
	if (!Movement || !Tyres)
	{
		return;
	}

	const int32 NumWheels = Movement->Wheels.Num();
	for (int32 Index = 0; Index < NumWheels; ++Index)
	{
		const float Friction = BaseWheelFriction * Tyres->GetWheelGripMultiplier(Index);

#if PAX_HAS_WHEEL_RUNTIME_API
		Movement->SetWheelFrictionMultiplier(Index, Friction);
#else
		// En 5.1 y 5.2 no hay setter en runtime: se escribe en la configuración
		// de la rueda, que Chaos relee en el siguiente paso de simulación.
		if (UChaosVehicleWheel* Wheel = Movement->Wheels[Index])
		{
			Wheel->FrictionForceMultiplier = Friction;
		}
#endif
	}
}

void AF1Car::UpdateMassFromFuel()
{
	// Reescribir la masa cada frame invalidaría el estado del cuerpo rígido
	// sin necesidad: medio kilo de diferencia es un cambio imperceptible.
	if (FMath::Abs(CurrentFuelKg - AppliedFuelKg) < 0.5f)
	{
		return;
	}

	if (USkeletalMeshComponent* Body = GetMesh())
	{
		Body->SetMassOverrideInKg(NAME_None, DryMassKg + CurrentFuelKg, true);
		AppliedFuelKg = CurrentFuelKg;
	}
}

void AF1Car::UpdateRecovery(float DeltaTime)
{
	const bool bUpsideDown = GetActorUpVector().Z < 0.2f;
	const bool bStopped = FMath::Abs(Telemetry.SpeedKph) < 5.f;
	const bool bOffTrack = Telemetry.Surface == ETrackSurface::Gravel || Telemetry.Surface == ETrackSurface::Grass;

	if (bUpsideDown || (bStopped && bOffTrack))
	{
		StuckTimer += DeltaTime;
		if (StuckTimer >= RecoveryDelaySeconds)
		{
			RecoverToTrack();
		}
	}
	else
	{
		StuckTimer = 0.f;
	}
}

void AF1Car::RecoverToTrack()
{
	StuckTimer = 0.f;

	if (!Track)
	{
		return;
	}

	USkeletalMeshComponent* Body = GetMesh();
	if (!Body)
	{
		return;
	}

	// Se recoloca algo por detrás del punto de salida para no reaparecer
	// encima de quien venga detrás por la trazada.
	const FTransform Frame = Track->GetTransformAtDistance(LapDistanceCm - 500.f);

	Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
	Body->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);

	FTransform Target;
	Target.SetLocation(Frame.GetLocation() + FVector(0.f, 0.f, 60.f));
	Target.SetRotation(FRotator(0.f, Frame.Rotator().Yaw, 0.f).Quaternion());
	SetActorTransform(Target, false, nullptr, ETeleportType::TeleportPhysics);

	UE_LOG(LogPax, Verbose, TEXT("'%s' recolocado en pista en la distancia %.0f m."), *GetName(), LapDistanceCm / 100.f);
}

// ---------------------------------------------------------------------------
// Entradas
// ---------------------------------------------------------------------------

void AF1Car::SetControlsLocked(bool bLocked)
{
	bControlsLocked = bLocked;

	if (UChaosWheeledVehicleMovementComponent* Movement =
		Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent()))
	{
		if (bLocked)
		{
			ThrottleInput = 0.f;
			Movement->SetThrottleInput(0.f);
			// El freno de mano mantiene el coche quieto en la parrilla sin
			// tener que congelar la física.
			Movement->SetHandbrakeInput(true);
		}
		else
		{
			Movement->SetHandbrakeInput(false);
		}
	}
}

void AF1Car::SetThrottle(float Value)
{
	ThrottleInput = bControlsLocked ? 0.f : FMath::Clamp(Value, 0.f, 1.f);

	if (UChaosWheeledVehicleMovementComponent* Movement =
		Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent()))
	{
		Movement->SetThrottleInput(ThrottleInput);
	}
}

void AF1Car::SetBrake(float Value)
{
	BrakeInput = FMath::Clamp(Value, 0.f, 1.f);

	if (UChaosWheeledVehicleMovementComponent* Movement =
		Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent()))
	{
		Movement->SetBrakeInput(BrakeInput);
	}

	// Frenar cierra el DRS, como en el coche real.
	if (BrakeInput > 0.05f && DRS)
	{
		DRS->Close();
	}
}

void AF1Car::SetSteering(float Value)
{
	SteeringInput = bControlsLocked ? 0.f : FMath::Clamp(Value, -1.f, 1.f);

	if (UChaosWheeledVehicleMovementComponent* Movement =
		Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent()))
	{
		Movement->SetSteeringInput(SteeringInput);
	}
}

void AF1Car::ShiftUp()
{
	if (bControlsLocked)
	{
		return;
	}

	if (UChaosWheeledVehicleMovementComponent* Movement =
		Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent()))
	{
		// El primer cambio manual saca la caja del modo automático: quien
		// quiere cambiar, cambia; quien no toca las levas, no se entera.
		Movement->SetUseAutomaticGears(false);
		// Marcha objetivo en lugar de pulso: una leva es un evento puntual y
		// SetChangeUpInput(true) se quedaría enganchado hasta soltarlo.
		const int32 MaxGear = Movement->TransmissionSetup.ForwardGearRatios.Num();
		Movement->SetTargetGear(FMath::Min(Movement->GetTargetGear() + 1, MaxGear), true);
	}
}

void AF1Car::ShiftDown()
{
	if (bControlsLocked)
	{
		return;
	}

	if (UChaosWheeledVehicleMovementComponent* Movement =
		Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent()))
	{
		Movement->SetUseAutomaticGears(false);
		Movement->SetTargetGear(FMath::Max(Movement->GetTargetGear() - 1, 0), true);
	}
}

void AF1Car::RequestDRS()
{
	if (!bControlsLocked && DRS)
	{
		DRS->RequestOpen();
	}
}

void AF1Car::ReleaseDRS()
{
	if (DRS)
	{
		DRS->Close();
	}
}

void AF1Car::CycleERSMode()
{
	if (ERS)
	{
		ERS->CycleMode();
	}
}

void AF1Car::CycleFuelMix()
{
	if (Fuel)
	{
		Fuel->CycleMix();
	}
}

void AF1Car::ToggleCameraView()
{
	bCockpitView = !bCockpitView;
	if (ChaseCamera)
	{
		ChaseCamera->SetActive(!bCockpitView);
	}
	if (CockpitCamera)
	{
		CockpitCamera->SetActive(bCockpitView);
	}
}

float AF1Car::GetMaxEngineRPM() const
{
	if (const UChaosWheeledVehicleMovementComponent* Movement =
		Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent()))
	{
		return Movement->EngineSetup.MaxRPM;
	}
	return 15000.f;
}

float AF1Car::GetDRSFlapAlpha() const
{
	return DRS ? DRS->GetFlapAlpha() : 0.f;
}
