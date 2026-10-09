#include "VehicleHealthComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "IronTeams.h"

UVehicleHealthComponent::UVehicleHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UVehicleHealthComponent::BeginPlay()
{
	Super::BeginPlay();
	State.MaxHealth = MaxHealth;
	State.Health = MaxHealth;
	State.MaxArmor = MaxArmor;
	State.Armor = MaxArmor;
	if (AActor* Owner = GetOwner())
	{
		Owner->OnTakeAnyDamage.AddDynamic(this, &UVehicleHealthComponent::HandleTakeAnyDamage);
	}
}

void UVehicleHealthComponent::ConfigureFromClassStats(float InMaxHealth, float InMaxArmor, float InArmorDamageReduction)
{
	MaxHealth = InMaxHealth;
	MaxArmor = InMaxArmor;
	BaseMaxArmor = InMaxArmor;
	ArmorDamageReduction = InArmorDamageReduction;
	State.MaxHealth = MaxHealth;
	State.Health = MaxHealth;
	State.MaxArmor = MaxArmor;
	State.Armor = MaxArmor;
}

void UVehicleHealthComponent::HandleTakeAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser)
{
	if (Damage <= 0.f || bDestroyedNotified || bInvulnerable)
	{
		return;
	}
	// No friendly fire: the Legion's cars ignore each other's shots, and the player cannot hurt a
	// truck they are escorting (IronTeams). A car's own driver can still hurt it.
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (InstigatedBy && OwnerPawn && OwnerPawn->GetController() && InstigatedBy != OwnerPawn->GetController()
		&& IronTeams::IsPlayerSideController(InstigatedBy) == IronTeams::IsPlayerSide(OwnerPawn))
	{
		return;
	}
	Damage *= DamageTakenScale;
	IronDamage::ApplyDamage(State, Damage, ArmorDamageReduction);
	OnVehicleDamaged.Broadcast(Damage, InstigatedBy);
	if (IronDamage::IsDestroyed(State) && !bDestroyedNotified)
	{
		bDestroyedNotified = true;
		OnVehicleDestroyed.Broadcast();
	}
}

void UVehicleHealthComponent::SetArmorScale(float Scale)
{
	if (BaseMaxArmor <= 0.f)
	{
		BaseMaxArmor = State.MaxArmor;
	}
	const float NewMax = BaseMaxArmor * FMath::Max(Scale, 0.f);
	if (State.Health > 0.f && NewMax > State.MaxArmor)
	{
		State.Armor += NewMax - State.MaxArmor;
	}
	State.MaxArmor = NewMax;
	State.Armor = FMath::Min(State.Armor, NewMax);
	MaxArmor = NewMax;
}
