// Copyright Pax. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "UI/PaxTouchLayout.h"
#include "PaxPlayerController.generated.h"

class AF1Car;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

/** Cómo se dirige el coche con el móvil en la mano. */
UENUM(BlueprintType)
enum class EPaxSteeringMode : uint8
{
	/** Volante flotante: el pulgar se apoya donde cae y arrastra. */
	TouchWheel,
	/** Inclinando el teléfono. */
	Tilt
};

/**
 * Mando del jugador.
 *
 * Enhanced Input, pero con las acciones y el contexto creados en código en
 * lugar de como assets binarios. Cuesta unas líneas más y a cambio todo el
 * mapeo de controles queda en el repositorio como texto: se revisa en un diff,
 * se cambia sin abrir el editor y no hay un .uasset que resolver a mano cuando
 * dos personas tocan los controles a la vez.
 *
 * En Android se añade encima un mando en pantalla. No se usa el interfaz táctil
 * que trae el motor porque necesita un asset de texturas y sólo da dos joysticks
 * virtuales, que para conducir es justo lo que peor funciona: aquí hay pedales
 * separados, volante flotante y botones para los sistemas del coche.
 */
UCLASS(Config = Game)
class PAX_API APaxPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	APaxPlayerController();

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void PlayerTick(float DeltaTime) override;

	UFUNCTION(BlueprintPure, Category = "Pax|Touch")
	bool AreTouchControlsEnabled() const { return bTouchControlsEnabled; }

	const FPaxTouchLayout& GetTouchLayout() const { return TouchLayout; }
	const FPaxTouchState& GetTouchState() const { return TouchState; }

protected:
	/** Crea acciones y contexto y los registra en el subsistema. */
	void BuildInputMappings();

	void HandleThrottle(const FInputActionValue& Value);
	void HandleBrake(const FInputActionValue& Value);
	void HandleSteer(const FInputActionValue& Value);
	void HandleShiftUp();
	void HandleShiftDown();
	void HandleDRSPressed();
	void HandleDRSReleased();
	void HandleERSMode();
	void HandleFuelMix();
	void HandleCamera();
	void HandleRecover();
	void HandleTilt(const FInputActionValue& Value);

	/** Lee los dedos apoyados y traduce el mando en pantalla a entradas. */
	void UpdateTouchControls(float DeltaTime);

	/** Aplica un botón del mando táctil, distinguiendo pulsar de mantener. */
	void ApplyTouchButton(EPaxTouchButton Button, bool bDown, bool bWasDown);

	AF1Car* GetCar() const;

	/** Fuerza el mando en pantalla fuera de móvil, para probarlo en el editor. */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Pax|Touch")
	bool bForceTouchControls = false;

	UPROPERTY(EditDefaultsOnly, Config, Category = "Pax|Touch")
	EPaxSteeringMode SteeringMode = EPaxSteeringMode::TouchWheel;

	/** Grados de inclinación que equivalen a giro completo. */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Pax|Touch")
	float TiltFullLockDegrees = 28.f;

	/** Zona muerta de la inclinación, en grados. */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Pax|Touch")
	float TiltDeadzoneDegrees = 3.f;

	/**
	 * Componente del vector de inclinación que se usa como volante.
	 * Qué eje corresponde al balanceo depende del montaje del sensor y de la
	 * orientación de la pantalla, así que se deja configurable en lugar de
	 * codificarlo: es un valor que se ajusta con el móvil en la mano.
	 */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Pax|Touch", meta = (ClampMin = "0", ClampMax = "2"))
	int32 TiltAxisIndex = 1;

	UPROPERTY(EditDefaultsOnly, Config, Category = "Pax|Touch")
	bool bInvertTilt = false;

	/** Velocidad con la que los pedales táctiles llegan a fondo, en 1/s. */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Pax|Touch")
	float PedalResponseRate = 6.f;

	/** Velocidad con la que el volante sigue al dedo, en 1/s. */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Pax|Touch")
	float SteeringResponseRate = 12.f;

private:
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> DrivingContext;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ThrottleAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> BrakeAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> SteerAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ShiftUpAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ShiftDownAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> DRSAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ERSAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> FuelMixAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> CameraAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> RecoverAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> TiltAction;

	UPROPERTY(Transient)
	FPaxTouchLayout TouchLayout;

	UPROPERTY(Transient)
	FPaxTouchState TouchState;

	/** Estado de los botones en el frame anterior, para detectar la pulsación. */
	bool PreviousButtons[PaxNumTouchButtons] = {};

	/** Dedo que tiene tomado el volante; se lo queda hasta que lo levanta. */
	int32 SteeringFinger = INDEX_NONE;

	/** Última lectura del acelerómetro. */
	FVector LastTilt = FVector::ZeroVector;

	bool bTouchControlsEnabled = false;
};
