// Copyright (c) 2026 James Russo and Cole. All rights reserved. SiteSync AR is proprietary software — see LICENSE.

#include "ARMeshBlueprintLibrary.h"
#include "ARBlueprintLibrary.h"
#include "ARTrackable.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Logging/LogMacros.h"
#include "ProceduralMeshComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogSiteSyncAR, Log, All);

namespace SiteSyncARVolume
{
	constexpr double CmCubedPerYardCubed = 91.44 * 91.44 * 91.44;

	// Sutherland-Hodgman clip against half-plane (a*x + b*y + c >= 0).
	static void ClipPolygonByHalfPlane(const TArray<FVector2D>& In,
	                                   double A, double B, double C,
	                                   TArray<FVector2D>& Out)
	{
		Out.Reset();
		const int32 N = In.Num();
		if (N == 0) return;

		auto Eval = [A, B, C](const FVector2D& P) -> double
		{
			return A * P.X + B * P.Y + C;
		};

		for (int32 i = 0; i < N; ++i)
		{
			const FVector2D& Curr = In[i];
			const FVector2D& Prev = In[(i + N - 1) % N];
			const double SC = Eval(Curr);
			const double SP = Eval(Prev);
			const bool InsideC = SC >= 0.0;
			const bool InsideP = SP >= 0.0;

			if (InsideC)
			{
				if (!InsideP)
				{
					const double T = SP / (SP - SC);
					Out.Add(FVector2D(Prev.X + (Curr.X - Prev.X) * T,
					                  Prev.Y + (Curr.Y - Prev.Y) * T));
				}
				Out.Add(Curr);
			}
			else if (InsideP)
			{
				const double T = SP / (SP - SC);
				Out.Add(FVector2D(Prev.X + (Curr.X - Prev.X) * T,
				                  Prev.Y + (Curr.Y - Prev.Y) * T));
			}
		}
	}

	static void ClipPolygonToRect(const TArray<FVector2D>& In,
	                              double XMin, double XMax, double YMin, double YMax,
	                              TArray<FVector2D>& Out)
	{
		TArray<FVector2D> A, B;
		ClipPolygonByHalfPlane(In, 1.0, 0.0, -XMin, A);
		ClipPolygonByHalfPlane(A, -1.0, 0.0, XMax, B);
		ClipPolygonByHalfPlane(B, 0.0, 1.0, -YMin, A);
		ClipPolygonByHalfPlane(A, 0.0, -1.0, YMax, Out);
	}

	static double PolygonArea2D(const TArray<FVector2D>& Verts)
	{
		const int32 N = Verts.Num();
		if (N < 3) return 0.0;
		double Sum = 0.0;
		for (int32 i = 0; i < N; ++i)
		{
			const FVector2D& P = Verts[i];
			const FVector2D& Q = Verts[(i + 1) % N];
			Sum += P.X * Q.Y - Q.X * P.Y;
		}
		return FMath::Abs(Sum) * 0.5;
	}

	static FVector2D PolygonCentroid2D(const TArray<FVector2D>& Verts)
	{
		const int32 N = Verts.Num();
		if (N < 3) return N > 0 ? Verts[0] : FVector2D::ZeroVector;
		double SumX = 0.0, SumY = 0.0, SumA = 0.0;
		for (int32 i = 0; i < N; ++i)
		{
			const FVector2D& P = Verts[i];
			const FVector2D& Q = Verts[(i + 1) % N];
			const double Cross = P.X * Q.Y - Q.X * P.Y;
			SumX += (P.X + Q.X) * Cross;
			SumY += (P.Y + Q.Y) * Cross;
			SumA += Cross;
		}
		if (FMath::Abs(SumA) < 1e-9) return Verts[0];
		const double InvDenom = 1.0 / (3.0 * SumA);
		return FVector2D(SumX * InvDenom, SumY * InvDenom);
	}

	struct FHeightField
	{
		double A;
		double B;
		double C;
	};

	// Vertical triangles project to a line and contribute zero volume — return false to skip.
	static bool ComputeHeightField(const FVector& Pa, const FVector& Pb, const FVector& Pc, FHeightField& Out)
	{
		const FVector E1 = Pb - Pa;
		const FVector E2 = Pc - Pa;
		const FVector N = FVector::CrossProduct(E1, E2);
		if (FMath::Abs(N.Z) < 1e-6) return false;
		Out.A = -N.X / N.Z;
		Out.B = -N.Y / N.Z;
		Out.C = Pa.Z - Out.A * Pa.X - Out.B * Pa.Y;
		return true;
	}
}

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
	UE_LOG(LogSiteSyncAR, Verbose, TEXT("GetAllARMeshGeometries: %d total tracked geometries, %d mesh geometries"), AllGeos.Num(), Result.Num());
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

