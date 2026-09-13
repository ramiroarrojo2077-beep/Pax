// Copyright Pax. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Core/PaxTypes.h"
#include "PaxTouchLayout.h"
#include "PaxHUD.generated.h"

class AF1Car;
class APaxGameState;

/**
 * Panel de información del piloto y mando en pantalla, dibujados sobre Canvas.
 *
 * Se dibuja en código en lugar de con widgets UMG por la misma razón que el
 * input: son assets binarios. Una HUD de carreras es un puñado de números y
 * barras que se refrescan cada frame, así que el Canvas basta y todo el
 * layout queda versionado en texto.
 *
 * Todo está medido sobre una pantalla de referencia de 1080 de alto y se
 * multiplica por UIScale. Sin eso, en un móvil de 2400×1080 los números
 * quedarían legibles y en una tableta de 2560×1600, diminutos: en pantalla
 * táctil el tamaño físico importa más que el número de píxeles.
 */
UCLASS()
class PAX_API APaxHUD : public AHUD
{
	GENERATED_BODY()

public:
	APaxHUD();

	virtual void DrawHUD() override;

protected:
	/** Velocidad, marcha y barra de revoluciones. */
	void DrawSpeedAndGear(const AF1Car& Car);

	/** Vuelta, tiempos y diferencia con el de delante. */
	void DrawTiming(const AF1Car& Car, const APaxGameState& RaceState);

	/** Vida y temperatura de los cuatro neumáticos. */
	void DrawTyres(const AF1Car& Car);

	/** Batería, modo de ERS, mezcla y combustible restante. */
	void DrawEnergy(const AF1Car& Car, const APaxGameState& RaceState);

	/** Estado del DRS. */
	void DrawDRS(const AF1Car& Car);

	/** Tabla de posiciones. */
	void DrawStandings(const APaxGameState& RaceState);

	/** Semáforo de salida. */
	void DrawStartLights();

	/** Pedales, volante y botones del mando táctil. */
	void DrawTouchControls(const FPaxTouchLayout& Layout, const FPaxTouchState& State);

	/** Barra con marcas de cambio, como el volante real. */
	void DrawRevBar(float X, float Y, float Width, float Height, float RPM, float MaxRPM);

	void DrawBar(float X, float Y, float Width, float Height, float Fraction, const FLinearColor& FillColor);

	void DrawLabel(const FString& Text, float X, float Y, const FLinearColor& Color, float Scale = 1.f);

	/** Dibuja un rectángulo con borde, que es lo que hace legible un botón. */
	void DrawPanel(const FBox2D& Rect, const FLinearColor& Fill, const FLinearColor& Border, float BorderWidth = 2.f);

	/** Escala una medida de la pantalla de referencia a la pantalla real. */
	float S(float Value) const { return Value * UIScale; }

	/**
	 * ¿Se está dibujando el mando en pantalla?
	 * Los paneles de información se recolocan cuando lo está: la parte baja de
	 * la pantalla pasa a ser de los pulgares.
	 */
	bool IsTouchHudActive() const { return bTouchHudActive; }

private:
	/** Factor de escala del frame actual. */
	float UIScale = 1.f;

	/** Mando táctil activo en este frame. */
	bool bTouchHudActive = false;

	/** Margen respecto a los bordes, ya escalado. */
	float Margin = 40.f;
};
