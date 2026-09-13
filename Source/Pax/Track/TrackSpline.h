// Copyright Pax. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/PaxTypes.h"
#include "TrackSpline.generated.h"

class USplineComponent;
class UProceduralMeshComponent;
class UMaterialInterface;

/**
 * Una zona de DRS: el punto de detección mide la diferencia con el coche de
 * delante y el tramo activo es donde el alerón puede abrirse.
 * Todo se expresa como fracción [0,1] de la longitud del circuito, así la
 * definición no depende de la escala del trazado.
 */
USTRUCT(BlueprintType)
struct FDRSZone
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DRS", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DetectionFraction = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DRS", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float StartFraction = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DRS", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EndFraction = 0.f;
};

/**
 * Descripción geométrica del circuito.
 *
 * Todo el cronometraje, la IA, el DRS y la detección de límites de pista se
 * apoyan en una sola magnitud: la distancia recorrida a lo largo de la spline
 * central. Eso evita tener que sembrar el nivel con volúmenes de checkpoint y
 * hace que un trazado nuevo funcione sin más que importar su centerline.
 *
 * El CSV que consume ImportCenterlineFromCSV lo genera el script de Blender
 * Tools/Blender/build_assets.py, de modo que la malla del circuito y la spline
 * de juego salen siempre de la misma fuente.
 *
 * Además el actor sabe construirse su propio asfalto: a partir de la misma
 * spline genera en tiempo de ejecución la calzada, los pianos, la escapatoria
 * y los muros, con su colisión. Eso permite empaquetar un APK jugable sin
 * importar ninguna malla de circuito, y garantiza que lo que se pisa coincide
 * exactamente con lo que mide el cronómetro. Si se asigna una malla de
 * circuito hecha en Blender, basta con desactivar bBuildRuntimeMesh.
 */
UCLASS()
class PAX_API ATrackSpline : public AActor
{
	GENERATED_BODY()

public:
	ATrackSpline();

	/** Devuelve el circuito del nivel actual, o nullptr si no hay ninguno. */
	static ATrackSpline* Get(const UWorld* World);

	/** Longitud total del trazado en cm. */
	UFUNCTION(BlueprintPure, Category = "Pax|Track")
	float GetTrackLength() const;

	/** Distancia [0, Length) del punto de la spline más cercano a WorldLocation. */
	UFUNCTION(BlueprintPure, Category = "Pax|Track")
	float GetDistanceAtLocation(const FVector& WorldLocation) const;

	/** Desplazamiento lateral con signo respecto al eje de pista (+ = derecha). */
	UFUNCTION(BlueprintPure, Category = "Pax|Track")
	float GetLateralOffsetAtLocation(const FVector& WorldLocation) const;

	/** Superficie estimada bajo una posición, a partir de su offset lateral. */
	UFUNCTION(BlueprintPure, Category = "Pax|Track")
	ETrackSurface GetSurfaceAtLocation(const FVector& WorldLocation) const;

	/** Cuatro ruedas dentro del asfalto (más el ancho del coche). */
	UFUNCTION(BlueprintPure, Category = "Pax|Track")
	bool IsWithinTrackLimits(const FVector& WorldLocation, float VehicleHalfWidth = 100.f) const;

	/** Sector (0..NumSectors-1) correspondiente a una distancia. */
	UFUNCTION(BlueprintPure, Category = "Pax|Track")
	int32 GetSectorAtDistance(float Distance) const;

	UFUNCTION(BlueprintPure, Category = "Pax|Track")
	int32 GetNumSectors() const { return SectorSplitFractions.Num() + 1; }

	/** Transform sobre el eje de pista a una distancia dada. */
	UFUNCTION(BlueprintPure, Category = "Pax|Track")
	FTransform GetTransformAtDistance(float Distance) const;

	/**
	 * Punto de la trayectoria ideal a una distancia dada.
	 * Si el actor tiene una spline de trazada la usa; si no, devuelve el eje
	 * desplazado por LateralOffset (en cm, + = derecha), que es lo que usa la
	 * IA para abrir la línea o defender el interior.
	 */
	UFUNCTION(BlueprintPure, Category = "Pax|Track")
	FVector GetRacingLineLocationAtDistance(float Distance, float LateralOffset = 0.f) const;

	/**
	 * Curvatura (1/radio, en 1/cm) del eje de pista a una distancia dada.
	 * La IA la usa para calcular la velocidad máxima de paso por curva.
	 */
	UFUNCTION(BlueprintPure, Category = "Pax|Track")
	float GetCurvatureAtDistance(float Distance) const;

	/** Diferencia de distancia entre dos puntos teniendo en cuenta el cierre del circuito. */
	UFUNCTION(BlueprintPure, Category = "Pax|Track")
	float GetSignedDistanceDelta(float FromDistance, float ToDistance) const;

	/** Índice de la zona de DRS activa en esa distancia, o INDEX_NONE. */
	UFUNCTION(BlueprintPure, Category = "Pax|Track")
	int32 GetActiveDRSZone(float Distance) const;

	UFUNCTION(BlueprintPure, Category = "Pax|Track")
	int32 GetNumDRSZones() const { return DRSZones.Num(); }

	/** Punto de detección de la zona de DRS indicada, en distancia absoluta. */
	UFUNCTION(BlueprintPure, Category = "Pax|Track")
	float GetDRSDetectionDistance(int32 ZoneIndex) const;