bool UARMeshBlueprintLibrary::GetDeviceGeoLocation(double& OutLatitude,
                                                   double& OutLongitude,
                                                   double& OutAltitudeMeters,
                                                   double& OutHorizontalAccuracyMeters)
{
	OutLatitude = 0.0;
	OutLongitude = 0.0;
	OutAltitudeMeters = 0.0;
	OutHorizontalAccuracyMeters = 0.0;
	return false;
}
#endif

bool UARMeshBlueprintLibrary::GeoToLocalOffsetCm(double DeviceLatitude,
                                                  double DeviceLongitude,
                                                  double TargetLatitude,
                                                  double TargetLongitude,
                                                  FVector& OutOffsetCm)
{
	OutOffsetCm = FVector::ZeroVector;

	if (!FMath::IsFinite(DeviceLatitude) || !FMath::IsFinite(DeviceLongitude) ||
	    !FMath::IsFinite(TargetLatitude) || !FMath::IsFinite(TargetLongitude))
	{
		UE_LOG(LogSiteSyncAR, Warning,
		       TEXT("GeoToLocalOffsetCm: non-finite input (device=%.7f,%.7f target=%.7f,%.7f) — abort"),
		       DeviceLatitude, DeviceLongitude, TargetLatitude, TargetLongitude);
		return false;
	}

	// WGS84 mean earth radius in meters. Equirectangular projection is correct
	// enough for sub-km site work — we're not crossing tectonic plates.
	constexpr double EarthRadiusM = 6378137.0;
	constexpr double DegToRad = PI / 180.0;

	const double DeltaLatRad = (TargetLatitude - DeviceLatitude) * DegToRad;
	const double DeltaLongRad = (TargetLongitude - DeviceLongitude) * DegToRad;
	const double DeviceLatRad = DeviceLatitude * DegToRad;

	const double MetersNorth = DeltaLatRad * EarthRadiusM;
	const double MetersEast = DeltaLongRad * EarthRadiusM * FMath::Cos(DeviceLatRad);

	// UE world axes under DA_SiteSyncARConfig.WorldAlignment = GravityAndHeading
	// (Node 2.2b): UE +X = true north, UE +Y = east, UE +Z = up.
	// If an axis sign issue surfaces in 2.2c end-to-end testing, the fix is
	// here (sign-flip on X or Y) — NOT in the AR config.
	OutOffsetCm.X = MetersNorth * 100.0;
	OutOffsetCm.Y = MetersEast * 100.0;
	OutOffsetCm.Z = 0.0;

	// Straight-line distance for log readability.
	const double DistanceM = FMath::Sqrt(MetersNorth * MetersNorth + MetersEast * MetersEast);
	// Bearing from device to target, degrees, 0=N CW (matches compass app convention).
	const double BearingDeg = FMath::RadiansToDegrees(FMath::Atan2(MetersEast, MetersNorth));
	const double BearingNorm = BearingDeg < 0.0 ? BearingDeg + 360.0 : BearingDeg;

	UE_LOG(LogSiteSyncAR, Warning,
	       TEXT("GeoToLocalOffsetCm: device=(%.7f,%.7f) target=(%.7f,%.7f) -> north=%.2fm east=%.2fm dist=%.2fm bearing=%.1f° offset_cm=(%.1f,%.1f,%.1f)"),
	       DeviceLatitude, DeviceLongitude, TargetLatitude, TargetLongitude,
	       MetersNorth, MetersEast, DistanceM, BearingNorm,
	       OutOffsetCm.X, OutOffsetCm.Y, OutOffsetCm.Z);

	return true;
}

void UARMeshBlueprintLibrary::GetBIMLayers(AActor* BIM,
                                            TArray<FString>& OutLayerNames,
                                            TArray<UStaticMeshComponent*>& OutComponents)
{
	OutLayerNames.Reset();
	OutComponents.Reset();

	if (!BIM)
	{
		UE_LOG(LogSiteSyncAR, Warning, TEXT("GetBIMLayers: null BIM actor"));
		return;
	}

	TArray<UStaticMeshComponent*> Comps;
	BIM->GetComponents<UStaticMeshComponent>(Comps);

	UE_LOG(LogSiteSyncAR, Warning,
	       TEXT("GetBIMLayers: actor '%s' has %d StaticMeshComponents"),
	       *BIM->GetName(), Comps.Num());

	for (UStaticMeshComponent* C : Comps)
	{
		if (!C)
		{
			continue;
		}

		// Label = COMPONENT name. Datasmith names the component after the source
		// Rhino object, and unlike the mesh-asset name it's always distinct even
		// when several layers reuse one mesh asset (proven by the cube test rig
		// where all 5 share /Engine/BasicShapes/Cube but the components are
		// named Structure_Floor / HVAC_Duct / etc.).
		UStaticMesh* SM = C->GetStaticMesh();
		const FString Label = C->GetName();

		OutComponents.Add(C);
		OutLayerNames.Add(Label);

		// Diagnostic dump — the first device run against a real multi-layer
		// model reveals the actual naming so we can confirm the toggle UI keys
		// on something meaningful (not "extrusion_N").
		FString TagStr;
		for (const FName& T : C->ComponentTags)
		{
			TagStr += T.ToString() + TEXT(" ");
		}
		UE_LOG(LogSiteSyncAR, Warning,
		       TEXT("GetBIMLayers:   comp='%s' mesh='%s' tags=[%s] visible=%d"),
		       *C->GetName(),
		       SM ? *SM->GetName() : TEXT("(none)"),
		       *TagStr,
		       C->IsVisible() ? 1 : 0);
	}

	UE_LOG(LogSiteSyncAR, Warning, TEXT("GetBIMLayers: returning %d layers"), OutLayerNames.Num());
}

