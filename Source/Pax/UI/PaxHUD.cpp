// Copyright Pax. All Rights Reserved.

#include "PaxHUD.h"
#include "Core/PaxGameState.h"
#include "Core/PaxGameMode.h"
#include "Core/PaxPlayerController.h"
#include "Vehicle/F1Car.h"
#include "Vehicle/Components/TyreComponent.h"
#include "Vehicle/Components/ERSComponent.h"
#include "Vehicle/Components/FuelComponent.h"
#include "Vehicle/Components/DRSComponent.h"
#include "Engine/Canvas.h"
#include "CanvasItem.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"

namespace
{
	const FLinearColor ColorText(0.92f, 0.92f, 0.94f, 1.f);
	const FLinearColor ColorDim(0.62f, 0.62f, 0.66f, 1.f);
	const FLinearColor ColorPanel(0.03f, 0.03f, 0.05f, 0.55f);
	const FLinearColor ColorAccent(0.95f, 0.25f, 0.20f, 1.f);
	const FLinearColor ColorGood(0.25f, 0.85f, 0.45f, 1.f);
	const FLinearColor ColorWarn(0.98f, 0.75f, 0.15f, 1.f);
	const FLinearColor ColorERS(0.20f, 0.65f, 0.95f, 1.f);

	/** Relleno de un control táctil en reposo y pulsado. */
	const FLinearColor ColorTouchIdle(1.f, 1.f, 1.f, 0.10f);
	const FLinearColor ColorTouchBorder(1.f, 1.f, 1.f, 0.35f);

	/** Alto de la pantalla de referencia sobre la que está medida la HUD. */
	constexpr float ReferenceHeight = 1080.f;

	FString ERSModeName(EERSMode Mode)
	{
		switch (Mode)
		{
		case EERSMode::Harvest:  return TEXT("RECUPERA");
		case EERSMode::Balanced: return TEXT("EQUILIBRADO");
		case EERSMode::Overtake: return TEXT("ATAQUE");
		}
		return TEXT("-");
	}

	FString FuelMixName(EFuelMix Mix)
	{
		switch (Mix)
		{
		case EFuelMix::Lean:     return TEXT("AHORRO");
		case EFuelMix::Standard: return TEXT("ESTÁNDAR");
		case EFuelMix::Rich:     return TEXT("POTENCIA");
		}
		return TEXT("-");
	}

	FString CompoundName(ETyreCompound Compound)
	{
		switch (Compound)
		{
		case ETyreCompound::Soft:         return TEXT("BLANDO");
		case ETyreCompound::Medium:       return TEXT("MEDIO");
		case ETyreCompound::Hard:         return TEXT("DURO");
		case ETyreCompound::Intermediate: return TEXT("INTERMEDIO");
		case ETyreCompound::Wet:          return TEXT("LLUVIA");
		}
		return TEXT("-");
	}

	FString GearName(int32 Gear)
	{
		if (Gear < 0) return TEXT("R");
		if (Gear == 0) return TEXT("N");
		return FString::FromInt(Gear);
	}
}

APaxHUD::APaxHUD()
{
	PrimaryActorTick.bCanEverTick = false;
}

void APaxHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas)
	{
		return;
	}

	// Toda la HUD está medida sobre 1080 de alto; aquí se adapta a la pantalla
	// real, que en un móvil puede ser cualquier cosa entre 720 y 1600.
	UIScale = FMath::Clamp(Canvas->SizeY / ReferenceHeight, 0.55f, 2.5f);
	Margin = S(40.f);

	const APaxPlayerController* PaxController = Cast<APaxPlayerController>(GetOwningPlayerController());
	bTouchHudActive = PaxController && PaxController->AreTouchControlsEnabled();

	const AF1Car* Car = Cast<AF1Car>(GetOwningPawn());
	const APaxGameState* RaceState = GetWorld() ? GetWorld()->GetGameState<APaxGameState>() : nullptr;
	if (!Car || !RaceState)
	{
		return;
	}

	DrawSpeedAndGear(*Car);
	DrawTiming(*Car, *RaceState);
	DrawTyres(*Car);
	DrawEnergy(*Car, *RaceState);
	DrawDRS(*Car);
	DrawStandings(*RaceState);

	if (RaceState->GetRaceState() == ERaceState::Countdown)
	{
		DrawStartLights();
	}

	if (bTouchHudActive)
	{
		DrawTouchControls(PaxController->GetTouchLayout(), PaxController->GetTouchState());
	}
}

// ---------------------------------------------------------------------------
// Primitivas
// ---------------------------------------------------------------------------

