// Copyright Pax. All Rights Reserved.

#include "TrackSpline.h"
#include "Pax.h"
#include "Components/SplineComponent.h"
#include "EngineUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

ATrackSpline::ATrackSpline()
{
	PrimaryActorTick.bCanEverTick = false;

	CenterLine = CreateDefaultSubobject<USplineComponent>(TEXT("CenterLine"));
	SetRootComponent(CenterLine);
	CenterLine->SetClosedLoop(true);

	RacingLine = CreateDefaultSubobject<USplineComponent>(TEXT("RacingLine"));
	RacingLine->SetupAttachment(CenterLine);
	RacingLine->SetClosedLoop(true);
	RacingLine->ClearSplinePoints(true);

	PitLane = CreateDefaultSubobject<USplineComponent>(TEXT("PitLane"));
	PitLane->SetupAttachment(CenterLine);
	PitLane->SetClosedLoop(false);
	PitLane->ClearSplinePoints(true);

	// Trazado por defecto para que el proyecto sea jugable en un nivel vacío:
	// un circuito de ~4,3 km con recta principal, horquilla y sección rápida.
	// Coordenadas en cm; el eje X es la recta de meta.
	static const FVector DefaultLayout[] = {
		FVector(      0.f,       0.f, 0.f),
		FVector( 120000.f,       0.f, 0.f),
		FVector( 165000.f,   18000.f, 0.f),
		FVector( 172000.f,   62000.f, 0.f),
		FVector( 140000.f,   88000.f, 0.f),
		FVector(  96000.f,   80000.f, 0.f),
		FVector(  74000.f,  104000.f, 0.f),
		FVector(  88000.f,  140000.f, 0.f),
		FVector(  46000.f,  152000.f, 0.f),
		FVector(  14000.f,  126000.f, 0.f),
		FVector( -22000.f,  132000.f, 0.f),
		FVector( -46000.f,  100000.f, 0.f),
		FVector( -30000.f,   58000.f, 0.f),
		FVector( -54000.f,   26000.f, 0.f),
		FVector( -30000.f,   -2000.f, 0.f)
	};

	CenterLine->ClearSplinePoints(false);
	for (const FVector& Point : DefaultLayout)
	{
		CenterLine->AddSplinePoint(Point, ESplineCoordinateSpace::Local, false);
	}
	CenterLine->SetClosedLoop(true, false);
	CenterLine->UpdateSpline();

	// Dos zonas de DRS: recta principal y la recta tras la horquilla.
	FDRSZone MainStraight;
	MainStraight.DetectionFraction = 0.94f;
	MainStraight.StartFraction = 0.98f;
	MainStraight.EndFraction = 0.16f;
	DRSZones.Add(MainStraight);

	FDRSZone BackStraight;
	BackStraight.DetectionFraction = 0.44f;
	BackStraight.StartFraction = 0.48f;
	BackStraight.EndFraction = 0.60f;
	DRSZones.Add(BackStraight);
}

