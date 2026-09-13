// Copyright Pax. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WheeledVehiclePawn.h"
#include "Core/PaxTypes.h"
#include "Components/PaxVehicleTelemetry.h"
#include "F1Car.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UAeroComponent;
class UTyreComponent;
class UERSComponent;
class UFuelComponent;
class UDRSComponent;
class ATrackSpline;

/**
 * El monoplaza.
 *
 * La física base la lleva Chaos Vehicles (motor, caja, suspensión, contacto
 * rueda-suelo). Encima van los sistemas que diferencian un F1 de un coche
 * cualquiera y que Chaos no modela: carga aerodinámica con reparto por ejes,
 * degradación de neumáticos, ERS con cupo por vuelta, consumo de combustible
 * que cambia la masa, y DRS con sus reglas de activación.
 *
 * Esos sistemas no leen el estado interno de Chaos: el coche compone una vez
 * por frame un FPaxVehicleTelemetry y se lo pasa a todos. Así el modelo de
 * juego queda aislado de los cambios de API del motor entre versiones.
 *
 * Los nombres de hueso que espera el WheelSetups (Wheel_FL, Wheel_FR,
 * Wheel_RL, Wheel_RR) los genera el rig de Blender en
 * Tools/Blender/pax_blender/rig.py; si se cambian ahí, hay que cambiarlos aquí.
 */