namespace SiteSyncClash
{
	// Oriented bounding box: center, three orthonormal axes, half-extents.
	struct FOBB
	{
		FVector Center;
		FVector Axis[3];
		FVector Half;
	};

	static bool BuildOBB(UStaticMeshComponent* Comp, FOBB& Out)
	{
		UStaticMesh* SM = Comp->GetStaticMesh();
		if (!SM) return false;

		const FBox Local = SM->GetBoundingBox();
		if (!Local.IsValid) return false;

		const FTransform Xf = Comp->GetComponentTransform();
		Out.Center = Xf.TransformPosition(Local.GetCenter());

		const FVector Scale = Xf.GetScale3D();
		const FVector LocalExtent = Local.GetExtent();
		Out.Half = FVector(LocalExtent.X * FMath::Abs(Scale.X),
		                   LocalExtent.Y * FMath::Abs(Scale.Y),
		                   LocalExtent.Z * FMath::Abs(Scale.Z));

		const FQuat Q = Xf.GetRotation();
		Out.Axis[0] = Q.GetAxisX();
		Out.Axis[1] = Q.GetAxisY();
		Out.Axis[2] = Q.GetAxisZ();
		return true;
	}

	// Separating Axis Theorem for two OBBs. Returns true if they overlap.
	static bool OBBOverlap(const FOBB& A, const FOBB& B)
	{
		const FVector T = B.Center - A.Center;
		// Rotation matrix R[i][j] = A.Axis[i] · B.Axis[j], plus abs with epsilon
		// to guard against parallel-edge numerical issues.
		double R[3][3];
		double AbsR[3][3];
		constexpr double Eps = 1e-6;
		for (int32 i = 0; i < 3; ++i)
		{
			for (int32 j = 0; j < 3; ++j)
			{
				R[i][j] = FVector::DotProduct(A.Axis[i], B.Axis[j]);
				AbsR[i][j] = FMath::Abs(R[i][j]) + Eps;
			}
		}

		// T projected onto A's axes.
		double Ta[3];
		for (int32 i = 0; i < 3; ++i) Ta[i] = FVector::DotProduct(T, A.Axis[i]);

		const double Ae[3] = { A.Half.X, A.Half.Y, A.Half.Z };
		const double Be[3] = { B.Half.X, B.Half.Y, B.Half.Z };

		// 3 axes of A.
		for (int32 i = 0; i < 3; ++i)
		{
			const double ra = Ae[i];
			const double rb = Be[0] * AbsR[i][0] + Be[1] * AbsR[i][1] + Be[2] * AbsR[i][2];
			if (FMath::Abs(Ta[i]) > ra + rb) return false;
		}
		// 3 axes of B.
		for (int32 j = 0; j < 3; ++j)
		{
			const double ra = Ae[0] * AbsR[0][j] + Ae[1] * AbsR[1][j] + Ae[2] * AbsR[2][j];
			const double rb = Be[j];
			const double t = FMath::Abs(Ta[0] * R[0][j] + Ta[1] * R[1][j] + Ta[2] * R[2][j]);
			if (t > ra + rb) return false;
		}
		// 9 cross-product axes.
		const int32 N[3][2] = { {1,2}, {2,0}, {0,1} };
		for (int32 i = 0; i < 3; ++i)
		{
			for (int32 j = 0; j < 3; ++j)
			{
				const double ra = Ae[N[i][0]] * AbsR[N[i][1]][j] + Ae[N[i][1]] * AbsR[N[i][0]][j];
				const double rb = Be[N[j][0]] * AbsR[i][N[j][1]] + Be[N[j][1]] * AbsR[i][N[j][0]];
				const double t = FMath::Abs(Ta[N[i][1]] * R[N[i][0]][j] - Ta[N[i][0]] * R[N[i][1]][j]);
				if (t > ra + rb) return false;
			}
		}
		return true; // no separating axis found
	}
}

