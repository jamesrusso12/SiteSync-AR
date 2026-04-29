#include "ARMeshBlueprintLibrary.h"
#include "ARBlueprintLibrary.h"
#include "ARTrackable.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"
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

	UE_LOG(LogSiteSyncAR, Verbose, TEXT("UpdateLiDARMeshes: populated %d sections from %d geometries"), SectionCount, MeshGeometries.Num());
	return SectionCount;
}

bool UARMeshBlueprintLibrary::CalculateCutFillVolumes(UProceduralMeshComponent* TerrainMesh,
                                                      AActor* FoundationActor,
                                                      float& OutCutCubicYards,
                                                      float& OutFillCubicYards)
{
	using namespace SiteSyncARVolume;

	OutCutCubicYards = 0.0f;
	OutFillCubicYards = 0.0f;

	if (!TerrainMesh || !FoundationActor)
	{
		UE_LOG(LogSiteSyncAR, Warning, TEXT("CalculateCutFillVolumes: null TerrainMesh=%p FoundationActor=%p"),
		       TerrainMesh, FoundationActor);
		return false;
	}

	// BP_Foundation visually scales SlabMesh.RelativeScale3D (in meters) — actor
	// scale itself stays at (1,1,1). Read SlabMesh's relative scale to get the
	// real placed dimensions. Falls back to actor scale if no SlabMesh found.
	FVector ScaleMeters = FoundationActor->GetActorScale3D();
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
		if (SlabMesh) ScaleMeters = SlabMesh->GetRelativeScale3D();
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
