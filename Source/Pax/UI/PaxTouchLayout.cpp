// Copyright Pax. All Rights Reserved.

#include "PaxTouchLayout.h"

namespace
{
	/** Alto de la pantalla de referencia sobre la que están medidos los controles. */
	constexpr float ReferenceHeight = 1080.f;
}

const TCHAR* PaxTouchButtonLabel(EPaxTouchButton Button)
{
	switch (Button)
	{
	case EPaxTouchButton::DRS:       return TEXT("DRS");
	case EPaxTouchButton::ERS:       return TEXT("ERS");
	case EPaxTouchButton::Mix:       return TEXT("MIX");
	case EPaxTouchButton::Camera:    return TEXT("CAM");
	case EPaxTouchButton::Recover:   return TEXT("REC");
	case EPaxTouchButton::ShiftUp:   return TEXT("+");
	case EPaxTouchButton::ShiftDown: return TEXT("-");
	default:                         return TEXT("");
	}
}

bool FPaxTouchLayout::Matches(const FVector2D& InViewportSize) const
{
	return ViewportSize.Equals(InViewportSize, 1.f);
}

void FPaxTouchLayout::Build(const FVector2D& InViewportSize)
{
	ViewportSize = InViewportSize;

	// La escala sale del alto porque es la dimensión que no cambia al girar el
	// móvil ni entre relaciones de aspecto: un botón de 120 px en referencia
	// mide lo mismo en pantalla en cualquier teléfono.
	// FVector2D es de doble precisión en UE5; el reparto se hace en float
	// porque todo lo que consume estas medidas (Canvas, FMath::Clamp) lo es.
	const float Width = static_cast<float>(InViewportSize.X);
	const float Height = static_cast<float>(InViewportSize.Y);

	Scale = FMath::Clamp(Height / ReferenceHeight, 0.55f, 2.5f);
	const float Margin = 36.f * Scale;

	// --- Dirección ---------------------------------------------------------
	// Franja inferior izquierda. Es grande a propósito: el pulgar no apunta,
	// se apoya donde cae y el volante se ancla ahí. Empieza al 55% de la
	// altura para no invadir los paneles de neumáticos y energía, que en
	// móvil se recolocan en esa misma esquina.
	SteerArea = FBox2D(FVector2D(0.f, Height * 0.55f),
	                   FVector2D(Width * 0.46f, Height));
	SteerRadius = 190.f * Scale;

	// --- Pedales -----------------------------------------------------------
	const float PedalWidth = 170.f * Scale;
	const float PedalHeight = 230.f * Scale;
	const float PedalGap = 18.f * Scale;

	const float ThrottleRight = Width - Margin;
	const float PedalBottom = Height - Margin;

	ThrottlePedal = FBox2D(FVector2D(ThrottleRight - PedalWidth, PedalBottom - PedalHeight),
	                       FVector2D(ThrottleRight, PedalBottom));

	const float BrakeRight = ThrottlePedal.Min.X - PedalGap;
	BrakePedal = FBox2D(FVector2D(BrakeRight - PedalWidth * 0.9f, PedalBottom - PedalHeight),
	                    FVector2D(BrakeRight, PedalBottom));

	// --- Botones -----------------------------------------------------------
	// Fila sobre los pedales, al alcance del mismo pulgar sin soltar el gas.
	const float ButtonSize = 88.f * Scale;
	const float ButtonGap = 12.f * Scale;
	const float ButtonBottom = ThrottlePedal.Min.Y - ButtonGap;
	const float ButtonTop = ButtonBottom - ButtonSize;

	// La fila se coloca de derecha a izquierda, ordenada por frecuencia de uso.
	// El DRS va pegado al borde, encima del acelerador, que es donde ya está el
	// pulgar; volver a pista queda en el extremo opuesto, porque es lo único
	// que no quieres pulsar por accidente a mitad de una recta.
	const EPaxTouchButton Row[] = {
		EPaxTouchButton::DRS,
		EPaxTouchButton::ERS,
		EPaxTouchButton::Mix,
		EPaxTouchButton::Camera,
		EPaxTouchButton::Recover,
	};

	float Right = Width - Margin;
	for (const EPaxTouchButton Button : Row)
	{
		// El DRS además es el más ancho: se pulsa varias veces por vuelta.
		const float ThisWidth = (Button == EPaxTouchButton::DRS) ? ButtonSize * 1.6f : ButtonSize;
		Buttons[static_cast<int32>(Button)] =
			FBox2D(FVector2D(Right - ThisWidth, ButtonTop), FVector2D(Right, ButtonBottom));
		Right -= ThisWidth + ButtonGap;
	}

	// Levas: pegadas al borde derecho, encima de la fila de botones. Sólo
	// hacen falta si el piloto decide cambiar a mano.
	const float PaddleWidth = ButtonSize * 0.9f;
	const float PaddleHeight = ButtonSize * 0.8f;
	const float PaddleRight = Width - Margin;
	const float PaddleBottom = ButtonTop - ButtonGap;

	Buttons[static_cast<int32>(EPaxTouchButton::ShiftUp)] =
		FBox2D(FVector2D(PaddleRight - PaddleWidth, PaddleBottom - PaddleHeight),
		       FVector2D(PaddleRight, PaddleBottom));

	Buttons[static_cast<int32>(EPaxTouchButton::ShiftDown)] =
		FBox2D(FVector2D(PaddleRight - PaddleWidth * 2.f - ButtonGap, PaddleBottom - PaddleHeight),
		       FVector2D(PaddleRight - PaddleWidth - ButtonGap, PaddleBottom));
}
