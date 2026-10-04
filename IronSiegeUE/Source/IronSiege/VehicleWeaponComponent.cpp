#include "VehicleWeaponComponent.h"
#include "GameFramework/Pawn.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Misc/PackageName.h"

UVehicleWeaponComponent::UVehicleWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UVehicleWeaponComponent::BeginPlay()
{
	Super::BeginPlay();
	Loadout.Init(static_cast<IronWeapons::WeaponType>(WeaponType));
}

void UVehicleWeaponComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	Loadout.Tick(DeltaTime);
}

bool UVehicleWeaponComponent::TryFire(const FVector& Origin, const FVector& AimDirection)
{
	if (!Loadout.Fire())
	{
		return false;
	}
	DoFire(Origin, AimDirection, WeaponSpec());
	USoundBase* Sound = nullptr;
	const APlayerCameraManager* Camera = FireSoundFarVariants.Num() > 0 ? UGameplayStatics::GetPlayerCameraManager(this, 0) : nullptr;
	const float ListenerDistance = Camera ? float(FVector::Dist(Camera->GetCameraLocation(), Origin)) : 0.f;
	if (ListenerDistance > FarSoundDistance)
	{
		Sound = LoadIfPresent(FireSoundFarVariants[FMath::RandHelper(FireSoundFarVariants.Num())]);
	}
	// (-LogCmds="LogTemp Verbose" lists which take each shot played and how far away it was.)
	UE_LOG(LogTemp, Verbose, TEXT("IronSiege: shot sound %s for %s at %.0f m"), Sound ? TEXT("far") : TEXT("near"), *GetNameSafe(GetOwner()), ListenerDistance / 100.f);
	if (!Sound && FireSoundVariants.Num() > 0)
	{
		const int32 Pick = FMath::RandHelper(FireSoundVariants.Num() + 1);
		Sound = Pick < FireSoundVariants.Num() ? LoadIfPresent(FireSoundVariants[Pick]) : nullptr;
	}
	if (!Sound)
	{
		Sound = FireSound.LoadSynchronous();
	}
	if (Sound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, Origin, 1.f, FMath::FRandRange(0.94f, 1.06f));
	}
	return true;
}

USoundBase* UVehicleWeaponComponent::LoadIfPresent(const TSoftObjectPtr<USoundBase>& Sound)
{
	if (Sound.IsNull())
	{
		return nullptr;
	}
	if (USoundBase* Loaded = Sound.Get())
	{
		return Loaded;
	}
	static TSet<FSoftObjectPath> Missing;
	const FSoftObjectPath Path = Sound.ToSoftObjectPath();
	if (Missing.Contains(Path))
	{
		return nullptr;
	}
	if (!FPackageName::DoesPackageExist(Path.GetLongPackageName()))
	{
		Missing.Add(Path);
		return nullptr;
	}
	return Sound.LoadSynchronous();
}

AController* UVehicleWeaponComponent::GetOwnerController() const
{
	const AActor* Owner = GetOwner();
	if (const APawn* Pawn = Cast<APawn>(Owner))
	{
		return Pawn->GetController();
	}
	return Owner ? Owner->GetInstigatorController() : nullptr;
}

bool UVehicleWeaponComponent::StartReload()
{
	if (!Loadout.StartReload())
	{
		return false;
	}
	if (USoundBase* Sound = ReloadSound.LoadSynchronous())
	{
		if (const AActor* Owner = GetOwner())
		{
			UGameplayStatics::PlaySoundAtLocation(this, Sound, Owner->GetActorLocation(), 0.9f);
		}
	}
	return true;
}
