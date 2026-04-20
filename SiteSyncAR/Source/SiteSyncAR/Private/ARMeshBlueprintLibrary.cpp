#include "ARMeshBlueprintLibrary.h"
#include "ARBlueprintLibrary.h"
#include "ARMeshGeometry.h"

bool UARMeshBlueprintLibrary::GetARMeshData(UARMeshGeometry* MeshGeometry,
                                             TArray<FVector>& OutVertices,
                                             TArray<int32>& OutIndices)
{
	if (!MeshGeometry) return false;

	OutVertices = MeshGeometry->GetVertexBuffer();
	OutIndices  = MeshGeometry->GetIndexBuffer();
	return OutVertices.Num() > 0;
}

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
