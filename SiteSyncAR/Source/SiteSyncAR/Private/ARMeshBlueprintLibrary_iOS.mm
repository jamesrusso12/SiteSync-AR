// Copyright (c) 2026 James Russo and Cole. All rights reserved. SiteSync AR is proprietary software — see LICENSE.

#if PLATFORM_IOS

#include "ARMeshBlueprintLibrary.h"
#include "ARTrackable.h"
#include "ARSupportInterface.h"
#include "AppleARKitConversion.h"
#include "Engine/Engine.h"
#include "IXRTrackingSystem.h"
#include "Logging/LogMacros.h"

#import <ARKit/ARKit.h>
#import <CoreLocation/CoreLocation.h>

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

// =============================================================================
// CoreLocation shim — Node 2.2a GPS readout.
//
// Replaces UE's LocationServicesIOSImpl which calls -requestAlwaysAuthorization.
// iOS Console.app on 2026-05-23 outdoor test confirmed locationd treats Always-
// without-prior-WhenInUse as "legacy on-demand authorization, not supported for
// new apps" and rejects with kCLErrorDomain Code=1 (Denied) — even when iOS
// Settings shows the app with "While Using + Precise" granted. The correct
// modern pattern is WhenInUse-first.
// =============================================================================

@interface FSiteSyncLocationDelegate : NSObject<CLLocationManagerDelegate>
@property(strong, nonatomic) CLLocationManager* Manager;
@property(assign, atomic) BOOL HasFix;
@property(assign, atomic) double Lat;
@property(assign, atomic) double Lon;
@property(assign, atomic) double Alt;
@property(assign, atomic) double Acc;
+ (instancetype)shared;
- (void)kick;
@end

@implementation FSiteSyncLocationDelegate

+ (instancetype)shared
{
	static FSiteSyncLocationDelegate* sInstance = nil;
	static dispatch_once_t sOnce;
	dispatch_once(&sOnce, ^{
		sInstance = [[FSiteSyncLocationDelegate alloc] init];
	});
	return sInstance;
}

- (instancetype)init
{
	if ((self = [super init]))
	{
		self.HasFix = NO;
		self.Lat = 0.0; self.Lon = 0.0; self.Alt = 0.0; self.Acc = -1.0;
		// CLLocationManager must be created on a thread with a run loop.
		// Game thread on iOS doesn't have one by default — use main queue.
		dispatch_async(dispatch_get_main_queue(), ^{
			self.Manager = [[CLLocationManager alloc] init];
			self.Manager.delegate = self;
			self.Manager.desiredAccuracy = kCLLocationAccuracyBest;
			self.Manager.distanceFilter = kCLDistanceFilterNone;
			UE_LOG(LogSiteSyncARiOS, Warning,
			       TEXT("CoreLocation shim init: requesting WhenInUse authorization"));
			[self.Manager requestWhenInUseAuthorization];
			if ([CLLocationManager locationServicesEnabled])
			{
				[self.Manager startUpdatingLocation];
				UE_LOG(LogSiteSyncARiOS, Warning, TEXT("CoreLocation shim: startUpdatingLocation called"));
			}
			else
			{
				UE_LOG(LogSiteSyncARiOS, Warning,
				       TEXT("CoreLocation shim: locationServicesEnabled=NO — user disabled in Settings"));
			}
		});
	}
	return self;
}

- (void)kick
{
	// First Blueprint call may land before the async init block has run.
	// Kick the manager again to make sure updates are flowing.
	dispatch_async(dispatch_get_main_queue(), ^{
		if (self.Manager != nil && [CLLocationManager locationServicesEnabled])
		{
			[self.Manager startUpdatingLocation];
		}
	});
}

- (void)locationManager:(CLLocationManager*)manager didUpdateLocations:(NSArray<CLLocation*>*)locations
{
	CLLocation* loc = [locations lastObject];
	if (loc == nil) return;
	self.Lat = loc.coordinate.latitude;
	self.Lon = loc.coordinate.longitude;
	self.Alt = loc.altitude;
	self.Acc = loc.horizontalAccuracy;
	if (!self.HasFix)
	{
		UE_LOG(LogSiteSyncARiOS, Warning,
		       TEXT("CoreLocation shim: first fix lat=%.7f lon=%.7f alt=%.2fm acc=±%.1fm"),
		       self.Lat, self.Lon, self.Alt, self.Acc);
	}
	self.HasFix = YES;
}

- (void)locationManager:(CLLocationManager*)manager didFailWithError:(NSError*)error
{
	const FString Domain = error.domain ? FString(error.domain) : TEXT("?");
	const FString Desc = error.localizedDescription ? FString(error.localizedDescription) : TEXT("?");
	UE_LOG(LogSiteSyncARiOS, Warning,
	       TEXT("CoreLocation shim: didFailWithError domain=%s code=%ld desc=%s"),
	       *Domain, (long)error.code, *Desc);
}

- (void)locationManagerDidChangeAuthorization:(CLLocationManager*)manager API_AVAILABLE(ios(14.0))
{
	CLAuthorizationStatus status = manager.authorizationStatus;
	const TCHAR* statusName = TEXT("?");
	switch (status)
	{
		case kCLAuthorizationStatusNotDetermined:       statusName = TEXT("NotDetermined"); break;
		case kCLAuthorizationStatusRestricted:          statusName = TEXT("Restricted"); break;
		case kCLAuthorizationStatusDenied:              statusName = TEXT("Denied"); break;
		case kCLAuthorizationStatusAuthorizedAlways:    statusName = TEXT("AuthorizedAlways"); break;
		case kCLAuthorizationStatusAuthorizedWhenInUse: statusName = TEXT("AuthorizedWhenInUse"); break;
		default: break;
	}
	UE_LOG(LogSiteSyncARiOS, Warning,
	       TEXT("CoreLocation shim: authorization changed → %s (raw=%d)"),
	       statusName, (int)status);

	if (status == kCLAuthorizationStatusAuthorizedWhenInUse ||
	    status == kCLAuthorizationStatusAuthorizedAlways)
	{
		[manager startUpdatingLocation];
	}
}

@end

bool UARMeshBlueprintLibrary::GetDeviceGeoLocation(double& OutLatitude,
                                                   double& OutLongitude,
                                                   double& OutAltitudeMeters,
                                                   double& OutHorizontalAccuracyMeters)
{
	FSiteSyncLocationDelegate* shim = [FSiteSyncLocationDelegate shared];
	if (!shim.HasFix)
	{
		[shim kick];
		OutLatitude = 0.0;
		OutLongitude = 0.0;
		OutAltitudeMeters = 0.0;
		OutHorizontalAccuracyMeters = 0.0;
		return false;
	}
	OutLatitude = shim.Lat;
	OutLongitude = shim.Lon;
	OutAltitudeMeters = shim.Alt;
	OutHorizontalAccuracyMeters = shim.Acc;
	return true;
}

#endif // PLATFORM_IOS
