// Copyright Pax. All Rights Reserved.

#include "PaxHUD.h"
#include "Core/PaxGameState.h"
#include "Core/PaxGameMode.h"
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
}

void APaxHUD::DrawLabel(const FString& Text, float X, float Y, const FLinearColor& Color, float Scale)
{
	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	if (!Font)
	{
		return;
	}

	FCanvasTextItem Item(FVector2D(X, Y), FText::FromString(Text), Font, Color);
	Item.Scale = FVector2D(Scale, Scale);
	Item.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.8f));
	Canvas->DrawItem(Item);
}

void APaxHUD::DrawBar(float X, float Y, float Width, float Height, float Fraction, const FLinearColor& FillColor)
{
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f), X, Y, Width, Height);
	DrawRect(FillColor, X + 1.f, Y + 1.f, FMath::Max((Width - 2.f) * FMath::Clamp(Fraction, 0.f, 1.f), 0.f), Height - 2.f);
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
		// de cambio, y los dos últimos segmentos parpadean en el corte.
		FLinearColor Color = ColorDim;
		if (Index < Lit)
		{
			const float SegmentFraction = static_cast<float>(Index) / NumSegments;
			Color = SegmentFraction < 0.6f ? ColorGood : (SegmentFraction < 0.85f ? ColorWarn : ColorAccent);
		}

		DrawRect(Color, X + Index * SegmentWidth, Y, SegmentWidth - 3.f, Height);
	}
}

void APaxHUD::DrawSpeedAndGear(const AF1Car& Car)
{
	const float CenterX = Canvas->SizeX * 0.5f;
	const float Top = Margin;

	DrawRect(ColorPanel, CenterX - 190.f, Top, 380.f, 120.f);
	DrawRevBar(CenterX - 175.f, Top + 12.f, 350.f, 14.f, Car.GetEngineRPM(), Car.GetMaxEngineRPM());

	DrawLabel(GearName(Car.GetDisplayGear()), CenterX - 22.f, Top + 34.f, ColorText, 2.6f);

	const FString Speed = FString::Printf(TEXT("%d"), FMath::RoundToInt(FMath::Abs(Car.GetSpeedKph())));
	DrawLabel(Speed, CenterX + 40.f, Top + 44.f, ColorText, 1.8f);
	DrawLabel(TEXT("km/h"), CenterX + 118.f, Top + 62.f, ColorDim, 0.9f);
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

	DrawRect(ColorPanel, X, Y, 300.f, 150.f);

	DrawLabel(FString::Printf(TEXT("P%d / %d"), Entry->Position, RaceState.GetEntries().Num()), X + 14.f, Y + 10.f, ColorText, 1.6f);
	DrawLabel(FString::Printf(TEXT("VUELTA %d / %d"), FMath::Min(Entry->LapsCompleted + 1, RaceState.GetTotalLaps()), RaceState.GetTotalLaps()),
		X + 14.f, Y + 46.f, ColorDim, 1.f);

	DrawLabel(FormatLapTime(Entry->CurrentLapTime), X + 14.f, Y + 68.f, ColorText, 1.2f);
	DrawLabel(FString::Printf(TEXT("MEJOR  %s"), *FormatLapTime(Entry->BestLap.TotalSeconds)), X + 14.f, Y + 96.f, ColorDim, 0.9f);

	const float GapAhead = RaceState.GetGapAheadSeconds(&Car);
	const FString GapText = GapAhead < TNumericLimits<float>::Max()
		? FString::Printf(TEXT("DELANTE %s"), *FormatDelta(GapAhead))
		: TEXT("LÍDER");
	DrawLabel(GapText, X + 14.f, Y + 118.f, GapAhead < 1.f ? ColorGood : ColorDim, 0.9f);

	if (Entry->PenaltySeconds > 0.f)
	{
		DrawLabel(FString::Printf(TEXT("SANCIÓN +%.0f s"), Entry->PenaltySeconds), X + 170.f, Y + 118.f, ColorAccent, 0.9f);
	}
}

void APaxHUD::DrawTyres(const AF1Car& Car)
{
	const UTyreComponent* TyreSystem = Car.GetTyres();
	if (!TyreSystem)
	{
		return;
	}

	const float X = Margin;
	const float Y = Canvas->SizeY - Margin - 150.f;

	DrawRect(ColorPanel, X, Y, 230.f, 150.f);
	DrawLabel(CompoundName(TyreSystem->GetCompound()), X + 14.f, Y + 10.f, ColorText, 1.1f);
	DrawLabel(FString::Printf(TEXT("%.1f vueltas"), TyreSystem->GetSetAgeLaps()), X + 130.f, Y + 14.f, ColorDim, 0.8f);

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
		const float CellX = X + 20.f + (Slot % 2) * 110.f;
		const float CellY = Y + 44.f + (Slot / 2) * 52.f;

		// Color por temperatura: azul frío, verde en ventana, rojo pasado.
		const float TempC = Tyre.TemperatureC;
		FLinearColor TempColor = ColorGood;
		if (TempC < 80.f) TempColor = ColorERS;
		else if (TempC > 125.f) TempColor = ColorAccent;

		DrawBar(CellX, CellY, 80.f, 12.f, Tyre.Life, Tyre.Life > 0.35f ? ColorGood : ColorWarn);
		DrawLabel(FString::Printf(TEXT("%d%%  %.0f°"), FMath::RoundToInt(Tyre.Life * 100.f), TempC),
			CellX, CellY + 16.f, TempColor, 0.75f);
	}
}

