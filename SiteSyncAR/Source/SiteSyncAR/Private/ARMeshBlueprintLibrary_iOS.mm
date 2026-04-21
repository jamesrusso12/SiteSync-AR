#if PLATFORM_IOS

#include "ARMeshBlueprintLibrary.h"
#include "ARTrackable.h"
#include "ARSupportInterface.h"
#include "AppleARKitConversion.h"
#include "Engine/Engine.h"
#include "IXRTrackingSystem.h"

#import <ARKit/ARKit.h>

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

	if (GEngine == nullptr || !GEngine->XRSystem.IsValid())
	{
		return false;
	}

	TSharedPtr<FARSupportInterface, ESPMode::ThreadSafe> ARSystem = GEngine->XRSystem->GetARCompositionComponent();
	if (!ARSystem.IsValid())
	{
		return false;
	}

	ARSession* Session = (__bridge ARSession*)ARSystem->GetARSessionRawPointer();
	if (Session == nil)
	{
		return false;
	}

	ARFrame* Frame = Session.currentFrame;
	if (Frame == nil)
	{
		return false;
	}

	const FGuid TargetId = MeshGeometry->UniqueId;
	ARMeshAnchor* FoundAnchor = nil;
	for (ARAnchor* Anchor in Frame.anchors)
	{
		if (![Anchor isKindOfClass:[ARMeshAnchor class]])
		{
			continue;
		}
		if (FAppleARKitConversion::ToFGuid(Anchor.identifier) == TargetId)
		{
			FoundAnchor = (ARMeshAnchor*)Anchor;
			break;
		}
	}
	if (FoundAnchor == nil)
	{
		return false;
	}

	ARMeshGeometry* Geo = FoundAnchor.geometry;
	ARGeometrySource* InVerts = Geo.vertices;
	if (InVerts.componentsPerVector != 3 ||
		InVerts.format != MTLVertexFormatFloat3 ||
		InVerts.buffer == nil ||
		InVerts.buffer.contents == nullptr)
	{
		return false;
	}

	const NSInteger NumVerts = InVerts.count;
	const NSInteger VertStride = InVerts.stride;
	const uint8* VertBase = (const uint8*)InVerts.buffer.contents + InVerts.offset;
	OutVertices.SetNumUninitialized(NumVerts);
	for (NSInteger i = 0; i < NumVerts; ++i)
	{
		const float* V = (const float*)(VertBase + i * VertStride);
		// ARKit: right-handed, Y-up, meters. UE: left-handed, Z-up, cm.
		OutVertices[i] = FVector(-V[2], V[0], V[1]) * FAppleARKitConversion::ToUEScale();
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

	return NumVerts > 0 && NumIndices > 0;
}

#endif // PLATFORM_IOS
