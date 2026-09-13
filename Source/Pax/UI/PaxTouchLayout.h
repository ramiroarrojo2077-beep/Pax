// Copyright Pax. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PaxTouchLayout.generated.h"

/** Botones del mando en pantalla. */
UENUM(BlueprintType)
enum class EPaxTouchButton : uint8
{
	DRS,
	ERS,
	Mix,
	Camera,
	Recover,
	ShiftUp,
	ShiftDown,
	Count UMETA(Hidden)
};

/** Número de botones del mando táctil. */
static constexpr int32 PaxNumTouchButtons = static_cast<int32>(EPaxTouchButton::Count);

/** Etiqueta corta que se dibuja dentro de cada botón. */
PAX_API const TCHAR* PaxTouchButtonLabel(EPaxTouchButton Button);

/**
 * Reparto del mando táctil en la pantalla.
 *
 * Una sola estructura calcula los rectángulos, y tanto el control (que decide
 * qué has pulsado) como la HUD (que los dibuja) leen de ella. Separarlos sería
 * el camino directo a un botón que se pinta en un sitio y responde en otro,
 * que es un fallo imposible de ver en una captura y evidente con el dedo.
 *
 * Se diseña sobre una pantalla de referencia de 1080 de alto y se escala: un
 * dedo mide lo mismo en un móvil de gama baja que en una tableta, así que los
 * controles deben crecer con la pantalla, no con la resolución.
 */
USTRUCT()
struct PAX_API FPaxTouchLayout
{
	GENERATED_BODY()

	FPaxTouchLayout()
	{
		// FBox2D no se inicializa sola. Si la HUD dibujase antes del primer
		// Build, los rectángulos serían basura y aparecerían botones en
		// cualquier parte de la pantalla.
		for (FBox2D& Button : Buttons)
		{
			Button = FBox2D(ForceInit);
		}
	}

	/** Recalcula el reparto para un tamaño de viewport dado. */
	void Build(const FVector2D& InViewportSize);

	/** ¿Está calculado para este tamaño? Evita rehacerlo cada frame. */
	bool Matches(const FVector2D& InViewportSize) const;

	/** Zona de dirección: el pulgar izquierdo arrastra dentro de ella. */
	FBox2D SteerArea = FBox2D(ForceInit);

	/** Recorrido en píxeles que equivale a giro completo. */
	float SteerRadius = 200.f;

	FBox2D ThrottlePedal = FBox2D(ForceInit);
	FBox2D BrakePedal = FBox2D(ForceInit);

	FBox2D Buttons[PaxNumTouchButtons];

	/** Escala respecto a la pantalla de referencia. */
	float Scale = 1.f;

	FVector2D ViewportSize = FVector2D::ZeroVector;

	const FBox2D& GetButton(EPaxTouchButton Button) const
	{
		return Buttons[static_cast<int32>(Button)];
	}
};

/** Lo que el jugador está tocando ahora mismo. */
USTRUCT()
struct PAX_API FPaxTouchState
{
	GENERATED_BODY()

	float Steering = 0.f;
	float Throttle = 0.f;
	float Brake = 0.f;

	/** Hay un dedo apoyado en la zona de dirección. */
	bool bSteerActive = false;

	/** Punto donde se apoyó el dedo: el volante flota bajo el pulgar. */
	FVector2D SteerAnchor = FVector2D::ZeroVector;
	FVector2D SteerCurrent = FVector2D::ZeroVector;

	bool bButtonDown[PaxNumTouchButtons] = {};

	bool IsButtonDown(EPaxTouchButton Button) const
	{
		return bButtonDown[static_cast<int32>(Button)];
	}
};