int32 UARMeshBlueprintLibrary::DetectBIMClashes(AActor* BIM,
                                                 const TArray<FString>& ExcludedLayerSubstrings,
                                                 TArray<FBIMClashPair>& OutClashes)
{
	using namespace SiteSyncClash;
	OutClashes.Reset();

	if (!BIM)
	{
		UE_LOG(LogSiteSyncAR, Warning, TEXT("DetectBIMClashes: null BIM actor"));
		return 0;
	}

	TArray<UStaticMeshComponent*> Comps;
	BIM->GetComponents<UStaticMeshComponent>(Comps);

	// Pre-build OBBs + labels once.
	TArray<FOBB> Boxes;
	TArray<FString> Labels;
	TArray<UStaticMeshComponent*> Valid;
	for (UStaticMeshComponent* C : Comps)
	{
		if (!C) continue;
		FOBB Box;
		if (!BuildOBB(C, Box)) continue;
		Valid.Add(C);
		Boxes.Add(Box);
		Labels.Add(C->GetName()); // component name — see GetBIMLayers rationale
	}

	UE_LOG(LogSiteSyncAR, Warning,
	       TEXT("DetectBIMClashes: actor '%s', %d valid components, %d exclusion substrings"),
	       *BIM->GetName(), Valid.Num(), ExcludedLayerSubstrings.Num());

	auto BothExcluded = [&ExcludedLayerSubstrings](const FString& A, const FString& B) -> bool
	{
		for (const FString& Sub : ExcludedLayerSubstrings)
		{
			if (A.Contains(Sub) && B.Contains(Sub)) return true;
		}
		return false;
	};

	for (int32 i = 0; i < Valid.Num(); ++i)
	{
		for (int32 j = i + 1; j < Valid.Num(); ++j)
		{
			if (BothExcluded(Labels[i], Labels[j]))
			{
				UE_LOG(LogSiteSyncAR, Warning,
				       TEXT("DetectBIMClashes:   skip (excluded pair) '%s' x '%s'"), *Labels[i], *Labels[j]);
				continue;
			}

			const bool bClash = OBBOverlap(Boxes[i], Boxes[j]);
			UE_LOG(LogSiteSyncAR, Warning,
			       TEXT("DetectBIMClashes:   '%s' x '%s' -> %s"),
			       *Labels[i], *Labels[j], bClash ? TEXT("CLASH") : TEXT("clear"));

			if (bClash)
			{
				FBIMClashPair Pair;
				Pair.LayerA = Labels[i];
				Pair.LayerB = Labels[j];
				Pair.ComponentA = Valid[i];
				Pair.ComponentB = Valid[j];
				Pair.ApproxCenter = (Boxes[i].Center + Boxes[j].Center) * 0.5;
				OutClashes.Add(Pair);
			}
		}
	}

	UE_LOG(LogSiteSyncAR, Warning, TEXT("DetectBIMClashes: %d clashes detected"), OutClashes.Num());
	return OutClashes.Num();
}

int32 UARMeshBlueprintLibrary::DetectBIMClashesAll(AActor* BIM,
                                                    TArray<FBIMClashPair>& OutClashes)
{
	static const TArray<FString> NoExclusions;
	return DetectBIMClashes(BIM, NoExclusions, OutClashes);
}

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

	UE_LOG(LogSiteSyncAR, Verbose, TEXT("UpdateLiDARMeshes: populated %d sections from %d geometries"), SectionCount, MeshGeometries.Num());
	return SectionCount;
}