	/** Posición y orientación del puesto de parrilla indicado (0 = pole). */
	UFUNCTION(BlueprintPure, Category = "Pax|Track")
	FTransform GetGridSlotTransform(int32 GridPosition) const;

	/** ¿Está esta distancia dentro del pit lane (límite de velocidad activo)? */
	UFUNCTION(BlueprintPure, Category = "Pax|Track")
	bool IsInPitLane(const FVector& WorldLocation) const;

	UFUNCTION(BlueprintPure, Category = "Pax|Track")
	float GetPitSpeedLimitKph() const { return PitSpeedLimitKph; }

	USplineComponent* GetCenterLine() const { return CenterLine; }

	/**
	 * Genera calzada, pianos, escapatoria y muros a partir de la spline.
	 * Se llama sola al empezar la partida; en el editor sirve para ver el
	 * resultado tras mover un punto de la spline.
	 */
	UFUNCTION(CallInEditor, Category = "Pax|Track")
	void BuildRuntimeMesh();

	/**
	 * Reconstruye la spline central desde un CSV "x,y,z,width" en cm.
	 * Pensado para ejecutarse desde el detalle del actor tras regenerar el
	 * circuito en Blender.
	 */
	UFUNCTION(CallInEditor, Category = "Pax|Track")
	void ImportCenterlineFromCSV();

protected:
	virtual void BeginPlay() override;

	/** Eje de pista. Debe ser una spline cerrada. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pax|Track")
	TObjectPtr<USplineComponent> CenterLine;

	/** Trazada ideal opcional; si está vacía la IA usa el eje. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pax|Track")
	TObjectPtr<USplineComponent> RacingLine;

	/** Recorrido de boxes, del punto de entrada al de salida. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pax|Track")
	TObjectPtr<USplineComponent> PitLane;

	/** Calzada generada a partir de la spline. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pax|Track")
	TObjectPtr<UProceduralMeshComponent> RoadMesh;

	/** Construir la calzada al empezar. Desactívalo si usas una malla propia. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pax|Track|Mesh")
	bool bBuildRuntimeMesh = true;

	/**
	 * Secciones transversales a lo largo del trazado.
	 * 360 muestras en un circuito de 7 km salen a unos 20 m por sección, que
	 * es suficiente para que las curvas no se vean facetadas y deja la malla
	 * en unos pocos miles de triángulos: barato incluso en un móvil.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pax|Track|Mesh", meta = (ClampMin = "32", ClampMax = "2000"))
	int32 MeshSamples = 360;

	/** Ancho de la escapatoria más allá del piano, en cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pax|Track|Mesh")
	float RunoffWidth = 1200.f;

	/** Altura del muro exterior, en cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pax|Track|Mesh")
	float WallHeight = 120.f;

	/** Altura del borde exterior del piano, en cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pax|Track|Mesh")
	float KerbHeight = 5.f;

	/** Longitud de repetición de las texturas, en cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pax|Track|Mesh")
	float TextureTileSize = 800.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pax|Track|Mesh")
	TObjectPtr<UMaterialInterface> AsphaltMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pax|Track|Mesh")
	TObjectPtr<UMaterialInterface> KerbMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pax|Track|Mesh")
	TObjectPtr<UMaterialInterface> RunoffMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pax|Track|Mesh")
	TObjectPtr<UMaterialInterface> WallMaterial;

	/** Semiancho del asfalto en cm. 500 cm ≈ 10 m de pista útil. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pax|Track", meta = (ClampMin = "100.0"))
	float TrackHalfWidth = 600.f;

	/** Ancho del piano a cada lado, más allá del asfalto. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pax|Track", meta = (ClampMin = "0.0"))
	float KerbWidth = 120.f;

	/** Cortes de sector como fracción de vuelta. Dos cortes = tres sectores. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pax|Track")
	TArray<float> SectorSplitFractions = { 0.34f, 0.68f };

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pax|Track")
	TArray<FDRSZone> DRSZones;

	/** Separación longitudinal entre puestos de parrilla, en cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pax|Track")
	float GridSlotSpacing = 800.f;

	/** Desplazamiento lateral de la parrilla en zigzag, en cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pax|Track")
	float GridSlotLateralOffset = 250.f;

	/** Distancia antes de la línea de meta donde arranca la parrilla, en cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pax|Track")
	float GridStartSetback = 1500.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pax|Track")
	float PitSpeedLimitKph = 80.f;

	/** Semiancho del pit lane en cm, para decidir si un coche está dentro. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pax|Track")
	float PitLaneHalfWidth = 350.f;

private:
	/**
	 * Añade una cinta cerrada entre dos desplazamientos laterales de la spline.
	 * @param SectionIndex   sección de la malla procedural (una por material)
	 * @param InnerOffset    desplazamiento lateral del borde interior, en cm
	 * @param OuterOffset    desplazamiento lateral del borde exterior, en cm
	 * @param InnerHeight    altura del borde interior sobre la calzada, en cm
	 * @param OuterHeight    altura del borde exterior, en cm
	 * @param bVertical      si la cinta sube en vertical (muros) o se tumba
	 */
	void BuildRibbon(int32 SectionIndex, float InnerOffset, float OuterOffset,
		float InnerHeight, float OuterHeight, bool bVertical, UMaterialInterface* Material);

protected:
	/** Ruta al CSV de centerline generado por Blender, relativa a la raíz del proyecto. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pax|Track")
	FString CenterlineCSVPath = TEXT("Tools/Blender/Build/track_centerline.csv");
};
