// Copyright (c) 2026 James Russo and Cole. All rights reserved. SiteSync AR is proprietary software — see LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ARMeshBlueprintLibrary.generated.h"

class UARMeshGeometry;
class UProceduralMeshComponent;
class UMaterialInterface;
class APlayerController;
class UStaticMeshComponent;

// One detected clash between two BIM layer components. Returned by
// DetectBIMClashes for the clash-highlight UI (Node 2.3c/d).
USTRUCT(BlueprintType)
struct FBIMClashPair
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "SiteSync|BIM")
	FString LayerA;

	UPROPERTY(BlueprintReadOnly, Category = "SiteSync|BIM")
	FString LayerB;

	UPROPERTY(BlueprintReadOnly, Category = "SiteSync|BIM")
	UStaticMeshComponent* ComponentA = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "SiteSync|BIM")
	UStaticMeshComponent* ComponentB = nullptr;

	// Approximate world-space center of the clash (midpoint of the two
	// component OBB centers) — for camera-pan-to-clash UX, not precise.
	UPROPERTY(BlueprintReadOnly, Category = "SiteSync|BIM")
	FVector ApproxCenter = FVector::ZeroVector;
};

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
	// FoundationActor's scale is interpreted as the slab's full extent in METERS
	// (matches BP_Foundation.InitFromCorners convention) — multiplied by 100 internally
	// for cm. Output volumes are in cubic yards (US AEC unit). The slab dimensions read
	// from FoundationActor scale are clamped before integration: length/width to
	// [50, 5000] cm, thickness to [5, 50] cm.
	// Returns false if inputs are invalid or the slab has zero scale on any axis.
	UFUNCTION(BlueprintCallable, Category = "SiteSync|Volume")
	static bool CalculateCutFillVolumes(UProceduralMeshComponent* TerrainMesh,
	                                    AActor* FoundationActor,
	                                    float& OutCutCubicYards,
	                                    float& OutFillCubicYards);

	// Places FoundationActor as an edge-aligned slab spanning CornerA → CornerB (long edge),
	// with the given width and thickness. Sets actor location (midpoint), yaw rotation
	// (atan2(deltaY, deltaX)), and scale3D in meters — replacing the fragile BP exec chain
	// in BP_Foundation.InitFromCorners. Clamp ranges: slab length (the CornerA→CornerB XY
	// distance) to [50, 5000] cm, WidthCm to [50, 5000] cm, ThicknessCm to [5, 50] cm.
	// Returns false on null actor.
	UFUNCTION(BlueprintCallable, Category = "SiteSync|Foundation")
	static bool InitFoundationFromCorners(AActor* FoundationActor,
	                                      FVector CornerA,
	                                      FVector CornerB,
	                                      float WidthCm = 500.0f,
	                                      float ThicknessCm = 10.0f);

	// Casts a ray from the given screen position into the LiDAR terrain mesh and returns
	// the world-space location of the nearest triangle hit (Möller-Trumbore). Replaces
	// LineTraceTrackedObjects in BP_ARPlayerController.GetWorldLocationFromTap so marker
	// placement lands on the actual scanned surface instead of the ARKit plane-anchor
	// approximation (which can sit a few cm above the mesh).
	// Returns false if no triangle is hit or inputs are invalid.
	UFUNCTION(BlueprintCallable, Category = "SiteSync|AR")
	static bool RaycastTerrainFromScreen(APlayerController* PlayerController,
	                                     UProceduralMeshComponent* TerrainMesh,
	                                     FVector2D ScreenPosition,
	                                     FVector& OutHitLocation,
	                                     FVector& OutHitNormal);

	// Node 2.1 — places BIMActor at the surveyor "corner + baseline" origin convention:
	// CornerWorld is the building footprint origin (one corner of the slab); ForwardWorld
	// is a second point along the building's +X axis (the baseline shot). Yaw is computed
	// as atan2(ForwardY - CornerY, ForwardX - CornerX). Building dimensions are passed as
	// LengthCm (along +X) / WidthCm (along +Y) / HeightCm (along +Z) and applied as actor
	// scale in METERS — matches BP_Foundation's scale-in-meters convention so the same
	// math reads back via ActorScale × MeshComponent.RelativeScale3D.
	//
	// IMPORTANT pivot convention: BP_BIMOverlay's mesh component must be offset so the
	// building CORNER (not center) sits at the actor origin — otherwise CornerWorld won't
	// match what the user tapped. For a unit cube /Engine/BasicShapes/Cube, this means
	// MeshComponent.RelativeLocation = (50, 50, 50) at component scale 1.0, with the
	// actor handling all dimensional scaling.
	//
	// Clamp ranges: LengthCm / WidthCm to [10, 50000] cm, HeightCm to [10, 30000] cm.
	// The 10cm floor = actor scale 0.1×, which supports tabletop/dollhouse Model Scale
	// for indoor demos alongside the 1:1 Site Scale production use case.
	//
	// Returns false on null actor or zero-distance Corner→Forward delta.
	UFUNCTION(BlueprintCallable, Category = "SiteSync|BIM")
	static bool PlaceBIMByCornerForward(AActor* BIMActor,
	                                    FVector CornerWorld,
	                                    FVector ForwardWorld,
	                                    float LengthCm = 500.0f,
	                                    float WidthCm = 500.0f,
	                                    float HeightCm = 900.0f);

	// Node 2.2a — replacement for UE's flaky LocationServicesIOSImpl.GetLastKnownLocation.
	// Internally manages a CLLocationManager and calls -requestWhenInUseAuthorization
	// (NOT Always — iOS treats Always-without-prior-WhenInUse as legacy on-demand
	// authorization and denies new apps with kCLErrorDomain Code=1). First call
	// triggers the system permission prompt; subsequent calls return the most
	// recent fix or false if no fix yet / unauthorized / location services off.
	//
	// Doubles are used for lat/long so we preserve <1cm precision (float32 carries
	// only ~7 sig figs which loses ~10cm at typical geo magnitudes).
	//
	// Returns false if:
	//   - not running on iOS
	//   - user denied authorization
	//   - location services disabled in system settings
	//   - no GPS fix received yet (transient; HUD should keep polling)
	UFUNCTION(BlueprintCallable, Category = "SiteSync|Geo")
	static bool GetDeviceGeoLocation(double& OutLatitude,
	                                 double& OutLongitude,
	                                 double& OutAltitudeMeters,
	                                 double& OutHorizontalAccuracyMeters);

	// Node 2.2c — equirectangular projection: distance between two lat/long
	// points expressed as a UE world-space offset in centimeters.
	//
	// Math:
	//   metersNorth = (TargetLat - DeviceLat) * π/180 * 6378137
	//   metersEast  = (TargetLong - DeviceLong) * π/180 * 6378137 * cos(DeviceLat)
	//
	// Mapping to UE world axes (requires DA_SiteSyncARConfig.WorldAlignment =
	// GravityAndHeading — see Node 2.2b):
	//   UE +X = true north  -> OutOffsetCm.X = metersNorth * 100
	//   UE +Y = east        -> OutOffsetCm.Y = metersEast  * 100
	//   UE +Z = up          -> OutOffsetCm.Z = 0 (altitude not used here;
	//                                            BIM placement uses LiDAR
	//                                            ground hit for Z)
	//
	// Accuracy: equirectangular is sub-metre out to ~1 km from DeviceLat —
	// fine at construction-site scale. Beyond ~10 km, prefer Haversine.
	//
	// Logs raw inputs and computed metersNorth/metersEast + final FVector at
	// Warning level (visible in default-verbosity device logs) for diagnosis
	// of axis sign issues during 2.2c end-to-end testing.
	//
	// Returns false if any input is NaN/Inf. Returns true otherwise (no
	// distance cap — caller decides if the offset is sane for the use case).
	UFUNCTION(BlueprintCallable, Category = "SiteSync|Geo")
	static bool GeoToLocalOffsetCm(double DeviceLatitude,
	                               double DeviceLongitude,
	                               double TargetLatitude,
	                               double TargetLongitude,
	                               FVector& OutOffsetCm);

	// Node 2.3b — enumerate the addressable "layers" (Static Mesh Components)
	// on a placed BIM actor, for the layer-toggle UI. Returns two parallel
	// arrays: OutLayerNames[i] labels OutComponents[i]. The label is the
	// component's static-mesh asset name (Datasmith derives that from the
	// Rhino OBJECT name — see docs/node-2.3-plan.md: objects MUST be named in
	// Rhino or they arrive as "extrusion_N"). Falls back to the component name
	// if the mesh is null.
	//
	// Works on any actor with Static Mesh Components — a Datasmith Scene Actor
	// (per-layer components) or a single-mesh BP. Against the merged
	// SM_TestBuilding_Merged it returns 1 entry; against the un-merged
	// Datasmith Scene Actor it returns one per source object.
	//
	// Logs every component's name + mesh name + tags at Warning level so the
	// first device run reveals the real naming and we can confirm the layer
	// labels are meaningful before wiring the toggle UI to them.
	UFUNCTION(BlueprintCallable, Category = "SiteSync|BIM")
	static void GetBIMLayers(AActor* BIM,
	                         TArray<FString>& OutLayerNames,
	                         TArray<UStaticMeshComponent*>& OutComponents);

	// Node 2.3c — detect clashes between the BIM's layer components. Each pair
	// of Static Mesh Components is tested for geometric overlap using
	// Oriented-Bounding-Box separating-axis (SAT) — exact for box-shaped
	// meshes (walls/beams/ducts), a tight approximation for the rest, and with
	// NO dependency on whether the imported meshes have collision generated
	// (Datasmith often doesn't). Returns the clash count; OutClashes carries
	// the component refs + layer names + approx center for the highlight UI.
	//
	// ExcludedLayerSubstrings: any pair where BOTH layer names contain one of
	// these substrings is skipped — use it to suppress "trivial" structural
	// adjacencies (e.g. pass ["Structure_"] so Floor∩Wall doesn't count, while
	// HVAC∩Structure_Beam still does). Empty = report everything.
	//
	// Logs every tested pair's overlap result at Warning level for diagnosis.
	UFUNCTION(BlueprintCallable, Category = "SiteSync|BIM")
	static int32 DetectBIMClashes(AActor* BIM,
	                              const TArray<FString>& ExcludedLayerSubstrings,
	                              TArray<FBIMClashPair>& OutClashes);
};
