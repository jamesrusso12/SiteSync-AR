#include "ARMeshBlueprintLibrary.h"
#include "ARBlueprintLibrary.h"
#include "ARTrackable.h"

// GetARMeshData:
//
// In UE 5.5, UARMeshGeometry does NOT expose vertex/index buffers directly.
// The mesh data is held in two places:
//   - iOS runtime: FARKitMeshData (AppleARKit plugin, private cache keyed by FGuid)
//   - Cross-platform: UMRMeshComponent "bricks" (MRMesh module, chunked storage)
//
// For the LiDAR Node 1.2 prototype, the proper iOS implementation requires
// accessing the FARKitMeshData cache through PLATFORM_IOS-guarded code.
// On Windows (editor / non-iOS targets) there is no LiDAR source, so this
// function always returns false and leaves the arrays empty.
//
// TODO (Node 1.2 iOS pass): implement iOS-specific vertex/index extraction
// via FARKitMeshData::GetMeshData(MeshGeometry->GetTrackableId()) under
// #if PLATFORM_IOS — requires a dependency on AppleARKit module (iOS-only).
bool UARMeshBlueprintLibrary::GetARMeshData(UARMeshGeometry* MeshGeometry,
                                             TArray<FVector>& OutVertices,
                                             TArray<int32>& OutIndices)
{
	OutVertices.Reset();
	OutIndices.Reset();

	if (!MeshGeometry)
	{
		return false;
	}

	// Cross-platform stub. On iOS the real mesh data must be read from the
	// AppleARKit mesh data cache. See FARKitMeshData in the AppleARKit plugin.
	return false;
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