bool UARMeshBlueprintLibrary::CalculateCutFillVolumes(UProceduralMeshComponent* TerrainMesh,
                                                      AActor* FoundationActor,
                                                      float& OutCutCubicYards,
                                                      float& OutFillCubicYards)
{
	UE_LOG(LogSiteSyncAR, Warning, TEXT("CalculateCutFillVolumes: invoked"));
	using namespace SiteSyncARVolume;

	OutCutCubicYards = 0.0f;
	OutFillCubicYards = 0.0f;

	if (!TerrainMesh || !FoundationActor)
	{
		UE_LOG(LogSiteSyncAR, Warning, TEXT("CalculateCutFillVolumes: null TerrainMesh=%p FoundationActor=%p"),
		       TerrainMesh, FoundationActor);
		return false;
	}

	// BP_Foundation may set the slab dimensions as either ACTOR scale or
	// SlabMesh component relative scale (or both, depending on InitFromCorners
	// wiring). Effective world scale of the slab visual = actor scale ×
	// component relative scale, so multiply both sources to be wiring-agnostic.
	const FVector ActorScale = FoundationActor->GetActorScale3D();
	FVector ScaleMeters = ActorScale;
	UStaticMeshComponent* SlabMesh = nullptr;
	{
		TArray<UStaticMeshComponent*> SMs;
		FoundationActor->GetComponents<UStaticMeshComponent>(SMs);
		for (UStaticMeshComponent* SM : SMs)
		{
			if (SM && SM->GetName().Contains(TEXT("SlabMesh")))
			{
				SlabMesh = SM;
				break;
			}
		}
		if (!SlabMesh && SMs.Num() > 0) SlabMesh = SMs[0];
		if (SlabMesh) ScaleMeters = ActorScale * SlabMesh->GetRelativeScale3D();
	}

	const float RawLengthCm = static_cast<float>(ScaleMeters.X) * 100.0f;
	const float RawWidthCm = static_cast<float>(ScaleMeters.Y) * 100.0f;
	const float RawThicknessCm = static_cast<float>(ScaleMeters.Z) * 100.0f;

	UE_LOG(LogSiteSyncAR, Log,
	       TEXT("CalculateCutFillVolumes: enter slabComp=%s scale=(%.3f,%.3f,%.3f)m raw_dims=(%.1f,%.1f,%.1f)cm sections=%d"),
	       SlabMesh ? *SlabMesh->GetName() : TEXT("(actor)"),
	       ScaleMeters.X, ScaleMeters.Y, ScaleMeters.Z,
	       RawLengthCm, RawWidthCm, RawThicknessCm,
	       TerrainMesh->GetNumSections());

	if (RawLengthCm <= 0.0f || RawWidthCm <= 0.0f || RawThicknessCm <= 0.0f) return false;

	// Clamp slab dimensions to sane ranges so a wonky tap (e.g., A and B on
	// different LiDAR plane heights producing a 2m thick slab) doesn't break
	// the math. Real foundations: length 50cm-50m, width 50cm-50m, thickness
	// 5cm-50cm.
	const float SlabLengthCm = FMath::Clamp(RawLengthCm, 50.0f, 5000.0f);
	const float SlabWidthCm = FMath::Clamp(RawWidthCm, 50.0f, 5000.0f);
	const float SlabThicknessCm = FMath::Clamp(RawThicknessCm, 5.0f, 50.0f);

	UE_LOG(LogSiteSyncAR, Log,
	       TEXT("CalculateCutFillVolumes: slab dims raw→clamped L=%.1f→%.1fcm W=%.1f→%.1fcm T=%.1f→%.1fcm"),
	       RawLengthCm, SlabLengthCm, RawWidthCm, SlabWidthCm, RawThicknessCm, SlabThicknessCm);

	// Plane sits at slab BOTTOM face. Slab actor pivot is the slab center, so subgrade in
	// slab-local space is z = -SlabThicknessCm/2.
	const double PlaneZ = -static_cast<double>(SlabThicknessCm) * 0.5;

	const double XMin = -static_cast<double>(SlabLengthCm) * 0.5;
	const double XMax = +static_cast<double>(SlabLengthCm) * 0.5;
	const double YMin = -static_cast<double>(SlabWidthCm) * 0.5;
	const double YMax = +static_cast<double>(SlabWidthCm) * 0.5;

	const FTransform WorldToLocal = FoundationActor->GetActorTransform().Inverse();
	const FTransform CompToWorld = TerrainMesh->GetComponentTransform();

	double TotalCutCm3 = 0.0;
	double TotalFillCm3 = 0.0;
	int32 TrianglesProcessed = 0;
	int32 TrianglesSkipped = 0;

	// Reuse buffers across triangle iterations — at 28 sections × ~5000 tris
	// × 5 TArray allocs/tri this used to do ~700k heap allocations per 1Hz
	// call, which is the most plausible cause for iOS jetsam. Reserve(8) is
	// the typical worst-case Sutherland-Hodgman rect-clipped triangle vertex
	// count.
	TArray<FVector2D> Tri;        Tri.Reserve(8);
	TArray<FVector2D> InsideFootprint; InsideFootprint.Reserve(8);
	TArray<FVector2D> AbovePoly;  AbovePoly.Reserve(8);
	TArray<FVector2D> BelowPoly;  BelowPoly.Reserve(8);

	// Hard cap on triangles per call as an emergency brake against runaway
	// iteration on a corrupt mesh. 250k is generous (28 sections × ~9k tris).
	constexpr int32 MaxTrianglesPerCall = 250000;

	const int32 NumSections = TerrainMesh->GetNumSections();
	for (int32 SectionIdx = 0; SectionIdx < NumSections; ++SectionIdx)
	{
		const FProcMeshSection* Section = TerrainMesh->GetProcMeshSection(SectionIdx);
		if (!Section) continue;

		const TArray<FProcMeshVertex>& Verts = Section->ProcVertexBuffer;
		const TArray<uint32>& Indices = Section->ProcIndexBuffer;
		const int32 NumVerts = Verts.Num();

		for (int32 i = 0; i + 2 < Indices.Num(); i += 3)
		{
			if (TrianglesProcessed + TrianglesSkipped >= MaxTrianglesPerCall)
			{
				UE_LOG(LogSiteSyncAR, Warning,
				       TEXT("CalculateCutFillVolumes: hit MaxTrianglesPerCall cap (%d) — bailing"),
				       MaxTrianglesPerCall);
				goto Done;
			}

			const uint32 Ia = Indices[i + 0];
			const uint32 Ib = Indices[i + 1];
			const uint32 Ic = Indices[i + 2];
			if (Ia >= (uint32)NumVerts || Ib >= (uint32)NumVerts || Ic >= (uint32)NumVerts)
			{
				TrianglesSkipped++;
				continue;
			}

			const FVector PaW = CompToWorld.TransformPosition(Verts[Ia].Position);
			const FVector PbW = CompToWorld.TransformPosition(Verts[Ib].Position);
			const FVector PcW = CompToWorld.TransformPosition(Verts[Ic].Position);

			const FVector Pa = WorldToLocal.TransformPosition(PaW);
			const FVector Pb = WorldToLocal.TransformPosition(PbW);
			const FVector Pc = WorldToLocal.TransformPosition(PcW);

			// Skip triangles with NaN/Inf positions before the math touches them.
			if (!FMath::IsFinite(Pa.X) || !FMath::IsFinite(Pa.Y) || !FMath::IsFinite(Pa.Z) ||
			    !FMath::IsFinite(Pb.X) || !FMath::IsFinite(Pb.Y) || !FMath::IsFinite(Pb.Z) ||
			    !FMath::IsFinite(Pc.X) || !FMath::IsFinite(Pc.Y) || !FMath::IsFinite(Pc.Z))
			{
				TrianglesSkipped++;
				continue;
			}

			FHeightField H;
			if (!ComputeHeightField(Pa, Pb, Pc, H)) { TrianglesSkipped++; continue; }

			Tri.Reset();
			Tri.Add(FVector2D(Pa.X, Pa.Y));
			Tri.Add(FVector2D(Pb.X, Pb.Y));
			Tri.Add(FVector2D(Pc.X, Pc.Y));

			InsideFootprint.Reset();
			ClipPolygonToRect(Tri, XMin, XMax, YMin, YMax, InsideFootprint);
			if (InsideFootprint.Num() < 3) { TrianglesSkipped++; continue; }

			// d(x,y) = h(x,y) - PlaneZ. Linear in (x,y), so can be split with the same
			// half-plane primitive used for the rect clip.
			const double DOffset = H.C - PlaneZ;

			AbovePoly.Reset();
			ClipPolygonByHalfPlane(InsideFootprint, H.A, H.B, DOffset, AbovePoly);

			BelowPoly.Reset();
			ClipPolygonByHalfPlane(InsideFootprint, -H.A, -H.B, -DOffset, BelowPoly);

			auto EvalD = [&H, DOffset](const FVector2D& P) -> double
			{
				return H.A * P.X + H.B * P.Y + DOffset;
			};

			if (AbovePoly.Num() >= 3)
			{
				const double Area = PolygonArea2D(AbovePoly);
				if (Area > 1e-3)
				{
					const FVector2D Cen = PolygonCentroid2D(AbovePoly);
					TotalCutCm3 += Area * FMath::Max(EvalD(Cen), 0.0);
				}
			}
			if (BelowPoly.Num() >= 3)
			{
				const double Area = PolygonArea2D(BelowPoly);
				if (Area > 1e-3)
				{
					const FVector2D Cen = PolygonCentroid2D(BelowPoly);
					TotalFillCm3 += Area * FMath::Max(-EvalD(Cen), 0.0);
				}
			}

			TrianglesProcessed++;
		}
	}

Done:
	OutCutCubicYards = static_cast<float>(TotalCutCm3 / CmCubedPerYardCubed);
	OutFillCubicYards = static_cast<float>(TotalFillCm3 / CmCubedPerYardCubed);

	UE_LOG(LogSiteSyncAR, Log,
	       TEXT("CalculateCutFillVolumes: processed %d triangles, skipped %d → cut %.3f yd³, fill %.3f yd³"),
	       TrianglesProcessed, TrianglesSkipped, OutCutCubicYards, OutFillCubicYards);
	return true;
}