void APaxHUD::DrawLabel(const FString& Text, float X, float Y, const FLinearColor& Color, float Scale)
{
	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	if (!Font)
	{
		return;
	}

	FCanvasTextItem Item(FVector2D(X, Y), FText::FromString(Text), Font, Color);
	Item.Scale = FVector2D(Scale * UIScale, Scale * UIScale);
	Item.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.8f));
	Canvas->DrawItem(Item);
}

void APaxHUD::DrawBar(float X, float Y, float Width, float Height, float Fraction, const FLinearColor& FillColor)
{
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f), X, Y, Width, Height);
	const float Inset = FMath::Max(1.f * UIScale, 1.f);
	DrawRect(FillColor, X + Inset, Y + Inset,
		FMath::Max((Width - Inset * 2.f) * FMath::Clamp(Fraction, 0.f, 1.f), 0.f),
		Height - Inset * 2.f);
}

void APaxHUD::DrawPanel(const FBox2D& Rect, const FLinearColor& Fill, const FLinearColor& Border, float BorderWidth)
{
	const float Left = static_cast<float>(Rect.Min.X);
	const float Top = static_cast<float>(Rect.Min.Y);
	const float Width = static_cast<float>(Rect.Max.X - Rect.Min.X);
	const float Height = static_cast<float>(Rect.Max.Y - Rect.Min.Y);

	DrawRect(Fill, Left, Top, Width, Height);

	// Cuatro tiras finas como borde: Canvas no dibuja rectángulos huecos.
	const float W = FMath::Max(BorderWidth * UIScale, 1.f);
	DrawRect(Border, Left, Top, Width, W);
	DrawRect(Border, Left, Top + Height - W, Width, W);
	DrawRect(Border, Left, Top, W, Height);
	DrawRect(Border, Left + Width - W, Top, W, Height);
}

void APaxHUD::DrawRevBar(float X, float Y, float Width, float Height, float RPM, float MaxRPM)
{
	constexpr int32 NumSegments = 20;
	const float SegmentWidth = Width / NumSegments;
	const float Fraction = MaxRPM > 0.f ? FMath::Clamp(RPM / MaxRPM, 0.f, 1.f) : 0.f;
	const int32 Lit = FMath::FloorToInt(Fraction * NumSegments);

	for (int32 Index = 0; Index < NumSegments; ++Index)
	{
		// Verde, ámbar y rojo como en el volante: el tramo rojo es el aviso
		// de cambio, y los dos últimos segmentos marcan el corte.
		FLinearColor Color = ColorDim;
		if (Index < Lit)
		{
			const float SegmentFraction = static_cast<float>(Index) / NumSegments;
			Color = SegmentFraction < 0.6f ? ColorGood : (SegmentFraction < 0.85f ? ColorWarn : ColorAccent);
		}

		DrawRect(Color, X + Index * SegmentWidth, Y, SegmentWidth - S(3.f), Height);
	}
}

// ---------------------------------------------------------------------------
// Paneles
// ---------------------------------------------------------------------------

void APaxHUD::DrawSpeedAndGear(const AF1Car& Car)
{
	const float CenterX = Canvas->SizeX * 0.5f;
	const float Top = Margin;

	DrawRect(ColorPanel, CenterX - S(190.f), Top, S(380.f), S(120.f));
	DrawRevBar(CenterX - S(175.f), Top + S(12.f), S(350.f), S(14.f), Car.GetEngineRPM(), Car.GetMaxEngineRPM());

	DrawLabel(GearName(Car.GetDisplayGear()), CenterX - S(22.f), Top + S(34.f), ColorText, 2.6f);

	const FString Speed = FString::Printf(TEXT("%d"), FMath::RoundToInt(FMath::Abs(Car.GetSpeedKph())));
	DrawLabel(Speed, CenterX + S(40.f), Top + S(44.f), ColorText, 1.8f);
	DrawLabel(TEXT("km/h"), CenterX + S(118.f), Top + S(62.f), ColorDim, 0.9f);
}

