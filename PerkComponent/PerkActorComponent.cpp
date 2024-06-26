// Fill out your copyright notice in the Description page of Project Settings.


#include "../Components/PerkActorComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "GESHandler.h"
#include "PVDMaterialEffectControllerComp.h"
#include "PVDPlayerProgressComponent.h"
#include "Kismet/GameplayStatics.h"
#include "PVD/PVDPlayerController.h"
#include "PVD/PVDPlayerState.h"
#include "PVD/Characters/PVDCharacter.h"
#include "PVD/Characters/PVDCharacterBase.h"
#include "PVD/GAS/PVDAbilitySystemComponent.h"
#include "PVD/UI/ElementalPerkSelectionWidget.h"
#include "PVD/UI/MainHUDWidget.h"
#include "PVD/UI/PerkElementWidget.h"
#include "PVD/UI/PerkPanelWidget.h"

// Sets default values for this component's properties
UPerkActorComponent::UPerkActorComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.

	// ...
	CurrentPlayerLevel = 1;
	LastLevelUp = 1;
}


// Called when the game starts
void UPerkActorComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	if (const auto PlayerState = Cast<APVDPlayerState>(UGameplayStatics::GetPlayerState(GetWorld(), 0)))
	{
		if (const auto PlayerProgressComp = PlayerState->GetPlayerProgressComponent())
		{
			PlayerProgressComp->OnLevelUp.AddDynamic(this, &ThisClass::OnPlayerLeveledUp);
		}
	}
}

void UPerkActorComponent::LevelUp()
{
	ShowRandomPerks(3);
}

TArray<UPerkDataAsset*> UPerkActorComponent::GetPerkList(int8 RequestedAmount) const
{
	TArray<UPerkDataAsset*> PerkPool = AvailablePerks;

	/** Remove none selectable perks from the pool */
	PerkPool.RemoveAll<>([=](const UPerkDataAsset* PerkDataAsset)
	{
		return !PerkDataAsset->IsSelectable;
	});

	if (RequestedAmount > 0)
	{
		TArray<UPerkDataAsset*> PerkDataAssets;

		RequestedAmount = FMath::Clamp(RequestedAmount, 0, PerkPool.Num());
		if (RequestedAmount > 0)
		{
			for (int i = 0; i < RequestedAmount; i++)
			{
				const int32 RandomValue = FMath::RandRange(0, PerkPool.Num() - 1);
				PerkDataAssets.Add(PerkPool[RandomValue]);
				
				PerkPool.RemoveAt(RandomValue);
			}
		}
		
		return PerkDataAssets;
	}

	/** return all available Perks that are selectable/purchasable */
	return PerkPool;
}

void UPerkActorComponent::ShowRandomPerks(int8 Amount)
{
	const auto PerkDataAssets = GetPerkList(Amount);

	const auto PlayerController = Cast<APVDPlayerController>(GetWorld()->GetFirstPlayerController());

	const auto MainHUDWidget = PlayerController->GetHUDWidget();

	MainHUDWidget->PerkSelection();

	PerkSelectionWidget = MainHUDWidget->PerkPanel;
	PerkSelectionWidget->OnPerkSelect.AddUniqueDynamic(this, &ThisClass::OnLevelUpPerkSelected);

	PerkSelectionWidget->DrawLevelUpPerks(PerkDataAssets, OwnedPerks, LastLevelUp);

	/** Set gameplay ui and mouse */
	PlayerController->SetInputMode(FInputModeUIOnly());
	PlayerController->SetShowMouseCursor(true);

	UGameplayStatics::SetGamePaused(GetWorld(), true);
}

