#include "IronPuffEmitter.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"

namespace
{
// The smoke fills about two thirds of its cell on the sheet, so a card is this much wider than the
// sphere it stands in for.
constexpr float CardScale = 1.6f;
// Of the sheet's 64 frames: how far a puff billows through them over its life.
constexpr float FramesPerLife = 22.f;
constexpr float SheetFrames = 64.f;
}

UIronPuffEmitter::UIronPuffEmitter()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UIronPuffEmitter::Init(UMaterialInterface* Material, int32 PoolSize)
{
	AActor* Owner = GetOwner();
	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (!Owner || !Sphere || Meshes.Num() > 0)
	{
		return;
	}
	// Cards once the smoke sheet's material is imported; the soft spheres until then.
	UStaticMesh* Shape = Sphere;
	if (FPackageName::DoesPackageExist(TEXT("/Game/IronSiege/City/Materials/M_FX_Puff")))
	{
		UMaterialInterface* Puff = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/IronSiege/City/Materials/M_FX_Puff.M_FX_Puff"));
		UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
		if (Puff && Plane)
		{
			Shape = Plane;
			Material = Puff;
			bCards = true;
		}
	}
	Puffs.SetNum(PoolSize);
	for (int32 i = 0; i < PoolSize; ++i)
	{
		UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(Owner);
		Mesh->SetStaticMesh(Shape);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCastShadow(false);
		Mesh->SetVisibility(false);
		Mesh->SetupAttachment(this);
		Mesh->RegisterComponent();
		Mesh->SetAbsolute(true, true, true); // Puffs stay where they were emitted, not on the car.
		Meshes.Add(Mesh);
		Materials.Add(Material ? Mesh->CreateDynamicMaterialInstance(0, Material) : nullptr);
	}
}

void UIronPuffEmitter::Emit(const FVector& WorldLocation, const FVector& Velocity, const FIronPuffStyle& Style, float SizeScale)
{
	if (Meshes.Num() == 0)
	{
		return;
	}
	FPuff& Puff = Puffs[Next];
	Puff.Age = 0.f;
	Puff.Velocity = Velocity + Style.Drift;
	Puff.SizeScale = SizeScale;
	Puff.Style = Style;
	if (bCards)
	{
		Puff.Roll = FMath::FRandRange(-40.f, 40.f);
		Puff.Flip = FMath::RandBool() ? 1.f : -1.f;
		Puff.Frame = FMath::FRandRange(0.f, SheetFrames - 1.f - FramesPerLife);
		UpdateFacing();
	}
	UStaticMeshComponent* Mesh = Meshes[Next];
	Mesh->SetWorldLocation(WorldLocation);
	Place(Next, Style.StartScale * SizeScale);
	Mesh->SetVisibility(true);
	if (UMaterialInstanceDynamic* Material = Materials[Next])
	{
		Material->SetVectorParameterValue(TEXT("Color"), Style.StartColor);
		Material->SetScalarParameterValue(TEXT("Opacity"), Style.bSwell ? 0.f : Style.PeakOpacity);
		if (bCards)
		{
			Material->SetScalarParameterValue(TEXT("Frame"), Puff.Frame);
		}
	}
	Next = (Next + 1) % Meshes.Num();
}

void UIronPuffEmitter::UpdateFacing()
{
	if (const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		// The card's face (its Z) toward the camera and its X along the screen's right, which stands
		// the sheet upright on screen.
		const FRotationMatrix View(Camera->GetCameraRotation());
		Facing = FRotationMatrix::MakeFromZX(-View.GetUnitAxis(EAxis::X), View.GetUnitAxis(EAxis::Y)).ToQuat();
	}
}

void UIronPuffEmitter::Place(int32 Index, float Scale)
{
	UStaticMeshComponent* Mesh = Meshes[Index];
	// A puff keeps its own material whatever its owner does to its meshes (a destroyed car chars
	// every static mesh it has, which turned its smoke into black slabs).
	if (Materials[Index] && Mesh->GetMaterial(0) != Materials[Index])
	{
		Mesh->SetMaterial(0, Materials[Index]);
	}
	if (!bCards)
	{
		Mesh->SetWorldScale3D(FVector(Scale));
		return;
	}
	const FPuff& Puff = Puffs[Index];
	Mesh->SetWorldScale3D(FVector(Scale * CardScale * Puff.Flip, Scale * CardScale, 1.f));
	Mesh->SetWorldRotation(Facing * FQuat(FVector::UpVector, FMath::DegreesToRadians(Puff.Roll)));
}

void UIronPuffEmitter::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (bCards)
	{
		UpdateFacing();
	}
	for (int32 i = 0; i < Puffs.Num(); ++i)
	{
		FPuff& Puff = Puffs[i];
		if (Puff.Age < 0.f)
		{
			continue;
		}
		Puff.Age += DeltaTime;
		const FIronPuffStyle& S = Puff.Style;
		if (Puff.Age >= S.Life)
		{
			Puff.Age = -1.f;
			Meshes[i]->SetVisibility(false);
			continue;
		}
		const float Alpha = Puff.Age / S.Life;
		Meshes[i]->AddWorldOffset(Puff.Velocity * DeltaTime);
		Place(i, FMath::Lerp(S.StartScale, S.EndScale, Alpha) * Puff.SizeScale);
		if (UMaterialInstanceDynamic* Material = Materials[i])
		{
			const float Opacity = S.bSwell ? FMath::Sin(Alpha * PI) * S.PeakOpacity : (1.f - Alpha) * S.PeakOpacity;
			Material->SetScalarParameterValue(TEXT("Opacity"), Opacity);
			if (bCards)
			{
				Material->SetScalarParameterValue(TEXT("Frame"), Puff.Frame + Alpha * FramesPerLife);
			}
			if (S.StartColor != S.EndColor)
			{
				Material->SetVectorParameterValue(TEXT("Color"), FMath::Lerp(S.StartColor, S.EndColor, Alpha));
			}
		}
	}
}
