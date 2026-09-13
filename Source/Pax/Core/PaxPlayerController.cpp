// Copyright Pax. All Rights Reserved.

#include "PaxPlayerController.h"
#include "Pax.h"
#include "Vehicle/F1Car.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "InputActionValue.h"

APaxPlayerController::APaxPlayerController()
{
	bShowMouseCursor = false;
	bAutoManageActiveCameraTarget = true;
}

AF1Car* APaxPlayerController::GetCar() const
{
	return Cast<AF1Car>(GetPawn());
}

void APaxPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// El contexto se añade al poseer: si el coche se recrea (reinicio de
	// sesión) los controles siguen funcionando sin tocar nada más.
	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
		{
			// AddMappingContext es idempotente: volver a añadir el mismo
			// contexto sólo refresca su prioridad.
			if (DrivingContext)
			{
				Subsystem->AddMappingContext(DrivingContext, 0);
			}
		}
	}
}

void APaxPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	BuildInputMappings();

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EnhancedInput)
	{
		UE_LOG(LogPax, Error, TEXT("Se esperaba un UEnhancedInputComponent; revisa DefaultInputComponentClass en DefaultInput.ini."));
		return;
	}

	// Triggered llega en cada frame que el eje tiene valor; Completed, cuando
	// se suelta. Sin el segundo el acelerador se quedaría clavado.
	EnhancedInput->BindAction(ThrottleAction, ETriggerEvent::Triggered, this, &APaxPlayerController::HandleThrottle);
	EnhancedInput->BindAction(ThrottleAction, ETriggerEvent::Completed, this, &APaxPlayerController::HandleThrottle);
	EnhancedInput->BindAction(BrakeAction, ETriggerEvent::Triggered, this, &APaxPlayerController::HandleBrake);
	EnhancedInput->BindAction(BrakeAction, ETriggerEvent::Completed, this, &APaxPlayerController::HandleBrake);
	EnhancedInput->BindAction(SteerAction, ETriggerEvent::Triggered, this, &APaxPlayerController::HandleSteer);
	EnhancedInput->BindAction(SteerAction, ETriggerEvent::Completed, this, &APaxPlayerController::HandleSteer);

	EnhancedInput->BindAction(ShiftUpAction, ETriggerEvent::Started, this, &APaxPlayerController::HandleShiftUp);
	EnhancedInput->BindAction(ShiftDownAction, ETriggerEvent::Started, this, &APaxPlayerController::HandleShiftDown);

	EnhancedInput->BindAction(DRSAction, ETriggerEvent::Started, this, &APaxPlayerController::HandleDRSPressed);
	EnhancedInput->BindAction(DRSAction, ETriggerEvent::Completed, this, &APaxPlayerController::HandleDRSReleased);

	EnhancedInput->BindAction(ERSAction, ETriggerEvent::Started, this, &APaxPlayerController::HandleERSMode);
	EnhancedInput->BindAction(FuelMixAction, ETriggerEvent::Started, this, &APaxPlayerController::HandleFuelMix);
	EnhancedInput->BindAction(CameraAction, ETriggerEvent::Started, this, &APaxPlayerController::HandleCamera);
	EnhancedInput->BindAction(RecoverAction, ETriggerEvent::Started, this, &APaxPlayerController::HandleRecover);
}