bool UPerkActorComponent::GainPerk(UPerkDataAsset* PerkDataAsset)
{
	const bool IsPerkAvailable = AvailablePerks.Contains(PerkDataAsset);
	const bool IsPerkOwned = OwnedPerks.Contains(PerkDataAsset);
	int PerkLevel;

	if (IsPerkOwned)
	{
		PerkLevel = GetCurrentPerkLevel(PerkDataAsset);
	}
	else
	{
		PerkLevel = 0;
	}

	if (IsPerkAvailable && !IsPerkOwned)
	{
		OwnedPerks.Add(PerkDataAsset, PerkLevel + 1);
		GEngine->AddOnScreenDebugMessage(-1, 3, FColor::Blue,
		                                 FString::Printf(TEXT("Perk Gained:%s %d"), *PerkDataAsset->GetName(), PerkLevel + 1));
	}
	else if (IsPerkAvailable && IsPerkOwned)
	{
		TrySetCurrentPerkLevel(PerkDataAsset, PerkLevel + 1);
		GEngine->AddOnScreenDebugMessage(-1, 3, FColor::Yellow,
		                                 FString::Printf(TEXT("Owned Perk Level Set:%s %d"), *PerkDataAsset->GetName(), PerkLevel + 1));
	}

	bool IsPerkLastLevel = PerkDataAsset->PerkLevelDatas.Num() == PerkLevel + 1;

	if (IsPerkLastLevel)
	{
		AvailablePerks.Remove(PerkDataAsset);
	}

	switch (PerkDataAsset->PerkType)
	{
	case EPerkType::GameplayAbility:
		GivePerkAbility(PerkDataAsset);
		break;
	case EPerkType::GameplayEffect:
		ApplyGameplayEffect(PerkDataAsset);
		break;
	default:
		PVD_LOG(Error, TEXT("Given Perk Data Asset's Perk Type is Invalid!"))
	}

	return IsPerkAvailable && !IsPerkOwned;
}

bool UPerkActorComponent::LosePerk(UPerkDataAsset* PerkDataAsset)
{
	const bool IsPerkOwned = OwnedPerks.Contains(PerkDataAsset);

	if (IsPerkOwned)
	{
		ClearPerkAbilities(PerkDataAsset);
		OwnedPerks.Remove(PerkDataAsset);
		AvailablePerks.Add(PerkDataAsset);
	}

	return IsPerkOwned;
}

void UPerkActorComponent::GivePerkAbility(UPerkDataAsset* PerkDataAsset)
{
	const bool IsPerkOwned = OwnedPerks.Contains(PerkDataAsset);

	if (!IsPerkOwned)
	{
		return;
	}

	uint8 PerkLevelIndex = GetCurrentPerkLevel(PerkDataAsset) - 1;

	APVDCharacter* Character = Cast<APVDCharacter>(GetOwner());

	if (!IsValid(Character))
	{
		return;
	}

	UAbilitySystemComponent* AbilitySystemComponent = Character->GetAbilitySystemComponent();

	if (!IsValid(AbilitySystemComponent))
	{
		return;
	}

	if (PerkLevelIndex > 1 && PerkDataAsset->AbilityImplementationType == EPerkAbilityImplementationType::Override)
	{
		if (PerkDataAsset->PerkLevelDatas.IsValidIndex(PerkLevelIndex))
		{
			TArray<FGameplayAbilitySpec> Abilities = AbilitySystemComponent->GetActivatableAbilities();

			FGameplayAbilitySpec* GameplayAbilitySpec = Abilities.FindByPredicate(
				[&PerkDataAsset , PerkLevelIndex](const FGameplayAbilitySpec& AbilitySpec)
				{
					return AbilitySpec.Ability.Get() == PerkDataAsset->PerkLevelDatas[PerkLevelIndex - 1].Ability.
						GetDefaultObject();
				});

			if (GameplayAbilitySpec == nullptr)
			{
				return;
			}

			AbilitySystemComponent->ClearAbility(GameplayAbilitySpec->Handle);

			if (PerkDataAsset->PerkLevelDatas[PerkLevelIndex].Ability != nullptr)
			{
				const auto GameplayAbilitySpecHandle = AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(
					PerkDataAsset->PerkLevelDatas[PerkLevelIndex].Ability,
					1, static_cast<int32>(PerkDataAsset->PerkLevelDatas
						[
							PerkLevelIndex].Ability.GetDefaultObject()->
							                AbilityInputID), this));

				/** Set spec handle of the perk ablity to reference if needed */
				PerkDataAsset->GivenAbilitySpecHandle = GameplayAbilitySpecHandle;
			}

			if (PerkDataAsset->PerkLevelDatas[PerkLevelIndex].UseAbilityOnAdd)
			{
				AbilitySystemComponent->TryActivateAbilityByClass(PerkDataAsset->PerkLevelDatas[PerkLevelIndex].Ability);
			}
		}
		else
		{
			PVD_LOG(Error, TEXT("Invalid override ability perk level %d for Abiity Perk:%s"), PerkLevelIndex, *PerkDataAsset->GetName())
		}
	}
	else
	{
		if (PerkDataAsset->PerkLevelDatas.IsValidIndex(PerkLevelIndex))
		{
			if (PerkDataAsset->PerkLevelDatas[PerkLevelIndex].Ability != nullptr)
			{
				const auto GameplayAbilitySpecHandle = AbilitySystemComponent->GiveAbility(
					FGameplayAbilitySpec(PerkDataAsset->PerkLevelDatas[PerkLevelIndex].Ability, 1,
					                     static_cast<int32>(PerkDataAsset->PerkLevelDatas[PerkLevelIndex].Ability.
						                     GetDefaultObject()
						                     ->AbilityInputID), this));

				/** Set spec handle of the perk ablity to reference if needed */
				PerkDataAsset->GivenAbilitySpecHandle = GameplayAbilitySpecHandle;
			}

			if (PerkDataAsset->PerkLevelDatas[PerkLevelIndex].UseAbilityOnAdd)
			{
				AbilitySystemComponent->TryActivateAbilityByClass(PerkDataAsset->PerkLevelDatas[PerkLevelIndex].Ability);
			}
		}
		else
		{
			PVD_LOG(Error, TEXT("Invalid ability perk level %d for Abiity Perk:%s"), PerkLevelIndex, *PerkDataAsset->GetName())
		}
	}

	if (PerkDataAsset->HasMaterialControlFX)
	{
		GES_MATERIAL_EFFECT_EMIT(PerkDataAsset->MaterialFXEventName, Character);
	}

}