void APaxHUD::DrawTiming(const AF1Car& Car, const APaxGameState& RaceState)
{
	const FRaceEntry* Entry = RaceState.FindEntry(&Car);
	if (!Entry)
	{
		return;
	}

	const float X = Margin;
	const float Y = Margin;

	DrawRect(ColorPanel, X, Y, S(300.f), S(150.f));

	DrawLabel(FString::Printf(TEXT("P%d / %d"), Entry->Position, RaceState.GetEntries().Num()),
		X + S(14.f), Y + S(10.f), ColorText, 1.6f);
	DrawLabel(FString::Printf(TEXT("VUELTA %d / %d"),
		FMath::Min(Entry->LapsCompleted + 1, RaceState.GetTotalLaps()), RaceState.GetTotalLaps()),
		X + S(14.f), Y + S(46.f), ColorDim, 1.f);

	DrawLabel(FormatLapTime(Entry->CurrentLapTime), X + S(14.f), Y + S(68.f), ColorText, 1.2f);
	DrawLabel(FString::Printf(TEXT("MEJOR  %s"), *FormatLapTime(Entry->BestLap.TotalSeconds)),
		X + S(14.f), Y + S(96.f), ColorDim, 0.9f);

	const float GapAhead = RaceState.GetGapAheadSeconds(&Car);
	const FString GapText = GapAhead < TNumericLimits<float>::Max()
		? FString::Printf(TEXT("DELANTE %s"), *FormatDelta(GapAhead))
		: TEXT("LÍDER");
	DrawLabel(GapText, X + S(14.f), Y + S(118.f), GapAhead < 1.f ? ColorGood : ColorDim, 0.9f);

	if (Entry->PenaltySeconds > 0.f)
	{
		DrawLabel(FString::Printf(TEXT("SANCIÓN +%.0f s"), Entry->PenaltySeconds),
			X + S(170.f), Y + S(118.f), ColorAccent, 0.9f);
	}
}

void APaxHUD::DrawTyres(const AF1Car& Car)
{
	const UTyreComponent* TyreSystem = Car.GetTyres();
	if (!TyreSystem)
	{
		return;
	}

	// En móvil la esquina inferior izquierda es donde vive el pulgar del
	// volante, así que el panel se sube por encima de la zona de dirección.
	const bool bTouch = IsTouchHudActive();
	const float X = Margin;
	const float Y = bTouch ? Margin + S(160.f) : Canvas->SizeY - Margin - S(150.f);

	DrawRect(ColorPanel, X, Y, S(230.f), S(150.f));
	DrawLabel(CompoundName(TyreSystem->GetCompound()), X + S(14.f), Y + S(10.f), ColorText, 1.1f);
	DrawLabel(FString::Printf(TEXT("%.1f vueltas"), TyreSystem->GetSetAgeLaps()),
		X + S(130.f), Y + S(14.f), ColorDim, 0.8f);

	// Las cuatro esquinas dispuestas como se ven desde arriba: el orden de
	// las ruedas (FL, FR, RL, RR) coincide con el de la rejilla de dos por dos.
	const TArray<FTyreState>& States = TyreSystem->GetTyreStates();

	for (int32 Slot = 0; Slot < 4; ++Slot)
	{
		if (!States.IsValidIndex(Slot))
		{
			continue;
		}

		const FTyreState& Tyre = States[Slot];
		const float CellX = X + S(20.f) + (Slot % 2) * S(110.f);
		const float CellY = Y + S(44.f) + (Slot / 2) * S(52.f);

		// Color por temperatura: azul frío, verde en ventana, rojo pasado.
		const float TempC = Tyre.TemperatureC;
		FLinearColor TempColor = ColorGood;
		if (TempC < 80.f) TempColor = ColorERS;
		else if (TempC > 125.f) TempColor = ColorAccent;

		DrawBar(CellX, CellY, S(80.f), S(12.f), Tyre.Life, Tyre.Life > 0.35f ? ColorGood : ColorWarn);
		DrawLabel(FString::Printf(TEXT("%d%%  %.0f°"), FMath::RoundToInt(Tyre.Life * 100.f), TempC),
			CellX, CellY + S(16.f), TempColor, 0.75f);
	}
}

