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
};
