#include "RocketLauncherComponent.h"
#include "RocketProjectile.h"
#include "Engine/World.h"
#include "MissileRules.h"

void URocketLauncherComponent::DoFire(const FVector& Origin, const FVector& AimDirection, const IronWeapons::Spec& Spec)
{
	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!World || !Owner)
	{
		return;
	}

	FVector Direction = AimDirection.GetSafeNormal();
	FVector Start = Origin;
	if (bHasLaunchPoint)
	{
		// Leave the pod pointed at a spot 60 m down the aim line; lower cars are still hit.
		Start = LaunchPoint;
		Direction = (Origin + Direction * 6000.f - LaunchPoint).GetSafeNormal();
		bHasLaunchPoint = false;
	}
	const TSubclassOf<ARocketProjectile> ClassToSpawn = ProjectileClass ? ProjectileClass : TSubclassOf<ARocketProjectile>(ARocketProjectile::StaticClass());

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Owner;
	SpawnParams.Instigator = Owner->GetInstigator();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	if (ARocketProjectile* Projectile = World->SpawnActor<ARocketProjectile>(ClassToSpawn, FTransform(Direction.Rotation(), Start), SpawnParams))
	{
		Projectile->SetTurnRate(IronMissiles::Tuning().TurnRateDeg * GuidedTurnScale);
		Projectile->Launch(Direction, ShotDamage(Spec), Spec.ExplosionRadius, Owner, GetOwnerController(), LockedTarget.Get());
		LockedTarget.Reset();
	}
}