bool UARMeshBlueprintLibrary::InitFoundationFromCorners(AActor* FoundationActor,
                                                         FVector CornerA,
                                                         FVector CornerB,
                                                         float WidthCm,
                                                         float ThicknessCm)
{
	if (!FoundationActor)
	{
		UE_LOG(LogSiteSyncAR, Warning, TEXT("InitFoundationFromCorners: null FoundationActor"));
		return false;
	}

	const float ClampedWidthCm = FMath::Clamp(WidthCm, 50.0f, 5000.0f);
	const float ClampedThicknessCm = FMath::Clamp(ThicknessCm, 5.0f, 50.0f);

	const FVector Center = (CornerA + CornerB) * 0.5;
	const FVector Delta = CornerB - CornerA;

	const float DeltaXY = FMath::Sqrt(static_cast<float>(Delta.X * Delta.X + Delta.Y * Delta.Y));
	const float LengthCm = FMath::Clamp(DeltaXY, 50.0f, 5000.0f);
	const float YawDeg = FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X));

	// Slab visual scale is interpreted in METERS (engine cube is 100cm at scale 1.0,
	// so scale=N produces an N-meter side). CalculateCutFillVolumes reads back the
	// same convention via ActorScale × SlabMesh.RelativeScale3D.
	const FVector ScaleMeters(LengthCm / 100.0f,
	                          ClampedWidthCm / 100.0f,
	                          ClampedThicknessCm / 100.0f);

	FoundationActor->SetActorLocation(Center);
	FoundationActor->SetActorRotation(FRotator(0.0f, YawDeg, 0.0f));
	FoundationActor->SetActorScale3D(ScaleMeters);

	UE_LOG(LogSiteSyncAR, Warning,
	       TEXT("InitFoundationFromCorners: A=(%.1f,%.1f,%.1f) B=(%.1f,%.1f,%.1f) center=(%.1f,%.1f,%.1f) yaw=%.1f° L=%.1f→%.1fcm W=%.1f→%.1fcm T=%.1f→%.1fcm scale_m=(%.3f,%.3f,%.3f)"),
	       CornerA.X, CornerA.Y, CornerA.Z,
	       CornerB.X, CornerB.Y, CornerB.Z,
	       Center.X, Center.Y, Center.Z,
	       YawDeg,
	       DeltaXY, LengthCm, WidthCm, ClampedWidthCm, ThicknessCm, ClampedThicknessCm,
	       ScaleMeters.X, ScaleMeters.Y, ScaleMeters.Z);

	return true;
}