void APaxHUD::DrawEnergy(const AF1Car& Car, const APaxGameState& RaceState)
{
	const UERSComponent* ERSSystem = Car.GetERS();
	const UFuelComponent* FuelSystem = Car.GetFuel();

	const bool bTouch = IsTouchHudActive();
	const float X = bTouch ? Margin : Margin + S(250.f);
	const float Y = bTouch ? Margin + S(300.f) : Canvas->SizeY - Margin - S(150.f);

	DrawRect(ColorPanel, X, Y, S(260.f), S(150.f));

	if (ERSSystem)
	{
		DrawLabel(TEXT("ERS"), X + S(14.f), Y + S(10.f), ColorDim, 0.85f);
		DrawLabel(ERSModeName(ERSSystem->GetMode()), X + S(70.f), Y + S(10.f), ColorERS, 0.85f);
		DrawBar(X + S(14.f), Y + S(32.f), S(230.f), S(14.f), ERSSystem->GetChargeFraction(), ColorERS);
		DrawLabel(FString::Printf(TEXT("%.1f MJ en esta vuelta   %.0f kW"),
			ERSSystem->GetLapDeploymentRemainingMJ(), ERSSystem->GetDeployedPowerKw()),
			X + S(14.f), Y + S(50.f), ColorDim, 0.75f);
	}

	if (FuelSystem)
	{
		DrawLabel(TEXT("COMBUSTIBLE"), X + S(14.f), Y + S(78.f), ColorDim, 0.85f);
		DrawLabel(FuelMixName(FuelSystem->GetMix()), X + S(150.f), Y + S(78.f), ColorWarn, 0.85f);
		DrawLabel(FString::Printf(TEXT("%.1f kg   %.1f vueltas"),
			FuelSystem->GetFuelKg(), FuelSystem->GetLapsRemaining()),
			X + S(14.f), Y + S(100.f), ColorText, 0.8f);

		// Margen respecto a lo que falta: si es negativo hay que levantar.
		const FRaceEntry* Entry = RaceState.FindEntry(&Car);
		if (Entry)
		{
			const int32 Remaining = FMath::Max(RaceState.GetTotalLaps() - Entry->LapsCompleted, 0);
			const float Delta = FuelSystem->GetFuelDeltaLaps(Remaining);
			DrawLabel(FString::Printf(TEXT("MARGEN %s vueltas"), *FormatDelta(Delta)),
				X + S(14.f), Y + S(122.f), Delta < 0.f ? ColorAccent : ColorGood, 0.8f);
		}
	}
}

void APaxHUD::DrawDRS(const AF1Car& Car)
{
	const UDRSComponent* DRSSystem = Car.GetDRS();
	if (!DRSSystem || DRSSystem->GetState() == EDRSState::Unavailable)
	{
		return;
	}

	const float X = Canvas->SizeX * 0.5f - S(60.f);
	const float Y = Margin + S(132.f);

	const bool bOpen = DRSSystem->GetState() == EDRSState::Open;
	DrawRect(bOpen ? ColorGood : FLinearColor(0.f, 0.f, 0.f, 0.6f), X, Y, S(120.f), S(32.f));
	DrawLabel(bOpen ? TEXT("DRS ACTIVO") : TEXT("DRS LISTO"), X + S(12.f), Y + S(8.f),
		bOpen ? FLinearColor::Black : ColorGood, 0.85f);
}

void APaxHUD::DrawStandings(const APaxGameState& RaceState)
{
	const TArray<FRaceEntry>& Entries = RaceState.GetEntries();
	if (Entries.Num() == 0)
	{
		return;
	}

	const float RowHeight = S(22.f);
	const float Width = S(260.f);
	const float X = Canvas->SizeX - Margin - Width;
	const float Y = Margin;

	// La tabla no puede invadir los controles táctiles ni salirse por abajo:
	// se recorta al espacio que queda libre.
	const float AvailableHeight = IsTouchHudActive()
		? Canvas->SizeY * 0.42f
		: Canvas->SizeY - Margin * 2.f;
	const int32 MaxRows = FMath::Clamp(FMath::FloorToInt((AvailableHeight - S(16.f)) / RowHeight), 3, Entries.Num());

	DrawRect(ColorPanel, X, Y, Width, MaxRows * RowHeight + S(16.f));

	for (int32 Index = 0; Index < MaxRows; ++Index)
	{
		const FRaceEntry& Entry = Entries[Index];
		const float RowY = Y + S(8.f) + Index * RowHeight;

		const FLinearColor RowColor = Entry.bIsPlayer ? ColorAccent : ColorText;
		DrawLabel(FString::Printf(TEXT("%2d  %s"), Entry.Position, *Entry.DriverName),
			X + S(12.f), RowY, RowColor, 0.8f);

		const FString Right = Index == 0
			? FormatLapTime(Entry.BestLap.TotalSeconds)
			: FormatDelta(RaceState.GetGapToLeaderSeconds(Entry.Car));
		DrawLabel(Right, X + Width - S(90.f), RowY, ColorDim, 0.75f);
	}
}

