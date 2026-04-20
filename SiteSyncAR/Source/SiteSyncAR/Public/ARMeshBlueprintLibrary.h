#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ARMeshBlueprintLibrary.generated.h"

class UARMeshGeometry;

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
};