int UPerkActorComponent::GetCurrentPerkLevel(const UPerkDataAsset* PerkDataAsset)
{
	return OwnedPerks[PerkDataAsset];
}

bool UPerkActorComponent::TrySetCurrentPerkLevel(const UPerkDataAsset* PerkDataAsset, const int Level)
{
	if (!IsValid(PerkDataAsset))
	{
		return false;
	}

	if (!OwnedPerks.Contains(PerkDataAsset))
	{
		return false;
	}

	OwnedPerks[PerkDataAsset] = Level;

	return true;
}

void UPerkActorComponent::OnPlayerLeveledUp(int32 InNewLevel)
{
	CurrentPlayerLevel = InNewLevel;
}

void UPerkActorComponent::OnLevelUpPerkSelected(UPerkDataAsset* InGainPerk)
{
	GainPerk(InGainPerk);

	if (const auto PlayerController = Cast<APVDPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0)))
	{
		const auto MainHUDWidget = PlayerController->GetHUDWidget();
		MainHUDWidget->Main();

		PlayerController->SetShowMouseCursor(false);
		PlayerController->SetInputMode(FInputModeGameOnly());

		UGameplayStatics::SetGamePaused(GetWorld(), false);

		LastLevelUp++;

		/** we still have level up opportunity */
		if (LastLevelUp < CurrentPlayerLevel)
		{
			ShowRandomPerks(3);
		}
		else
		{
			MainHUDWidget->ToggleLevelUpIndication(false);
		}
	}
}

void UPerkActorComponent::ClearPerkAbilities(const UPerkDataAsset* PerkDataAsset)
{
	switch (PerkDataAsset->AbilityImplementationType)
	{
	case EPerkAbilityImplementationType::Override:
		ClearOverridePerkAbility(PerkDataAsset);
		break;
	case EPerkAbilityImplementationType::Additive:
		ClearPerkAbilities(PerkDataAsset);
		break;
	default:
		PVD_LOG(Error, TEXT("Perk Ability Implementation Type is Invalid!"));
	}
}

