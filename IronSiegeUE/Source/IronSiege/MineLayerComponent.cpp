#include "MineLayerComponent.h"
#include "IronMine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

void UMineLayerComponent::DoFire(const FVector& Origin, const FVector& AimDirection, const IronWeapons::Spec& Spec)
{
	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!World || !Owner || !bUnlocked)
	{
		return;
	}
	// Behind the car and just above the road, so it is not left hanging in mid-air on a slope.
	const FVector Drop = Owner->GetActorLocation() - Owner->GetActorForwardVector() * DropDistance + FVector(0.f, 0.f, 30.f);

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Owner;
	SpawnParams.Instigator = Owner->GetInstigator();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	const TSubclassOf<AIronMine> ClassToSpawn = MineClass ? MineClass : TSubclassOf<AIronMine>(AIronMine::StaticClass());
	if (AIronMine* Mine = World->SpawnActor<AIronMine>(ClassToSpawn, FTransform(FRotator::ZeroRotator, Drop), SpawnParams))
	{
		Mine->Arm(Spec.Damage * DamageMultiplier, Spec.ExplosionRadius, GetOwnerController(), Owner);
	}
}
