// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PVD/Data/PerkDataAsset.h"
#include "PerkActorComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class PVD_API UPerkActorComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UPerkActorComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	UFUNCTION()
	void LevelUp();
	
	UFUNCTION()
	bool GainPerk(UPerkDataAsset* PerkDataAsset);

	UFUNCTION()
	bool LosePerk(UPerkDataAsset* PerkDataAsset);
	
	/** Get Amount of available perks excluding not selectable ones. Return all available if Amount is not supplied */
	UFUNCTION()
	TArray<UPerkDataAsset*> GetPerkList(int8 Amount = 0) const;

	UFUNCTION()
	const TMap<UPerkDataAsset* , int8>& GetOwnedPerks() const { return OwnedPerks; }
	
private:
	UFUNCTION()
	void ShowRandomPerks(int8 Amount);
	UFUNCTION()
	void ClearPerkAbilities(const UPerkDataAsset* PerkDataAsset);
	UFUNCTION()
	void ClearOverridePerkAbility(const UPerkDataAsset* PerkDataAsset);
	UFUNCTION()
	void ClearAdditivePerkAbility(const UPerkDataAsset* PerkDataAsset);
	UFUNCTION()
	void ApplyGameplayEffect(UPerkDataAsset* PerkDataAsset);
	UFUNCTION()
	void GivePerkAbility(UPerkDataAsset* PerkDataAsset);
	UFUNCTION()
	int GetCurrentPerkLevel(const UPerkDataAsset* PerkDataAsset);
	UFUNCTION()
	bool TrySetCurrentPerkLevel(const UPerkDataAsset* PerkDataAsset,int Level);
	
	UFUNCTION()
	void OnPlayerLeveledUp(int32 InNewLevel);

	UFUNCTION()
	void OnLevelUpPerkSelected(UPerkDataAsset* InGainPerk);

	UPROPERTY()
	TMap<UPerkDataAsset* , int8> OwnedPerks;

	/** player level and level-up tracking */
	int32 LastLevelUp;
	int32 CurrentPlayerLevel;
	
public:
	UPROPERTY(VisibleAnywhere , BlueprintReadOnly)
	class UPerkPanelWidget* PerkSelectionWidget;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<UPerkDataAsset*> AvailablePerks;
	
};
