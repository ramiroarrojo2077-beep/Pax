// Copyright Pax. All Rights Reserved.

#include "PaxTypes.h"

FString FormatLapTime(float Seconds)
{
	if (Seconds <= 0.f)
	{
		return TEXT("--:--.---");
	}

	const int32 Minutes = FMath::FloorToInt(Seconds / 60.f);
	const float Remainder = Seconds - Minutes * 60.f;
	return FString::Printf(TEXT("%d:%06.3f"), Minutes, Remainder);
}

FString FormatDelta(float Seconds)
{
	if (FMath::IsNearlyZero(Seconds, 0.001f))
	{
		return TEXT("+0.000");
	}

	return FString::Printf(TEXT("%s%.3f"), Seconds < 0.f ? TEXT("-") : TEXT("+"), FMath::Abs(Seconds));
}
