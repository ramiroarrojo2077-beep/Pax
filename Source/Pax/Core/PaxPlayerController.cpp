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
#include "GameFramework/PlayerInput.h"

APaxPlayerController::APaxPlayerController()
{
	bShowMouseCursor = false;
	bAutoManageActiveCameraTarget = true;
}

void APaxPlayerController::BeginPlay()
{
	Super::BeginPlay();

#if PLATFORM_ANDROID || PLATFORM_IOS
	bTouchControlsEnabled = true;
#else
	bTouchControlsEnabled = bForceTouchControls;
#endif
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
	EnhancedInput->BindAction(TiltAction, ETriggerEvent::Triggered, this, &APaxPlayerController::HandleTilt);
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
	TiltAction = MakeAction(TEXT("IA_Tilt"), EInputActionValueType::Axis3D);

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

	// --- Sensores ----------------------------------------------------------
	// El acelerómetro sólo entrega datos en dispositivos que lo tienen; en
	// escritorio la acción existe pero nunca se dispara.
	DrivingContext->MapKey(TiltAction, EKeys::Tilt);
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

void APaxPlayerController::HandleTilt(const FInputActionValue& Value)
{
	LastTilt = Value.Get<FVector>();
}

void APaxPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	if (bTouchControlsEnabled)
	{
		UpdateTouchControls(DeltaTime);
	}
}

void APaxPlayerController::UpdateTouchControls(float DeltaTime)
{
	AF1Car* Car = GetCar();
	if (!Car || !PlayerInput)
	{
		return;
	}

	int32 SizeX = 0;
	int32 SizeY = 0;
	GetViewportSize(SizeX, SizeY);
	if (SizeX <= 0 || SizeY <= 0)
	{
		return;
	}

	const FVector2D ViewportSize(SizeX, SizeY);
	if (!TouchLayout.Matches(ViewportSize))
	{
		TouchLayout.Build(ViewportSize);
	}

	// --- Repartir los dedos -------------------------------------------------
	bool bButtonDown[PaxNumTouchButtons] = {};
	bool bThrottleHeld = false;
	bool bBrakeHeld = false;
	bool bSteerHeld = false;

	// Se lee de UPlayerInput::Touches en vez de GetInputTouchState porque es
	// un array plano y estable: posición en píxeles del viewport en X e Y, y
	// Z distinto de cero mientras el dedo siga apoyado.
	for (int32 Finger = 0; Finger < static_cast<int32>(EKeys::NUM_TOUCH_KEYS); ++Finger)
	{
		const FVector& Touch = PlayerInput->Touches[Finger];
		const bool bPressed = Touch.Z != 0.f;

		if (!bPressed)
		{
			if (SteeringFinger == Finger)
			{
				SteeringFinger = INDEX_NONE;
			}
			continue;
		}

		const FVector2D Point(Touch.X, Touch.Y);


		// El dedo que ha tomado el volante lo conserva aunque salga de la zona:
		// si no, un giro amplio se cortaría en seco al cruzar el borde.
		const bool bOwnsSteering = (SteeringFinger == Finger);
		if (bOwnsSteering || (SteeringFinger == INDEX_NONE && TouchLayout.SteerArea.IsInside(Point)))
		{
			if (!bOwnsSteering)
			{
				SteeringFinger = Finger;
				TouchState.SteerAnchor = Point;
			}
			TouchState.SteerCurrent = Point;
			bSteerHeld = true;
			continue;
		}

		if (TouchLayout.ThrottlePedal.IsInside(Point))
		{
			bThrottleHeld = true;
			continue;
		}

		if (TouchLayout.BrakePedal.IsInside(Point))
		{
			bBrakeHeld = true;
			continue;
		}

		for (int32 Index = 0; Index < PaxNumTouchButtons; ++Index)
		{
			if (TouchLayout.Buttons[Index].IsInside(Point))
			{
				bButtonDown[Index] = true;
				break;
			}
		}
	}

	TouchState.bSteerActive = bSteerHeld;

	// --- Dirección ----------------------------------------------------------
	float TargetSteering = 0.f;
	if (SteeringMode == EPaxSteeringMode::Tilt)
	{
		const float Raw = static_cast<float>(LastTilt.Component(FMath::Clamp(TiltAxisIndex, 0, 2)));
		const float Degrees = FMath::RadiansToDegrees(Raw);
		const float Deadzoned = FMath::Sign(Degrees) * FMath::Max(FMath::Abs(Degrees) - TiltDeadzoneDegrees, 0.f);
		TargetSteering = FMath::Clamp(Deadzoned / FMath::Max(TiltFullLockDegrees, 1.f), -1.f, 1.f);
		if (bInvertTilt)
		{
			TargetSteering = -TargetSteering;
		}
	}
	else if (bSteerHeld)
	{
		const float Offset = static_cast<float>(TouchState.SteerCurrent.X - TouchState.SteerAnchor.X);
		TargetSteering = FMath::Clamp(Offset / FMath::Max(TouchLayout.SteerRadius, 1.f), -1.f, 1.f);
	}

	// Al soltar, el volante vuelve al centro solo, como un volante de verdad.
	TouchState.Steering = FMath::FInterpTo(TouchState.Steering, TargetSteering, DeltaTime, SteeringResponseRate);

	// --- Pedales ------------------------------------------------------------
	// Un pedal táctil es un interruptor, pero pisarlo de golpe hace patinar el
	// coche al salir de una curva lenta: la rampa da el medio gas que en un
	// mando entrega el gatillo.
	TouchState.Throttle = FMath::FInterpTo(TouchState.Throttle, bThrottleHeld ? 1.f : 0.f, DeltaTime, PedalResponseRate);
	TouchState.Brake = FMath::FInterpTo(TouchState.Brake, bBrakeHeld ? 1.f : 0.f, DeltaTime, PedalResponseRate * 2.f);

	Car->SetSteering(TouchState.Steering);
	Car->SetThrottle(TouchState.Throttle);
	Car->SetBrake(TouchState.Brake);

	// --- Botones ------------------------------------------------------------
	for (int32 Index = 0; Index < PaxNumTouchButtons; ++Index)
	{
		ApplyTouchButton(static_cast<EPaxTouchButton>(Index), bButtonDown[Index], PreviousButtons[Index]);
		TouchState.bButtonDown[Index] = bButtonDown[Index];
		PreviousButtons[Index] = bButtonDown[Index];
	}
}

void APaxPlayerController::ApplyTouchButton(EPaxTouchButton Button, bool bDown, bool bWasDown)
{
	AF1Car* Car = GetCar();
	if (!Car)
	{
		return;
	}

	// El DRS se mantiene pulsado; el resto son pulsaciones sueltas.
	if (Button == EPaxTouchButton::DRS)
	{
		if (bDown && !bWasDown)
		{
			Car->RequestDRS();
		}
		else if (!bDown && bWasDown)
		{
			Car->ReleaseDRS();
		}
		return;
	}

	if (!bDown || bWasDown)
	{
		return;
	}

	switch (Button)
	{
	case EPaxTouchButton::ERS:       Car->CycleERSMode(); break;
	case EPaxTouchButton::Mix:       Car->CycleFuelMix(); break;
	case EPaxTouchButton::Camera:    Car->ToggleCameraView(); break;
	case EPaxTouchButton::Recover:   Car->RecoverToTrack(); break;
	case EPaxTouchButton::ShiftUp:   Car->ShiftUp(); break;
	case EPaxTouchButton::ShiftDown: Car->ShiftDown(); break;
	default: break;
	}
}
