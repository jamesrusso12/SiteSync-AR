#if PLATFORM_IOS

#include "ARMeshBlueprintLibrary.h"
#include "ARTrackable.h"
#include "ARSupportInterface.h"
#include "AppleARKitConversion.h"
#include "Engine/Engine.h"
#include "IXRTrackingSystem.h"
#include "Logging/LogMacros.h"

#import <ARKit/ARKit.h>

DEFINE_LOG_CATEGORY_STATIC(LogSiteSyncARiOS, Log, All);

bool UARMeshBlueprintLibrary::GetARMeshData(UARMeshGeometry* MeshGeometry,
                                             TArray<FVector>& OutVertices,
                                             TArray<int32>& OutIndices)
{
	OutVertices.Reset();
	OutIndices.Reset();

	if (!MeshGeometry)
	{
		UE_LOG(LogSiteSyncARiOS, Warning, TEXT("GetARMeshData: null MeshGeometry"));
		return false;
	}

	if (GEngine == nullptr || !GEngine->XRSystem.IsValid())
	{
		UE_LOG(LogSiteSyncARiOS, Warning, TEXT("GetARMeshData: no GEngine or XRSystem"));
		return false;
	}

	TSharedPtr<FARSupportInterface, ESPMode::ThreadSafe> ARSystem = GEngine->XRSystem->GetARCompositionComponent();
	if (!ARSystem.IsValid())
	{
		UE_LOG(LogSiteSyncARiOS, Warning, TEXT("GetARMeshData: no ARCompositionComponent"));
		return false;
	}

	ARSession* Session = (__bridge ARSession*)ARSystem->GetARSessionRawPointer();
	if (Session == nil)
	{
		UE_LOG(LogSiteSyncARiOS, Warning, TEXT("GetARMeshData: no ARSession"));
		return false;
	}

	ARFrame* Frame = Session.currentFrame;
	if (Frame == nil)
	{
		UE_LOG(LogSiteSyncARiOS, Warning, TEXT("GetARMeshData: no currentFrame"));
		return false;
	}

	const FTransform AlignmentTransform = ARSystem->GetAlignmentTransform();
	const FTransform TrackingToWorld = GEngine->XRSystem->GetTrackingToWorldTransform();

	const FGuid TargetId = MeshGeometry->UniqueId;
	ARMeshAnchor* FoundAnchor = nil;
	int32 MeshAnchorCount = 0;
	for (ARAnchor* Anchor in Frame.anchors)
	{
		if (![Anchor isKindOfClass:[ARMeshAnchor class]])
		{
			continue;
		}
		MeshAnchorCount++;
		if (FAppleARKitConversion::ToFGuid(Anchor.identifier) == TargetId)
		{
			FoundAnchor = (ARMeshAnchor*)Anchor;
			break;
		}
	}
	if (FoundAnchor == nil)
	{
		UE_LOG(LogSiteSyncARiOS, Warning, TEXT("GetARMeshData: no matching ARMeshAnchor for UniqueId (scanned %d mesh anchors in frame)"), MeshAnchorCount);
		return false;
	}

	ARMeshGeometry* Geo = FoundAnchor.geometry;
	ARGeometrySource* InVerts = Geo.vertices;
	if (InVerts.componentsPerVector != 3 ||
		InVerts.format != MTLVertexFormatFloat3 ||
		InVerts.buffer == nil ||
		InVerts.buffer.contents == nullptr)
	{
		UE_LOG(LogSiteSyncARiOS, Warning, TEXT("GetARMeshData: vertex buffer malformed (comps=%ld, format=%lu)"), (long)InVerts.componentsPerVector, (unsigned long)InVerts.format);
		return false;
	}

	const NSInteger NumVerts = InVerts.count;
	const NSInteger VertStride = InVerts.stride;
	const uint8* VertBase = (const uint8*)InVerts.buffer.contents + InVerts.offset;
	const simd_float4x4 AnchorXform = FoundAnchor.transform;
	OutVertices.SetNumUninitialized(NumVerts);
	for (NSInteger i = 0; i < NumVerts; ++i)
	{
		const float* V = (const float*)(VertBase + i * VertStride);
		// Vertices are in the anchor's local space (ARKit RH Y-up meters).
		// Multiply by anchor.transform to land in ARKit world, then convert to UE LH Z-up cm.
		const simd_float4 Local = simd_make_float4(V[0], V[1], V[2], 1.0f);
		const simd_float4 World = simd_mul(AnchorXform, Local);
		const FVector InTracking = FVector(-World.z, World.x, World.y) * FAppleARKitConversion::ToUEScale();
		const FVector InAligned = AlignmentTransform.TransformPosition(InTracking);
		OutVertices[i] = TrackingToWorld.TransformPosition(InAligned);
	}

	ARGeometryElement* InFaces = Geo.faces;
	if (InFaces.primitiveType != ARGeometryPrimitiveTypeTriangle ||
		InFaces.indexCountPerPrimitive != 3 ||
		InFaces.bytesPerIndex != sizeof(uint32) ||
		InFaces.buffer == nil ||
		InFaces.buffer.contents == nullptr)
	{
		OutVertices.Reset();
		return false;
	}

	const NSInteger NumTris = InFaces.count;
	const NSInteger NumIndices = NumTris * 3;
	OutIndices.SetNumUninitialized(NumIndices);
	const uint32* SrcIdx = (const uint32*)InFaces.buffer.contents;
	for (NSInteger i = 0; i < NumIndices; ++i)
	{
		OutIndices[i] = (int32)SrcIdx[i];
	}

	UE_LOG(LogSiteSyncARiOS, Verbose, TEXT("GetARMeshData: %ld verts, %ld tris extracted"), (long)NumVerts, (long)NumTris);
	return NumVerts > 0 && NumIndices > 0;
}

#endif // PLATFORM_IOS
