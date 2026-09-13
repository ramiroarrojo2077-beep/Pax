// Copyright Pax. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Core/PaxTypes.h"
#include "PaxHUD.generated.h"

class AF1Car;
class APaxGameState;

/**
 * Panel de información del piloto, dibujado sobre Canvas.
 *
 * Se dibuja en código en lugar de con widgets UMG por la misma razón que el
 * input: son assets binarios. Una HUD de carreras es un puñado de números y
 * barras que se refrescan cada frame, así que el Canvas basta y todo el
 * layout queda versionado en texto.
 *
 * Lo que se muestra está elegido por utilidad para conducir: marcha y cuentas
 * arriba en el centro, donde se mira; estado de neumáticos y energía abajo a
 * la izquierda, que se consultan de reojo; clasificación a la derecha.
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

	/** Barra con marcas de cambio, como el volante real. */
	void DrawRevBar(float X, float Y, float Width, float Height, float RPM, float MaxRPM);

	void DrawBar(float X, float Y, float Width, float Height, float Fraction, const FLinearColor& FillColor);

	void DrawLabel(const FString& Text, float X, float Y, const FLinearColor& Color, float Scale = 1.f);

private:
	/** Márgenes en píxeles respecto a los bordes de la pantalla. */
	float Margin = 40.f;
};
