#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ARMeshBlueprintLibrary.generated.h"

class UARMeshGeometry;
class UProceduralMeshComponent;
class UMaterialInterface;

UCLASS()
class SITESYNCAR_API UARMeshBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Extracts raw vertex and index arrays from an ARMeshGeometry anchor.
	// Returns false if the geometry is null or contains no vertices.
	UFUNCTION(BlueprintCallable, Category = "SiteSync|AR")
	static bool GetARMeshData(UARMeshGeometry* MeshGeometry,
	                          TArray<FVector>& OutVertices,
	                          TArray<int32>& OutIndices);

	// Returns all currently tracked ARMeshGeometry anchors from the active AR session.
	UFUNCTION(BlueprintCallable, Category = "SiteSync|AR")
	static TArray<UARMeshGeometry*> GetAllARMeshGeometries();

	// All-in-one rebuild: clears and repopulates every mesh section on the given
	// ProceduralMeshComponent from the current AR mesh anchors. Intended to be called
	// from a looping BP timer so the BP graph stays trivial. Returns the number of
	// sections populated (0 is normal before ARKit has scanned any surfaces).
	UFUNCTION(BlueprintCallable, Category = "SiteSync|AR")
	static int32 UpdateLiDARMeshes(UProceduralMeshComponent* TargetMeshComponent,
	                               UMaterialInterface* DebugMaterial);

	// Computes earthwork cut and fill volumes between the LiDAR-tracked terrain and the
	// slab BOTTOM face (subgrade convention), clipped to the slab's XY footprint.
	// FoundationActor's scale is interpreted as the slab's full extent in METERS
	// (matches BP_Foundation.InitFromCorners convention) — multiplied by 100 internally
	// for cm. Output volumes are in cubic yards (US AEC unit).
	// Returns false if inputs are invalid or the slab has zero scale on any axis.
	UFUNCTION(BlueprintCallable, Category = "SiteSync|Volume")
	static bool CalculateCutFillVolumes(UProceduralMeshComponent* TerrainMesh,
	                                    AActor* FoundationActor,
	                                    float& OutCutCubicYards,
	                                    float& OutFillCubicYards);

	// Places FoundationActor as an edge-aligned slab spanning CornerA → CornerB (long edge),
	// with the given width and thickness. Sets actor location (midpoint), yaw rotation
	// (atan2(deltaY, deltaX)), and scale3D in meters — replacing the fragile BP exec chain
	// in BP_Foundation.InitFromCorners. WidthCm / ThicknessCm are clamped to [50,5000] /
	// [5,50] respectively. Returns false on null actor.
	UFUNCTION(BlueprintCallable, Category = "SiteSync|Foundation")
	static bool InitFoundationFromCorners(AActor* FoundationActor,
	                                      FVector CornerA,
	                                      FVector CornerB,
	                                      float WidthCm = 500.0f,
	                                      float ThicknessCm = 10.0f);
};
