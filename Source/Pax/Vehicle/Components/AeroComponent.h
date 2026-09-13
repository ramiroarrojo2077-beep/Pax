// Copyright Pax. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AeroComponent.generated.h"

class UPrimitiveComponent;

/**
 * Aerodinámica del monoplaza: carga, resistencia, rebufo y efecto del DRS.
 *
 * Modelo cuadrático clásico, F = k·v², con k agrupando densidad del aire,
 * área frontal y coeficiente. Se calibra con dos puntos conocidos de un F1
 * moderno: ~1.500 kg de carga a 200 km/h y ~3.300 kg a 300 km/h, con una
 * eficiencia L/D en torno a 4.
 *
 * La carga se aplica en dos puntos (eje delantero y eje trasero) en lugar de
 * en el centro de masas. Eso es lo que hace que el coche se hunda de morro al
 * frenar a alta velocidad y que el reparto aerodinámico tenga efecto real
 * sobre el subviraje, en vez de ser un número decorativo.
 */
UCLASS(ClassGroup = (Pax), meta = (BlueprintSpawnableComponent))
class PAX_API UAeroComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAeroComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Enlaza el componente al cuerpo físico del coche y a sus ejes. */
	void Initialize(UPrimitiveComponent* InBody, float InFrontAxleOffsetCm, float InRearAxleOffsetCm);

	/** El DRS abierto reduce resistencia y, con ella, carga trasera. */
	void SetDRSOpen(bool bOpen) { bDRSOpen = bOpen; }

	/** Carga aerodinámica total del último tick, en Newtons. */
	UFUNCTION(BlueprintPure, Category = "Pax|Aero")
	float GetDownforceNewtons() const { return LastDownforceN; }

	/** Carga expresada en kg, que es como se habla de ella en el paddock. */
	UFUNCTION(BlueprintPure, Category = "Pax|Aero")
	float GetDownforceKg() const;

	UFUNCTION(BlueprintPure, Category = "Pax|Aero")
	float GetDragNewtons() const { return LastDragN; }

	/** 0 = aire limpio, 1 = rebufo máximo pegado al coche de delante. */
	UFUNCTION(BlueprintPure, Category = "Pax|Aero")
	float GetSlipstreamFactor() const { return SlipstreamFactor; }

	/** Multiplicador de agarre aerodinámico que ve el modelo de neumático. */
	UFUNCTION(BlueprintPure, Category = "Pax|Aero")
	float GetFrontGripBias() const;

protected:
	/** Carga total en N por (m/s)². 4.8 ≈ 1.500 kg a 200 km/h. */
	UPROPERTY(EditDefaultsOnly, Category = "Pax|Aero", meta = (ClampMin = "0.0"))
	float DownforceCoefficient = 4.8f;

	/** Resistencia en N por (m/s)². Con 1.2 el coche topa sobre 305 km/h. */
	UPROPERTY(EditDefaultsOnly, Category = "Pax|Aero", meta = (ClampMin = "0.0"))
	float DragCoefficient = 1.2f;

	/** Reparto de carga: 0.45 = 45% delante, el balance típico de seco. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pax|Aero", meta = (ClampMin = "0.3", ClampMax = "0.6"))
	float AeroBalance = 0.45f;

	/** Reducción de resistencia con el DRS abierto. */
	UPROPERTY(EditDefaultsOnly, Category = "Pax|Aero", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DRSDragReduction = 0.25f;

	/** Pérdida de carga trasera con el DRS abierto: el coche va suelto de atrás. */
	UPROPERTY(EditDefaultsOnly, Category = "Pax|Aero", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DRSRearDownforceLoss = 0.30f;

	/** Distancia máxima a la que se nota el rebufo, en cm. */
	UPROPERTY(EditDefaultsOnly, Category = "Pax|Aero", meta = (ClampMin = "0.0"))
	float SlipstreamRangeCm = 6000.f;

	/** Reducción de resistencia en rebufo máximo. */
	UPROPERTY(EditDefaultsOnly, Category = "Pax|Aero", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SlipstreamDragReduction = 0.35f;

	/** Pérdida de carga delantera en aire sucio: el clásico subviraje al seguir. */
	UPROPERTY(EditDefaultsOnly, Category = "Pax|Aero", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DirtyAirFrontLoss = 0.22f;

	/**
	 * Altura de referencia del fondo plano, en cm. Por encima de ella el
	 * efecto suelo se pierde: es lo que castiga subirse a los pianos altos.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Pax|Aero", meta = (ClampMin = "1.0"))
	float GroundEffectReferenceHeightCm = 12.f;

private:
	/** Mide el rebufo trazando hacia delante en busca de otro monoplaza. */
	void UpdateSlipstream();

	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> Body;

	/** Offsets longitudinales de los ejes respecto al origen del chasis, en cm. */
	float FrontAxleOffsetCm = 180.f;
	float RearAxleOffsetCm = -180.f;

	bool bDRSOpen = false;
	float SlipstreamFactor = 0.f;
	float LastDownforceN = 0.f;
	float LastDragN = 0.f;
	float LastFrontShare = 0.45f;
};