void UPerkActorComponent::ClearOverridePerkAbility(const UPerkDataAsset* PerkDataAsset)
{
	uint8 PerkLevel = GetCurrentPerkLevel(PerkDataAsset);

	APVDCharacter* Character = Cast<APVDCharacter>(GetOwner());

	if (!IsValid(Character))
	{
		return;
	}

	UAbilitySystemComponent* AbilitySystemComponent = Character->GetAbilitySystemComponent();

	TArray<FGameplayAbilitySpec> Abilities = AbilitySystemComponent->GetActivatableAbilities();

	FGameplayAbilitySpec* GameplayAbilitySpec = Abilities.FindByPredicate(
		[&PerkDataAsset , PerkLevel](const FGameplayAbilitySpec& AbilitySpec)
		{
			return AbilitySpec.Ability.Get() == PerkDataAsset->PerkLevelDatas[PerkLevel - 1].Ability.
				GetDefaultObject();
		});

	if (GameplayAbilitySpec != nullptr)
	{
		AbilitySystemComponent->ClearAbility(GameplayAbilitySpec->Handle);
	}
}

void UPerkActorComponent::ClearAdditivePerkAbility(const UPerkDataAsset* PerkDataAsset)
{
	APVDCharacter* Character = Cast<APVDCharacter>(GetOwner());

	if (!IsValid(Character))
	{
		return;
	}

	UAbilitySystemComponent* AbilitySystemComponent = Character->GetAbilitySystemComponent();

	TArray<FGameplayAbilitySpec> Abilities = AbilitySystemComponent->GetActivatableAbilities();

	int PerkLevel = GetCurrentPerkLevel(PerkDataAsset);

	for (int i = 0; i < PerkLevel; i++)
	{
		FGameplayAbilitySpec* GameplayAbilitySpec = Abilities.FindByPredicate(
			[&PerkDataAsset , i](const FGameplayAbilitySpec& AbilitySpec)
			{
				return AbilitySpec.Ability.Get() == PerkDataAsset->PerkLevelDatas[i].Ability.GetDefaultObject();
			});

		if (GameplayAbilitySpec != nullptr)
		{
			AbilitySystemComponent->ClearAbility(GameplayAbilitySpec->Handle);
		}
	}
}

void UPerkActorComponent::ApplyGameplayEffect(UPerkDataAsset* PerkDataAsset)
{
	if (!IsValid(PerkDataAsset))
	{
		PVD_LOG(Error, TEXT("Given Perk Data Asset is Invalid!"));
		return;
	}

	UGameplayEffect* GameplayEffect = PerkDataAsset->GameplayEffect.GetDefaultObject();

	if (!IsValid(GameplayEffect))
	{
		PVD_LOG(Error, TEXT("Given Perk Data Asset's Gameplay Effect is Invalid!"));
		return;
	}

	APVDCharacter* Character = Cast<APVDCharacter>(GetOwner());

	if (!IsValid(Character))
	{
		return;
	}

	//Can be Cached???
	UAbilitySystemComponent* AbilitySystemComponent = Character->GetAbilitySystemComponent();

	if (!IsValid(AbilitySystemComponent))
	{
		return;
	}

	FGameplayEffectContextHandle GameplayEffectContextHandle;
	if (PerkDataAsset->IsSetMagnitudeByCaller)
	{
		GameplayEffectContextHandle = AbilitySystemComponent->MakeEffectContext();
		GameplayEffectContextHandle.AddSourceObject(GetOwner());

		const auto EffectSpecHandle = AbilitySystemComponent->MakeOutgoingSpec(PerkDataAsset->GameplayEffect, 1,
		                                                                       GameplayEffectContextHandle);

		UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(EffectSpecHandle, PerkDataAsset->MagnitudeDataTag.First(),
		                                                              PerkDataAsset->Magnitude);
		PerkDataAsset->ActivationGameplayEffectHandle = AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*EffectSpecHandle.Data.Get());
	}
	else
	{
		PerkDataAsset->ActivationGameplayEffectHandle = AbilitySystemComponent->ApplyGameplayEffectToSelf(
			GameplayEffect, GetCurrentPerkLevel(PerkDataAsset), GameplayEffectContextHandle);
	}

	if (PerkDataAsset->HasMaterialControlFX)
	{
		GES_MATERIAL_EFFECT_EMIT(PerkDataAsset->MaterialFXEventName, Character);
	}
}