bool UARMeshBlueprintLibrary::RaycastTerrainFromScreen(APlayerController* PlayerController,
                                                       UProceduralMeshComponent* TerrainMesh,
                                                       FVector2D ScreenPosition,
                                                       FVector& OutHitLocation,
                                                       FVector& OutHitNormal)
{
	OutHitLocation = FVector::ZeroVector;
	OutHitNormal = FVector::UpVector;

	if (!PlayerController || !TerrainMesh)
	{
		UE_LOG(LogSiteSyncAR, Warning,
		       TEXT("RaycastTerrainFromScreen: null PC=%p TerrainMesh=%p"),
		       PlayerController, TerrainMesh);
		return false;
	}

	FVector RayOrigin, RayDirection;
	if (!UGameplayStatics::DeprojectScreenToWorld(PlayerController, ScreenPosition, RayOrigin, RayDirection))
	{
		UE_LOG(LogSiteSyncAR, Verbose, TEXT("RaycastTerrainFromScreen: deproject failed at (%.0f,%.0f)"),
		       ScreenPosition.X, ScreenPosition.Y);
		return false;
	}

	const FTransform CompToWorld = TerrainMesh->GetComponentTransform();

	float NearestHitT = TNumericLimits<float>::Max();
	FVector NearestHitLoc = FVector::ZeroVector;
	FVector NearestHitNormal = FVector::UpVector;
	bool bAnyHit = false;

	constexpr float MaxRayLength = 50000.0f; // 500m — way more than any AR scene
	constexpr float MinDet = 1e-6f;

	int32 TrianglesTested = 0;

	const int32 NumSections = TerrainMesh->GetNumSections();
	for (int32 SectionIdx = 0; SectionIdx < NumSections; ++SectionIdx)
	{
		const FProcMeshSection* Section = TerrainMesh->GetProcMeshSection(SectionIdx);
		if (!Section) continue;

		const TArray<FProcMeshVertex>& Verts = Section->ProcVertexBuffer;
		const TArray<uint32>& Indices = Section->ProcIndexBuffer;
		const int32 NumVerts = Verts.Num();

		for (int32 i = 0; i + 2 < Indices.Num(); i += 3)
		{
			const uint32 Ia = Indices[i + 0];
			const uint32 Ib = Indices[i + 1];
			const uint32 Ic = Indices[i + 2];
			if (Ia >= (uint32)NumVerts || Ib >= (uint32)NumVerts || Ic >= (uint32)NumVerts) continue;

			const FVector V0 = CompToWorld.TransformPosition(Verts[Ia].Position);
			const FVector V1 = CompToWorld.TransformPosition(Verts[Ib].Position);
			const FVector V2 = CompToWorld.TransformPosition(Verts[Ic].Position);

			// Möller-Trumbore ray-triangle intersection
			const FVector E1 = V1 - V0;
			const FVector E2 = V2 - V0;
			const FVector P = FVector::CrossProduct(RayDirection, E2);
			const float Det = static_cast<float>(FVector::DotProduct(E1, P));
			if (FMath::Abs(Det) < MinDet) continue;

			const float InvDet = 1.0f / Det;
			const FVector T = RayOrigin - V0;
			const float U = static_cast<float>(FVector::DotProduct(T, P)) * InvDet;
			if (U < 0.0f || U > 1.0f) continue;

			const FVector Q = FVector::CrossProduct(T, E1);
			const float Vv = static_cast<float>(FVector::DotProduct(RayDirection, Q)) * InvDet;
			if (Vv < 0.0f || U + Vv > 1.0f) continue;

			const float HitT = static_cast<float>(FVector::DotProduct(E2, Q)) * InvDet;
			if (HitT < 0.0f || HitT > MaxRayLength) continue;

			TrianglesTested++;

			if (HitT < NearestHitT)
			{
				NearestHitT = HitT;
				NearestHitLoc = RayOrigin + RayDirection * HitT;
				FVector N = FVector::CrossProduct(E1, E2).GetSafeNormal();
				// Flip normal toward the ray origin so callers always get a "face up"
				// oriented normal regardless of triangle winding.
				if (FVector::DotProduct(N, RayDirection) > 0.0f) N = -N;
				NearestHitNormal = N;
				bAnyHit = true;
			}
		}
	}

	if (bAnyHit)
	{
		OutHitLocation = NearestHitLoc;
		OutHitNormal = NearestHitNormal;
		UE_LOG(LogSiteSyncAR, Log,
		       TEXT("RaycastTerrainFromScreen: hit at (%.1f,%.1f,%.1f) t=%.1f screen=(%.0f,%.0f) candidates=%d"),
		       NearestHitLoc.X, NearestHitLoc.Y, NearestHitLoc.Z, NearestHitT,
		       ScreenPosition.X, ScreenPosition.Y, TrianglesTested);
		return true;
	}

	UE_LOG(LogSiteSyncAR, Verbose,
	       TEXT("RaycastTerrainFromScreen: no triangle hit screen=(%.0f,%.0f) sections=%d"),
	       ScreenPosition.X, ScreenPosition.Y, NumSections);
	return false;
}