void APaxPlayerController::BuildInputMappings()
{
	if (DrivingContext)
	{
		return;
	}

	auto MakeAction = [this](const TCHAR* Name, EInputActionValueType Type) -> UInputAction*
	{
		UInputAction* Action = NewObject<UInputAction>(this, Name);
		Action->ValueType = Type;
		return Action;
	};

	ThrottleAction = MakeAction(TEXT("IA_Throttle"), EInputActionValueType::Axis1D);
	BrakeAction = MakeAction(TEXT("IA_Brake"), EInputActionValueType::Axis1D);
	SteerAction = MakeAction(TEXT("IA_Steer"), EInputActionValueType::Axis1D);
	ShiftUpAction = MakeAction(TEXT("IA_ShiftUp"), EInputActionValueType::Boolean);
	ShiftDownAction = MakeAction(TEXT("IA_ShiftDown"), EInputActionValueType::Boolean);
	DRSAction = MakeAction(TEXT("IA_DRS"), EInputActionValueType::Boolean);
	ERSAction = MakeAction(TEXT("IA_ERS"), EInputActionValueType::Boolean);
	FuelMixAction = MakeAction(TEXT("IA_FuelMix"), EInputActionValueType::Boolean);
	CameraAction = MakeAction(TEXT("IA_Camera"), EInputActionValueType::Boolean);
	RecoverAction = MakeAction(TEXT("IA_Recover"), EInputActionValueType::Boolean);

	DrivingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Driving"));

	// --- Acelerador y freno ------------------------------------------------
	DrivingContext->MapKey(ThrottleAction, EKeys::W);
	DrivingContext->MapKey(ThrottleAction, EKeys::Up);
	DrivingContext->MapKey(ThrottleAction, EKeys::Gamepad_RightTriggerAxis);

	DrivingContext->MapKey(BrakeAction, EKeys::S);
	DrivingContext->MapKey(BrakeAction, EKeys::Down);
	DrivingContext->MapKey(BrakeAction, EKeys::Gamepad_LeftTriggerAxis);

	// --- Dirección ---------------------------------------------------------
	// El stick ya da -1..1; el teclado necesita que una de las dos teclas
	// entregue el valor negativo.
	DrivingContext->MapKey(SteerAction, EKeys::Gamepad_LeftX);
	DrivingContext->MapKey(SteerAction, EKeys::D);
	DrivingContext->MapKey(SteerAction, EKeys::Right);

	{
		FEnhancedActionKeyMapping& LeftA = DrivingContext->MapKey(SteerAction, EKeys::A);
		LeftA.Modifiers.Add(NewObject<UInputModifierNegate>(this));

		FEnhancedActionKeyMapping& LeftArrow = DrivingContext->MapKey(SteerAction, EKeys::Left);
		LeftArrow.Modifiers.Add(NewObject<UInputModifierNegate>(this));
	}

	// --- Levas -------------------------------------------------------------
	DrivingContext->MapKey(ShiftUpAction, EKeys::E);
	DrivingContext->MapKey(ShiftUpAction, EKeys::Gamepad_RightShoulder);
	DrivingContext->MapKey(ShiftDownAction, EKeys::Q);
	DrivingContext->MapKey(ShiftDownAction, EKeys::Gamepad_LeftShoulder);

	// --- Sistemas ----------------------------------------------------------
	DrivingContext->MapKey(DRSAction, EKeys::SpaceBar);
	DrivingContext->MapKey(DRSAction, EKeys::Gamepad_FaceButton_Bottom);
	DrivingContext->MapKey(ERSAction, EKeys::One);
	DrivingContext->MapKey(ERSAction, EKeys::Gamepad_FaceButton_Left);
	DrivingContext->MapKey(FuelMixAction, EKeys::Two);
	DrivingContext->MapKey(FuelMixAction, EKeys::Gamepad_FaceButton_Right);
	DrivingContext->MapKey(CameraAction, EKeys::C);
	DrivingContext->MapKey(CameraAction, EKeys::Gamepad_FaceButton_Top);
	DrivingContext->MapKey(RecoverAction, EKeys::R);
	DrivingContext->MapKey(RecoverAction, EKeys::Gamepad_Special_Right);
}

void APaxPlayerController::HandleThrottle(const FInputActionValue& Value)
{
	if (AF1Car* Car = GetCar())
	{
		Car->SetThrottle(Value.Get<float>());
	}
}

void APaxPlayerController::HandleBrake(const FInputActionValue& Value)
{
	if (AF1Car* Car = GetCar())
	{
		Car->SetBrake(Value.Get<float>());
	}
}

void APaxPlayerController::HandleSteer(const FInputActionValue& Value)
{
	if (AF1Car* Car = GetCar())
	{
		Car->SetSteering(Value.Get<float>());
	}
}

void APaxPlayerController::HandleShiftUp()
{
	if (AF1Car* Car = GetCar())
	{
		Car->ShiftUp();
	}
}

void APaxPlayerController::HandleShiftDown()
{
	if (AF1Car* Car = GetCar())
	{
		Car->ShiftDown();
	}
}

void APaxPlayerController::HandleDRSPressed()
{
	if (AF1Car* Car = GetCar())
	{
		Car->RequestDRS();
	}
}

void APaxPlayerController::HandleDRSReleased()
{
	if (AF1Car* Car = GetCar())
	{
		Car->ReleaseDRS();
	}
}

void APaxPlayerController::HandleERSMode()
{
	if (AF1Car* Car = GetCar())
	{
		Car->CycleERSMode();
	}
}

void APaxPlayerController::HandleFuelMix()
{
	if (AF1Car* Car = GetCar())
	{
		Car->CycleFuelMix();
	}
}

void APaxPlayerController::HandleCamera()
{
	if (AF1Car* Car = GetCar())
	{
		Car->ToggleCameraView();
	}
}

void APaxPlayerController::HandleRecover()
{
	if (AF1Car* Car = GetCar())
	{
		Car->RecoverToTrack();
	}
}