void APaxHUD::DrawStartLights()
{
	const APaxGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<APaxGameMode>() : nullptr;
	if (!GameMode)
	{
		return;
	}

	const int32 LightsOn = GameMode->GetStartLightsOn();
	const float LightSize = S(46.f);
	const float Spacing = S(14.f);
	const float TotalWidth = 5 * LightSize + 4 * Spacing;
	const float X = Canvas->SizeX * 0.5f - TotalWidth * 0.5f;
	const float Y = Canvas->SizeY * 0.22f;

	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.75f), X - S(16.f), Y - S(16.f),
		TotalWidth + S(32.f), LightSize + S(32.f));

	for (int32 Index = 0; Index < 5; ++Index)
	{
		const bool bOn = Index < LightsOn;
		DrawRect(bOn ? ColorAccent : FLinearColor(0.12f, 0.12f, 0.14f, 1.f),
			X + Index * (LightSize + Spacing), Y, LightSize, LightSize);
	}
}

// ---------------------------------------------------------------------------
// Mando en pantalla
// ---------------------------------------------------------------------------

void APaxHUD::DrawTouchControls(const FPaxTouchLayout& Layout, const FPaxTouchState& State)
{
	// --- Pedales ------------------------------------------------------------
	// El relleno crece con lo pisado que está el pedal, así se ve de un vistazo
	// si se va a fondo sin apartar la vista de la pista.
	auto DrawPedal = [this](const FBox2D& Rect, float Amount, const FLinearColor& Color, const TCHAR* Label)
	{
		DrawPanel(Rect, ColorTouchIdle, ColorTouchBorder);

		const float Left = static_cast<float>(Rect.Min.X);
		const float Bottom = static_cast<float>(Rect.Max.Y);
		const float Width = static_cast<float>(Rect.Max.X - Rect.Min.X);
		const float Height = static_cast<float>(Rect.Max.Y - Rect.Min.Y);

		const float FillHeight = Height * FMath::Clamp(Amount, 0.f, 1.f);
		if (FillHeight > 1.f)
		{
			DrawRect(FLinearColor(Color.R, Color.G, Color.B, 0.45f),
				Left, Bottom - FillHeight, Width, FillHeight);
		}

		DrawLabel(Label, Left + Width * 0.5f - S(18.f), Bottom - Height * 0.5f - S(10.f),
			ColorText, 1.0f);
	};

	DrawPedal(Layout.ThrottlePedal, State.Throttle, ColorGood, TEXT("GAS"));
	DrawPedal(Layout.BrakePedal, State.Brake, ColorAccent, TEXT("FRENO"));

	// --- Botones ------------------------------------------------------------
	for (int32 Index = 0; Index < PaxNumTouchButtons; ++Index)
	{
		const EPaxTouchButton Button = static_cast<EPaxTouchButton>(Index);
		const FBox2D& Rect = Layout.Buttons[Index];
		const bool bDown = State.IsButtonDown(Button);

		const FLinearColor Fill = bDown
			? FLinearColor(1.f, 1.f, 1.f, 0.30f)
			: ColorTouchIdle;
		DrawPanel(Rect, Fill, ColorTouchBorder);

		const float Left = static_cast<float>(Rect.Min.X);
		const float Top = static_cast<float>(Rect.Min.Y);
		const float Width = static_cast<float>(Rect.Max.X - Rect.Min.X);
		const float Height = static_cast<float>(Rect.Max.Y - Rect.Min.Y);

		const FString Label = PaxTouchButtonLabel(Button);
		// Centrado aproximado: la fuente media tiene un avance de unos 9 px
		// por carácter a escala 1, suficiente para etiquetas de tres letras.
		const float TextWidth = Label.Len() * S(9.f);
		DrawLabel(Label, Left + (Width - TextWidth) * 0.5f, Top + Height * 0.5f - S(9.f), ColorText, 0.9f);
	}

	// --- Volante ------------------------------------------------------------
	// Sólo se dibuja mientras hay un dedo apoyado, y donde se apoyó: un volante
	// fijo obliga a mirar la pantalla para encontrarlo.
	if (!State.bSteerActive)
	{
		return;
	}

	const float AnchorX = static_cast<float>(State.SteerAnchor.X);
	const float AnchorY = static_cast<float>(State.SteerAnchor.Y);
	const float Radius = Layout.SteerRadius;
	const float TrackHeight = S(14.f);

	DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.12f),
		AnchorX - Radius, AnchorY - TrackHeight * 0.5f, Radius * 2.f, TrackHeight);

	const float KnobSize = S(46.f);
	const float KnobX = AnchorX + FMath::Clamp(State.Steering, -1.f, 1.f) * Radius - KnobSize * 0.5f;
	DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.55f),
		KnobX, AnchorY - KnobSize * 0.5f, KnobSize, KnobSize);
}