UCLASS()
class PAX_API AF1Car : public AWheeledVehiclePawn
{
	GENERATED_BODY()

public:
	AF1Car(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// --- Entradas de conducción (las usan el jugador y la IA) ---------------

	UFUNCTION(BlueprintCallable, Category = "Pax|Input")
	void SetThrottle(float Value);

	UFUNCTION(BlueprintCallable, Category = "Pax|Input")
	void SetBrake(float Value);

	UFUNCTION(BlueprintCallable, Category = "Pax|Input")
	void SetSteering(float Value);

	UFUNCTION(BlueprintCallable, Category = "Pax|Input")
	void ShiftUp();

	UFUNCTION(BlueprintCallable, Category = "Pax|Input")
	void ShiftDown();

	UFUNCTION(BlueprintCallable, Category = "Pax|Input")
	void RequestDRS();

	UFUNCTION(BlueprintCallable, Category = "Pax|Input")
	void ReleaseDRS();

	UFUNCTION(BlueprintCallable, Category = "Pax|Input")
	void CycleERSMode();

	UFUNCTION(BlueprintCallable, Category = "Pax|Input")
	void CycleFuelMix();

	UFUNCTION(BlueprintCallable, Category = "Pax|Input")
	void ToggleCameraView();

	/** Devuelve el coche a la pista tras un accidente o una salida. */
	UFUNCTION(BlueprintCallable, Category = "Pax|Vehicle")
	void RecoverToTrack();

	/** Bloquea el coche en parrilla hasta que se apaguen las luces. */
	UFUNCTION(BlueprintCallable, Category = "Pax|Vehicle")
	void SetControlsLocked(bool bLocked);

	UFUNCTION(BlueprintPure, Category = "Pax|Vehicle")
	bool AreControlsLocked() const { return bControlsLocked; }

	// --- Consulta de estado --------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Pax|Vehicle")
	const FPaxVehicleTelemetry& GetTelemetry() const { return Telemetry; }

	UFUNCTION(BlueprintPure, Category = "Pax|Vehicle")
	float GetSpeedKph() const { return Telemetry.SpeedKph; }

	/** Marcha mostrada: 1..8, 0 = punto muerto, -1 = marcha atrás. */
	UFUNCTION(BlueprintPure, Category = "Pax|Vehicle")
	int32 GetDisplayGear() const { return Telemetry.Gear; }

	UFUNCTION(BlueprintPure, Category = "Pax|Vehicle")
	float GetEngineRPM() const { return Telemetry.EngineRPM; }

	UFUNCTION(BlueprintPure, Category = "Pax|Vehicle")
	float GetMaxEngineRPM() const;

	/** Masa total actual (chasis + piloto + combustible) en kg. */
	UFUNCTION(BlueprintPure, Category = "Pax|Vehicle")
	float GetTotalMassKg() const { return DryMassKg + CurrentFuelKg; }

	/** Distancia recorrida a lo largo del trazado, en cm. */
	UFUNCTION(BlueprintPure, Category = "Pax|Vehicle")
	float GetLapDistanceCm() const { return LapDistanceCm; }

	/** Apertura del flap del DRS [0,1]; la consume el Animation Blueprint. */
	UFUNCTION(BlueprintPure, Category = "Pax|Vehicle")
	float GetDRSFlapAlpha() const;

	UAeroComponent* GetAero() const { return Aero; }
	UTyreComponent* GetTyres() const { return Tyres; }
	UERSComponent* GetERS() const { return ERS; }
	UFuelComponent* GetFuel() const { return Fuel; }
	UDRSComponent* GetDRS() const { return DRS; }

protected:
	/** Cámara persecutoria. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pax|Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pax|Camera")
	TObjectPtr<UCameraComponent> ChaseCamera;

	/** Cámara de casco, por delante del halo. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pax|Camera")
	TObjectPtr<UCameraComponent> CockpitCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pax|Systems")
	TObjectPtr<UAeroComponent> Aero;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pax|Systems")
	TObjectPtr<UTyreComponent> Tyres;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pax|Systems")
	TObjectPtr<UERSComponent> ERS;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pax|Systems")
	TObjectPtr<UFuelComponent> Fuel;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pax|Systems")
	TObjectPtr<UDRSComponent> DRS;

	/** Masa en orden de marcha sin combustible, en kg (mínimo reglamentario). */
	UPROPERTY(EditDefaultsOnly, Category = "Pax|Setup")
	float DryMassKg = 798.f;

	/** Distancia del origen del chasis al eje delantero, en cm. */
	UPROPERTY(EditDefaultsOnly, Category = "Pax|Setup")
	float FrontAxleOffsetCm = 180.f;

	/** Distancia del origen del chasis al eje trasero, en cm (negativa). */
	UPROPERTY(EditDefaultsOnly, Category = "Pax|Setup")
	float RearAxleOffsetCm = -180.f;

	/** Fricción base de la rueda antes de aplicar el modelo de neumático. */
	UPROPERTY(EditDefaultsOnly, Category = "Pax|Setup")
	float BaseWheelFriction = 3.f;

	/** Segundos boca abajo o parado fuera de pista antes de recolocar el coche. */
	UPROPERTY(EditDefaultsOnly, Category = "Pax|Setup")
	float RecoveryDelaySeconds = 4.f;

private:
	/** Configura motor, caja, diferencial, dirección y ruedas. */
	void ConfigurePowertrain();

	/** Rellena Telemetry con el estado del frame. */
	void UpdateTelemetry(float DeltaTime);

	/** Empuje adicional del MGU-K como fuerza en el eje trasero. */
	void ApplyERSBoost(float PowerWatts);

	/** Traslada el agarre calculado por el modelo de neumático a las ruedas. */
	void ApplyTyreGrip();

	/** Ajusta la masa del chasis según el combustible que queda. */
	void UpdateMassFromFuel();

	/** Detecta vuelco o parada fuera de pista y recoloca el coche. */
	void UpdateRecovery(float DeltaTime);

	UPROPERTY(Transient)
	FPaxVehicleTelemetry Telemetry;

	UPROPERTY(Transient)
	TObjectPtr<ATrackSpline> Track;

	FVector PreviousVelocity = FVector::ZeroVector;
	float LapDistanceCm = 0.f;
	float CurrentFuelKg = 0.f;
	float AppliedFuelKg = -1.f;
	float StuckTimer = 0.f;
	float ThrottleInput = 0.f;
	float BrakeInput = 0.f;
	float SteeringInput = 0.f;
	bool bControlsLocked = false;
	bool bCockpitView = false;
};