void APaxHUD::DrawEnergy(const AF1Car& Car, const APaxGameState& RaceState)
{
	const UERSComponent* ERSSystem = Car.GetERS();
	const UFuelComponent* FuelSystem = Car.GetFuel();

	const float X = Margin + 250.f;
	const float Y = Canvas->SizeY - Margin - 150.f;

	DrawRect(ColorPanel, X, Y, 260.f, 150.f);

	if (ERSSystem)
	{
		DrawLabel(TEXT("ERS"), X + 14.f, Y + 10.f, ColorDim, 0.85f);
		DrawLabel(ERSModeName(ERSSystem->GetMode()), X + 70.f, Y + 10.f, ColorERS, 0.85f);
		DrawBar(X + 14.f, Y + 32.f, 230.f, 14.f, ERSSystem->GetChargeFraction(), ColorERS);
		DrawLabel(FString::Printf(TEXT("%.1f MJ en esta vuelta   %.0f kW"),
			ERSSystem->GetLapDeploymentRemainingMJ(), ERSSystem->GetDeployedPowerKw()),
			X + 14.f, Y + 50.f, ColorDim, 0.75f);
	}

	if (FuelSystem)
	{
		DrawLabel(TEXT("COMBUSTIBLE"), X + 14.f, Y + 78.f, ColorDim, 0.85f);
		DrawLabel(FuelMixName(FuelSystem->GetMix()), X + 150.f, Y + 78.f, ColorWarn, 0.85f);
		DrawLabel(FString::Printf(TEXT("%.1f kg   %.1f vueltas"), FuelSystem->GetFuelKg(), FuelSystem->GetLapsRemaining()),
			X + 14.f, Y + 100.f, ColorText, 0.8f);

		// Margen respecto a lo que falta: si es negativo hay que levantar.
		const FRaceEntry* Entry = RaceState.FindEntry(&Car);
		if (Entry)
		{
			const int32 Remaining = FMath::Max(RaceState.GetTotalLaps() - Entry->LapsCompleted, 0);
			const float Delta = FuelSystem->GetFuelDeltaLaps(Remaining);
			DrawLabel(FString::Printf(TEXT("MARGEN %s vueltas"), *FormatDelta(Delta)),
				X + 14.f, Y + 122.f, Delta < 0.f ? ColorAccent : ColorGood, 0.8f);
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

	const float X = Canvas->SizeX * 0.5f - 60.f;
	const float Y = Margin + 132.f;

	const bool bOpen = DRSSystem->GetState() == EDRSState::Open;
	DrawRect(bOpen ? ColorGood : FLinearColor(0.f, 0.f, 0.f, 0.6f), X, Y, 120.f, 32.f);
	DrawLabel(bOpen ? TEXT("DRS ACTIVO") : TEXT("DRS LISTO"), X + 12.f, Y + 8.f,
		bOpen ? FLinearColor::Black : ColorGood, 0.85f);
}

void APaxHUD::DrawStandings(const APaxGameState& RaceState)
{
	const TArray<FRaceEntry>& Entries = RaceState.GetEntries();
	if (Entries.Num() == 0)
	{
		return;
	}

	const float RowHeight = 22.f;
	const int32 MaxRows = FMath::Min(Entries.Num(), 12);
	const float Width = 260.f;
	const float X = Canvas->SizeX - Margin - Width;
	const float Y = Margin;

	DrawRect(ColorPanel, X, Y, Width, MaxRows * RowHeight + 16.f);

	for (int32 Index = 0; Index < MaxRows; ++Index)
	{
		const FRaceEntry& Entry = Entries[Index];
		const float RowY = Y + 8.f + Index * RowHeight;

		const FLinearColor RowColor = Entry.bIsPlayer ? ColorAccent : ColorText;
		DrawLabel(FString::Printf(TEXT("%2d  %s"), Entry.Position, *Entry.DriverName), X + 12.f, RowY, RowColor, 0.8f);

		const FString Right = Index == 0
			? FormatLapTime(Entry.BestLap.TotalSeconds)
			: FormatDelta(RaceState.GetGapToLeaderSeconds(Entry.Car));
		DrawLabel(Right, X + Width - 90.f, RowY, ColorDim, 0.75f);
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
	const float LightSize = 46.f;
	const float Spacing = 14.f;
	const float TotalWidth = 5 * LightSize + 4 * Spacing;
	const float X = Canvas->SizeX * 0.5f - TotalWidth * 0.5f;
	const float Y = Canvas->SizeY * 0.22f;

	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.75f), X - 16.f, Y - 16.f, TotalWidth + 32.f, LightSize + 32.f);

	for (int32 Index = 0; Index < 5; ++Index)
	{
		const bool bOn = Index < LightsOn;
		DrawRect(bOn ? ColorAccent : FLinearColor(0.12f, 0.12f, 0.14f, 1.f),
			X + Index * (LightSize + Spacing), Y, LightSize, LightSize);
	}
}
