#include "ARMeshBlueprintLibrary.h"
#include "ARBlueprintLibrary.h"
#include "ARTrackable.h"

TArray<UARMeshGeometry*> UARMeshBlueprintLibrary::GetAllARMeshGeometries()
{
	TArray<UARMeshGeometry*> Result;
	for (UARTrackedGeometry* Geometry : UARBlueprintLibrary::GetAllGeometries())
	{
		if (UARMeshGeometry* Mesh = Cast<UARMeshGeometry>(Geometry))
		{
			Result.Add(Mesh);
		}
	}
	return Result;
}

#if !PLATFORM_IOS
bool UARMeshBlueprintLibrary::GetARMeshData(UARMeshGeometry* MeshGeometry,
                                             TArray<FVector>& OutVertices,
                                             TArray<int32>& OutIndices)
{
	OutVertices.Reset();
	OutIndices.Reset();
	return false;
}
#endif
