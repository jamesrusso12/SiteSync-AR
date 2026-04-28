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
	// Returns false if inputs are invalid. Output volumes are in cubic yards (US AEC unit).
	UFUNCTION(BlueprintCallable, Category = "SiteSync|Volume")
	static bool CalculateCutFillVolumes(UProceduralMeshComponent* TerrainMesh,
	                                    AActor* FoundationActor,
	                                    float SlabLengthCm,
	                                    float SlabWidthCm,
	                                    float SlabThicknessCm,
	                                    float& OutCutCubicYards,
	                                    float& OutFillCubicYards);
};