ATrackSpline* ATrackSpline::Get(const UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<ATrackSpline> It(const_cast<UWorld*>(World)); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

void ATrackSpline::BeginPlay()
{
	Super::BeginPlay();

	if (GetTrackLength() <= 0.f)
	{
		UE_LOG(LogPax, Error, TEXT("ATrackSpline '%s' no tiene spline central: el cronometraje y la IA no funcionarán."), *GetName());
	}
}

float ATrackSpline::GetTrackLength() const
{
	return CenterLine ? CenterLine->GetSplineLength() : 0.f;
}

float ATrackSpline::GetDistanceAtLocation(const FVector& WorldLocation) const
{
	if (!CenterLine)
	{
		return 0.f;
	}

	const float Key = CenterLine->FindInputKeyClosestToWorldLocation(WorldLocation);
	return CenterLine->GetDistanceAlongSplineAtSplineInputKey(Key);
}

float ATrackSpline::GetLateralOffsetAtLocation(const FVector& WorldLocation) const
{
	if (!CenterLine)
	{
		return 0.f;
	}

	const float Distance = GetDistanceAtLocation(WorldLocation);
	const FTransform Frame = GetTransformAtDistance(Distance);
	// En el frame de la spline X apunta hacia delante e Y hacia la derecha,
	// así que la componente Y local es directamente el offset con signo.
	return Frame.InverseTransformPosition(WorldLocation).Y;
}

ETrackSurface ATrackSpline::GetSurfaceAtLocation(const FVector& WorldLocation) const
{
	if (IsInPitLane(WorldLocation))
	{
		return ETrackSurface::PitLane;
	}

	const float AbsOffset = FMath::Abs(GetLateralOffsetAtLocation(WorldLocation));
	if (AbsOffset <= TrackHalfWidth)
	{
		return ETrackSurface::Asphalt;
	}
	if (AbsOffset <= TrackHalfWidth + KerbWidth)
	{
		return ETrackSurface::Kerb;
	}
	// Más allá del piano hay escapatoria: asfalto de grava en las curvas
	// rápidas se simplifica aquí como grava, y hierba si está muy lejos.
	if (AbsOffset <= TrackHalfWidth + KerbWidth + 800.f)
	{
		return ETrackSurface::Gravel;
	}
	return ETrackSurface::Grass;
}

bool ATrackSpline::IsWithinTrackLimits(const FVector& WorldLocation, float VehicleHalfWidth) const
{
	if (IsInPitLane(WorldLocation))
	{
		return true;
	}

	// Regla real: se considera fuera cuando ninguna parte del coche toca la
	// línea blanca, es decir cuando el centro se aleja más de medio ancho del
	// coche por fuera del piano.
	const float AbsOffset = FMath::Abs(GetLateralOffsetAtLocation(WorldLocation));
	return AbsOffset <= TrackHalfWidth + KerbWidth + VehicleHalfWidth;
}

int32 ATrackSpline::GetSectorAtDistance(float Distance) const
{
	const float Length = GetTrackLength();
	if (Length <= 0.f)
	{
		return 0;
	}

	const float Fraction = FMath::Fmod(FMath::Max(Distance, 0.f), Length) / Length;
	for (int32 Index = 0; Index < SectorSplitFractions.Num(); ++Index)
	{
		if (Fraction < SectorSplitFractions[Index])
		{
			return Index;
		}
	}
	return SectorSplitFractions.Num();
}

FTransform ATrackSpline::GetTransformAtDistance(float Distance) const
{
	if (!CenterLine)
	{
		return FTransform::Identity;
	}

	const float Length = GetTrackLength();
	const float Wrapped = Length > 0.f ? FMath::Fmod(FMath::Fmod(Distance, Length) + Length, Length) : 0.f;
	return CenterLine->GetTransformAtDistanceAlongSpline(Wrapped, ESplineCoordinateSpace::World);
}

FVector ATrackSpline::GetRacingLineLocationAtDistance(float Distance, float LateralOffset) const
{
	const float Length = GetTrackLength();
	if (Length <= 0.f)
	{
		return GetActorLocation();
	}

	const float Wrapped = FMath::Fmod(FMath::Fmod(Distance, Length) + Length, Length);

	if (RacingLine && RacingLine->GetNumberOfSplinePoints() >= 2)
	{
		// La trazada puede tener otra longitud que el eje: se mapea por fracción.
		const float RacingDistance = (Wrapped / Length) * RacingLine->GetSplineLength();
		const FTransform Frame = RacingLine->GetTransformAtDistanceAlongSpline(RacingDistance, ESplineCoordinateSpace::World);
		return Frame.TransformPosition(FVector(0.f, LateralOffset, 0.f));
	}

	const FTransform Frame = GetTransformAtDistance(Wrapped);
	return Frame.TransformPosition(FVector(0.f, LateralOffset, 0.f));
}

float ATrackSpline::GetCurvatureAtDistance(float Distance) const
{
	const float Length = GetTrackLength();
	if (!CenterLine || Length <= 0.f)
	{
		return 0.f;
	}

	// Curvatura por diferencias finitas sobre la dirección de la tangente.
	// 10 m de paso filtra el ruido de los puntos de control sin perder el
	// radio real de las curvas lentas.
	constexpr float Step = 1000.f;
	const FVector Before = GetTransformAtDistance(Distance - Step).GetRotation().GetForwardVector();
	const FVector After = GetTransformAtDistance(Distance + Step).GetRotation().GetForwardVector();

	const float Angle = FMath::Acos(FMath::Clamp(FVector::DotProduct(Before.GetSafeNormal2D(), After.GetSafeNormal2D()), -1.f, 1.f));
	return Angle / (2.f * Step);
}

float ATrackSpline::GetSignedDistanceDelta(float FromDistance, float ToDistance) const
{
	const float Length = GetTrackLength();
	if (Length <= 0.f)
	{
		return ToDistance - FromDistance;
	}

	float Delta = FMath::Fmod(ToDistance - FromDistance, Length);
	if (Delta > Length * 0.5f)
	{
		Delta -= Length;
	}
	else if (Delta < -Length * 0.5f)
	{
		Delta += Length;
	}
	return Delta;
}

int32 ATrackSpline::GetActiveDRSZone(float Distance) const
{
	const float Length = GetTrackLength();
	if (Length <= 0.f)
	{
		return INDEX_NONE;
	}

	const float Fraction = FMath::Fmod(FMath::Fmod(Distance, Length) + Length, Length) / Length;
	for (int32 Index = 0; Index < DRSZones.Num(); ++Index)
	{
		const FDRSZone& Zone = DRSZones[Index];
		// Una zona puede cruzar la línea de meta (Start > End).
		const bool bInside = Zone.StartFraction <= Zone.EndFraction
			? (Fraction >= Zone.StartFraction && Fraction <= Zone.EndFraction)
			: (Fraction >= Zone.StartFraction || Fraction <= Zone.EndFraction);

		if (bInside)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

float ATrackSpline::GetDRSDetectionDistance(int32 ZoneIndex) const
{
	if (!DRSZones.IsValidIndex(ZoneIndex))
	{
		return 0.f;
	}
	return DRSZones[ZoneIndex].DetectionFraction * GetTrackLength();
}

FTransform ATrackSpline::GetGridSlotTransform(int32 GridPosition) const
{
	const float Length = GetTrackLength();
	if (Length <= 0.f)
	{
		return GetActorTransform();
	}

	// La parrilla se extiende hacia atrás desde la línea de meta (distancia 0)
	// y alterna lados, como la disposición real en zigzag.
	const float Distance = Length - GridStartSetback - GridPosition * GridSlotSpacing;
	const float Lateral = (GridPosition % 2 == 0) ? -GridSlotLateralOffset : GridSlotLateralOffset;

	FTransform Frame = GetTransformAtDistance(Distance);
	Frame.SetLocation(Frame.TransformPosition(FVector(0.f, Lateral, 0.f)));
	Frame.SetScale3D(FVector::OneVector);
	return Frame;
}

bool ATrackSpline::IsInPitLane(const FVector& WorldLocation) const
{
	if (!PitLane || PitLane->GetNumberOfSplinePoints() < 2)
	{
		return false;
	}

	const FVector Closest = PitLane->FindLocationClosestToWorldLocation(WorldLocation, ESplineCoordinateSpace::World);
	return FVector::Dist2D(Closest, WorldLocation) <= PitLaneHalfWidth;
}

void ATrackSpline::ImportCenterlineFromCSV()
{
	if (!CenterLine)
	{
		return;
	}

	const FString FullPath = FPaths::IsRelative(CenterlineCSVPath)
		? FPaths::Combine(FPaths::ProjectDir(), CenterlineCSVPath)
		: CenterlineCSVPath;

	TArray<FString> Lines;
	if (!FFileHelper::LoadFileToStringArray(Lines, *FullPath))
	{
		UE_LOG(LogPax, Error, TEXT("No se pudo leer el CSV de centerline en '%s'."), *FullPath);
		return;
	}

	TArray<FVector> Points;
	float WidthSum = 0.f;
	int32 WidthSamples = 0;

	for (const FString& Line : Lines)
	{
		const FString Trimmed = Line.TrimStartAndEnd();
		if (Trimmed.IsEmpty() || Trimmed.StartsWith(TEXT("#")) || Trimmed.StartsWith(TEXT("x")))
		{
			continue; // comentario o cabecera
		}

		TArray<FString> Fields;
		Trimmed.ParseIntoArray(Fields, TEXT(","), true);
		if (Fields.Num() < 3)
		{
			continue;
		}

		Points.Emplace(FCString::Atof(*Fields[0]), FCString::Atof(*Fields[1]), FCString::Atof(*Fields[2]));

		if (Fields.Num() >= 4)
		{
			WidthSum += FCString::Atof(*Fields[3]);
			++WidthSamples;
		}
	}

	if (Points.Num() < 3)
	{
		UE_LOG(LogPax, Error, TEXT("El CSV '%s' tiene %d puntos válidos; hacen falta al menos 3."), *FullPath, Points.Num());
		return;
	}

	CenterLine->ClearSplinePoints(false);
	for (const FVector& Point : Points)
	{
		CenterLine->AddSplinePoint(Point, ESplineCoordinateSpace::Local, false);
	}
	CenterLine->SetClosedLoop(true, false);
	CenterLine->UpdateSpline();

	if (WidthSamples > 0)
	{
		TrackHalfWidth = (WidthSum / WidthSamples) * 0.5f;
	}

	UE_LOG(LogPax, Log, TEXT("Centerline importada: %d puntos, %.0f m, semiancho %.0f cm."),
		Points.Num(), GetTrackLength() / 100.f, TrackHalfWidth);
}
