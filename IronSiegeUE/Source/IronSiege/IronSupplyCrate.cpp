#include "IronSupplyCrate.h"
#include "IronSiegeText.h"
#include "IronVehicle.h"
#include "IronSiegeHUD.h"
#include "VehicleHealthComponent.h"
#include "MachineGunComponent.h"
#include "RocketLauncherComponent.h"
#include "WarVehiclePawn.h"
#include "IronVehicleKit.h"
#include "Misc/PackageName.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "Sound/SoundBase.h"

AIronSupplyCrate::AIronSupplyCrate()
{
	PrimaryActorTick.bCanEverTick = true;
	InitialLifeSpan = 90.f; // Uncollected drops expire so they don't pile up across waves.

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	Crate = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Crate"));
	RootComponent = Crate;
	Crate->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Crate->SetRelativeScale3D(FVector(1.1f, 1.1f, 0.8f));
	if (CubeFinder.Succeeded()) Crate->SetStaticMesh(CubeFinder.Object);

	// Tall thin light column so drops can be spotted over parked traffic and down side streets.
	Beacon = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Beacon"));
	Beacon->SetupAttachment(Crate);
	Beacon->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Beacon->SetCastShadow(false);
	Beacon->SetAbsolute(false, false, true);
	Beacon->SetRelativeScale3D(FVector(0.07f, 0.07f, 7.f));
	Beacon->SetRelativeLocation(FVector(0.f, 0.f, 440.f));
	if (CylinderFinder.Succeeded()) Beacon->SetStaticMesh(CylinderFinder.Object);
}

void AIronSupplyCrate::BeginPlay()
{
	Super::BeginPlay();
	if (UMaterialInterface* Mat = (SupplyType == EIronSupplyType::Repair ? RepairMaterial : AmmoMaterial).LoadSynchronous())
	{
		// The modelled transit case (Tools/meshgen SupplyCrate) once it is imported: the kit's
		// finishes on the case, the drop's colour on its marker panels. The glowing block until then.
		const TCHAR* ModelPackage = TEXT("/Game/IronSiege/Arena/Meshes/SM_Prop_Crate");
		UStaticMesh* Model = FPackageName::DoesPackageExist(ModelPackage) ? LoadObject<UStaticMesh>(nullptr, TEXT("/Game/IronSiege/Arena/Meshes/SM_Prop_Crate.SM_Prop_Crate")) : nullptr;
		if (Model)
		{
			Crate->SetStaticMesh(Model);
			Crate->SetRelativeScale3D(FVector(1.f));
			IronKitBuilder::ApplyFinishesBySlot(Crate);
			const int32 Marker = Crate->GetMaterialIndex(TEXT("Supply"));
			if (Marker != INDEX_NONE)
			{
				Crate->SetMaterial(Marker, Mat);
			}
		}
		else
		{
			Crate->SetMaterial(0, Mat);
		}
		Beacon->SetMaterial(0, Mat);
	}
	// Settle just above whatever surface is below the spawn point.
	FVector Loc = GetActorLocation();
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(IronSupplySettle), false, this);
	if (GetWorld() && GetWorld()->LineTraceSingleByChannel(Hit, Loc + FVector(0.f, 0.f, 300.f), Loc - FVector(0.f, 0.f, 1000.f), ECC_Visibility, Params))
	{
		Loc.Z = Hit.ImpactPoint.Z + 90.f;
	}
	BaseLocation = Loc;
	SetActorLocation(Loc);
}

void AIronSupplyCrate::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	SetActorLocationAndRotation(BaseLocation + FVector(0.f, 0.f, 18.f * FMath::Sin(Age * 2.5f)), FRotator(0.f, Age * 70.f, 0.f));

	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	const float PlayerDist = Player ? FVector::Dist(Player->GetActorLocation(), BaseLocation) : BIG_NUMBER;
	Beacon->SetVisibility(PlayerDist > 1500.f);
	if (!Player || PlayerDist > PickupRadius)
	{
		return;
	}
	const FString Notice = TryApply(Player);
	if (Notice.IsEmpty())
	{
		return; // Nothing to top up - leave it for later.
	}
	if (APlayerController* PC = Cast<APlayerController>(Player->GetController()))
	{
		if (AIronSiegeHUD* HUD = PC->GetHUD<AIronSiegeHUD>())
		{
			HUD->ShowNotice(Notice, SupplyType == EIronSupplyType::Repair ? FLinearColor(0.35f, 1.f, 0.45f) : FLinearColor(1.f, 0.75f, 0.25f));
		}
	}
	if (USoundBase* Sound = PickupSound.LoadSynchronous())
	{
		UGameplayStatics::PlaySound2D(this, Sound);
	}
	Destroy();
}

FString AIronSupplyCrate::TryApply(APawn* PlayerPawn) const
{
	const IIronVehicle* Vehicle = Cast<IIronVehicle>(PlayerPawn);
	if (!Vehicle)
	{
		return FString();
	}
	// Some drivers get more out of a crate (IronCrew::Perk::SupplyGain).
	const AWarVehiclePawn* Car = Cast<AWarVehiclePawn>(PlayerPawn);
	const float Gain = Car ? Car->GetSupplyGain() : 1.f;
	if (SupplyType == EIronSupplyType::Repair)
	{
		UVehicleHealthComponent* Health = Vehicle->GetHealthComponent();
		return Health && Health->Repair(RepairHealth * Gain, RepairArmor * Gain) ? FString::Printf(TEXT("+%.0f %s  +%.0f %s"), RepairHealth * Gain, *IronText::Str(TEXT("CrateHealth"), TEXT("HEALTH")), RepairArmor * Gain, *IronText::Str(TEXT("CrateArmor"), TEXT("ARMOR"))) : FString();
	}
	int32 AddedRounds = 0, AddedRockets = 0;
	if (UMachineGunComponent* Gun = Vehicle->GetPrimaryWeaponComponent())
	{
		AddedRounds = Gun->AddReserveAmmo(FMath::RoundToInt(MachineGunRounds * Gain));
	}
	if (URocketLauncherComponent* Launcher = Vehicle->GetSecondaryWeaponComponent())
	{
		AddedRockets = Launcher->AddReserveAmmo(FMath::RoundToInt(Rockets * Gain));
	}
	return AddedRounds + AddedRockets > 0 ? FString::Printf(TEXT("+%d %s  +%d %s"), AddedRounds, *IronText::Str(TEXT("CrateRounds"), TEXT("ROUNDS")), AddedRockets, *IronText::Str(TEXT("CrateRockets"), TEXT("ROCKETS"))) : FString();
}
