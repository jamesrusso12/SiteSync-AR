#include "ARMeshBlueprintLibrary.h"
#include "ARBlueprintLibrary.h"
#include "ARTrackable.h"
#include "Logging/LogMacros.h"
#include "ProceduralMeshComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogSiteSyncAR, Log, All);

TArray<UARMeshGeometry*> UARMeshBlueprintLibrary::GetAllARMeshGeometries()
{
	TArray<UARMeshGeometry*> Result;
	const TArray<UARTrackedGeometry*> AllGeos = UARBlueprintLibrary::GetAllGeometries();
	for (UARTrackedGeometry* Geometry : AllGeos)
	{
		if (UARMeshGeometry* Mesh = Cast<UARMeshGeometry>(Geometry))
		{
			Result.Add(Mesh);
		}
	}
	UE_LOG(LogSiteSyncAR, Log, TEXT("GetAllARMeshGeometries: %d total tracked geometries, %d mesh geometries"), AllGeos.Num(), Result.Num());
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

int32 UARMeshBlueprintLibrary::UpdateLiDARMeshes(UProceduralMeshComponent* TargetMeshComponent,
                                                 UMaterialInterface* DebugMaterial)
{
	if (!TargetMeshComponent)
	{
		UE_LOG(LogSiteSyncAR, Warning, TEXT("UpdateLiDARMeshes: null TargetMeshComponent"));
		return 0;
	}

	TargetMeshComponent->ClearAllMeshSections();

	const TArray<UARMeshGeometry*> MeshGeometries = GetAllARMeshGeometries();
	int32 SectionCount = 0;

	for (int32 Index = 0; Index < MeshGeometries.Num(); ++Index)
	{
		UARMeshGeometry* Geometry = MeshGeometries[Index];
		if (!Geometry)
		{
			continue;
		}

		TArray<FVector> Vertices;
		TArray<int32> Indices;
		if (!GetARMeshData(Geometry, Vertices, Indices))
		{
			continue;
		}

		// Transform vertices from anchor-local space into world space so the rendered mesh
		// aligns with the physical environment the phone is scanning.
		const FTransform LocalToWorld = Geometry->GetLocalToWorldTransform();
		for (FVector& V : Vertices)
		{
			V = LocalToWorld.TransformPosition(V);
		}

		const TArray<FVector> EmptyNormals;
		const TArray<FVector2D> EmptyUVs;
		const TArray<FColor> EmptyColors;
		const TArray<FProcMeshTangent> EmptyTangents;

		TargetMeshComponent->CreateMeshSection(
			SectionCount,
			Vertices,
			Indices,
			EmptyNormals,
			EmptyUVs,
			EmptyColors,
			EmptyTangents,
			/*bCreateCollision=*/false);

		if (DebugMaterial)
		{
			TargetMeshComponent->SetMaterial(SectionCount, DebugMaterial);
		}

		SectionCount++;
	}

	UE_LOG(LogSiteSyncAR, Log, TEXT("UpdateLiDARMeshes: populated %d sections from %d geometries"), SectionCount, MeshGeometries.Num());
	return SectionCount;
}
