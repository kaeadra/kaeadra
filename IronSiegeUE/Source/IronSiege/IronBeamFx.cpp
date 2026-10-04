#include "IronBeamFx.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

UIronBeamFx::UIronBeamFx()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UIronBeamFx::Init(int32 PoolSize)
{
	UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UMaterialInterface* Soft = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/IronSiege/City/Materials/MI_FX_FlameSoft.MI_FX_FlameSoft"));
	AActor* Owner = GetOwner();
	if (!Cylinder || !Soft || !Owner)
	{
		return;
	}
	for (int32 i = 0; i < PoolSize; ++i)
	{
		UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(Owner);
		Mesh->SetStaticMesh(Cylinder);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCastShadow(false);
		Mesh->SetUsingAbsoluteLocation(true);
		Mesh->SetUsingAbsoluteRotation(true);
		Mesh->SetUsingAbsoluteScale(true);
		Mesh->SetupAttachment(this);
		Mesh->SetVisibility(false);
		Mesh->RegisterComponent();
		UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Soft, this);
		Mesh->SetMaterial(0, Material);
		Meshes.Add(Mesh);
		Materials.Add(Material);
		Segments.AddDefaulted();
	}
}

void UIronBeamFx::DrawSegment(const FVector& A, const FVector& B, float ThicknessCm, const FLinearColor& Color, float Life)
{
	if (Meshes.Num() == 0)
	{
		return;
	}
	const int32 Index = Next;
	Next = (Next + 1) % Meshes.Num();
	const FVector Delta = B - A;
	const float Length = Delta.Size();
	if (Length < 1.f)
	{
		return;
	}
	// The engine cylinder is 100 cm tall along Z, centred on its origin.
	UStaticMeshComponent* Mesh = Meshes[Index];
	Mesh->SetWorldLocationAndRotation((A + B) * 0.5f, FRotationMatrix::MakeFromZ(Delta / Length).Rotator());
	Mesh->SetWorldScale3D(FVector(ThicknessCm / 100.f, ThicknessCm / 100.f, Length / 100.f));
	Mesh->SetVisibility(true);
	Materials[Index]->SetVectorParameterValue(TEXT("Color"), Color);
	Materials[Index]->SetScalarParameterValue(TEXT("Opacity"), 1.f);
	Segments[Index] = { 0.f, FMath::Max(Life, 0.01f), Color };
}

void UIronBeamFx::DrawBolt(const FVector& A, const FVector& B, int32 Kinks, float JitterCm, const FLinearColor& Color, float Life)
{
	const FVector Axis = (B - A).GetSafeNormal();
	const FVector Side = FVector::CrossProduct(Axis, FVector::UpVector).GetSafeNormal();
	const FVector Up = FVector::CrossProduct(Side, Axis);
	FVector Prev = A;
	const int32 Steps = FMath::Max(Kinks, 1);
	for (int32 i = 1; i <= Steps; ++i)
	{
		const float T = float(i) / Steps;
		FVector Point = FMath::Lerp(A, B, T);
		if (i < Steps)
		{
			Point += Side * FMath::FRandRange(-JitterCm, JitterCm) + Up * FMath::FRandRange(-JitterCm, JitterCm);
		}
		DrawSegment(Prev, Point, 16.f, Color, Life);
		Prev = Point;
	}
}

void UIronBeamFx::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	for (int32 i = 0; i < Segments.Num(); ++i)
	{
		FSegment& S = Segments[i];
		if (S.Age < 0.f)
		{
			continue;
		}
		S.Age += DeltaTime;
		const float Alpha = 1.f - S.Age / S.Life;
		if (Alpha <= 0.f)
		{
			S.Age = -1.f;
			Meshes[i]->SetVisibility(false);
			continue;
		}
		Materials[i]->SetScalarParameterValue(TEXT("Opacity"), Alpha * Alpha);
	}
}
