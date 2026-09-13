// Copyright Pax. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PaxPlayerController.generated.h"

class AF1Car;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

/**
 * Mando del jugador.
 *
 * Enhanced Input, pero con las acciones y el contexto creados en código en
 * lugar de como assets binarios. Cuesta unas líneas más y a cambio todo el
 * mapeo de controles queda en el repositorio como texto: se revisa en un diff,
 * se cambia sin abrir el editor y no hay un .uasset que resolver a mano cuando
 * dos personas tocan los controles a la vez.
 */
UCLASS()
class PAX_API APaxPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	APaxPlayerController();

	virtual void SetupInputComponent() override;
	virtual void OnPossess(APawn* InPawn) override;

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

	AF1Car* GetCar() const;

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
};