bool UARMeshBlueprintLibrary::PlaceBIMByCornerForward(AActor* BIMActor,
                                                     FVector CornerWorld,
                                                     FVector ForwardWorld,
                                                     float LengthCm,
                                                     float WidthCm,
                                                     float HeightCm)
{
	if (!BIMActor)
	{
		UE_LOG(LogSiteSyncAR, Warning, TEXT("PlaceBIMByCornerForward: null BIMActor"));
		return false;
	}

	const FVector Delta = ForwardWorld - CornerWorld;
	const float DeltaXY = FMath::Sqrt(static_cast<float>(Delta.X * Delta.X + Delta.Y * Delta.Y));
	if (DeltaXY < 1.0f)
	{
		UE_LOG(LogSiteSyncAR, Warning,
		       TEXT("PlaceBIMByCornerForward: Corner→Forward XY delta %.2fcm too small, abort"), DeltaXY);
		return false;
	}

	// Clamp lower bound is 10cm = actor scale 0.1× → supports "tabletop / dollhouse" Model Scale
	// for indoor demos in addition to the 1:1 Site Scale production use case.
	const float ClampedLength = FMath::Clamp(LengthCm, 10.0f, 50000.0f);
	const float ClampedWidth = FMath::Clamp(WidthCm, 10.0f, 50000.0f);
	const float ClampedHeight = FMath::Clamp(HeightCm, 10.0f, 30000.0f);

	const float YawDeg = FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X));

	const FVector ScaleMeters(ClampedLength / 100.0f,
	                          ClampedWidth / 100.0f,
	                          ClampedHeight / 100.0f);

	BIMActor->SetActorLocation(CornerWorld);
	BIMActor->SetActorRotation(FRotator(0.0f, YawDeg, 0.0f));
	BIMActor->SetActorScale3D(ScaleMeters);

	UE_LOG(LogSiteSyncAR, Warning,
	       TEXT("PlaceBIMByCornerForward: Corner=(%.1f,%.1f,%.1f) Forward=(%.1f,%.1f,%.1f) yaw=%.1f° L=%.1f→%.1fcm W=%.1f→%.1fcm H=%.1f→%.1fcm scale_m=(%.3f,%.3f,%.3f)"),
	       CornerWorld.X, CornerWorld.Y, CornerWorld.Z,
	       ForwardWorld.X, ForwardWorld.Y, ForwardWorld.Z,
	       YawDeg,
	       LengthCm, ClampedLength, WidthCm, ClampedWidth, HeightCm, ClampedHeight,
	       ScaleMeters.X, ScaleMeters.Y, ScaleMeters.Z);

	return true;
}
