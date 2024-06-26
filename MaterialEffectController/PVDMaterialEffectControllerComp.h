// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <stdexcept>
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PVDMaterialEffectControllerComp.generated.h"

class UCurveFloat;
class UCurveLinearColor;

UENUM()
enum class EMatFXGlobalEvent : uint8
{
	MatFX_BeginPlay,
	MatFX_EnemyTakeDamage,
	MatFX_WeaponAttached,
	MatFX_WeaponDetached,
	MatFX_PerkDivineHaste,
	MatFX_PerkVolvansFeather,
	MatFx_MemoryPuzzle1,
	MatFx_MemoryPuzzle2,
	MatFx_MemoryPuzzle3,
	MatFx_MemoryPuzzle4,
	MatFx_BossBerserk,
	MatFx_BossSecondPhase,
};

UENUM()
enum class EMaterialEffectType
{
	OverrideMaterial,
	ChangeParameters
};

UENUM()
enum class EMaterialParameterChangeType
{
	Additive,
	Multiply,
	Override
};

UENUM()
enum class EMaterialParamType
{
	Float,
	Color UMETA(DisplayName="Color (Vector)"),
	Texture UMETA(DisplayName="Texture (Can't Animate)")
};

USTRUCT()
struct FMaterialParameterChangeConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FName ParameterName;
	
	UPROPERTY(EditAnywhere, meta = (EditConditionHides, EditCondition = "ParameterType != EMaterialParamType::Texture"))
	EMaterialParameterChangeType ParameterChangeType;

	UPROPERTY(EditAnywhere, meta = (EditConditionHides, EditCondition = "ParameterType != EMaterialParamType::Texture"))
	bool IsAnimation;
	
	UPROPERTY(EditAnywhere, meta = (EditConditionHides, EditCondition = "IsAnimation && ParameterType != EMaterialParamType::Texture"))

	float AnimationTime;
	
	UPROPERTY(EditAnywhere, meta = (EditConditionHides, EditCondition = "ParameterType == EMaterialParamType::Float"))
	bool ReturnToDefaultValue;
	UPROPERTY(EditAnywhere, meta = (EditConditionHides, EditCondition = "ReturnToDefaultValue && ParameterType == EMaterialParamType::Float"))

	float DefaultValue;
	
	UPROPERTY(EditAnywhere,
		meta = (EditConditionHides, EditCondition =
			"IsAnimation && ParameterType == EMaterialParamType::Float"
		))
	TObjectPtr<UCurveFloat> FloatCurve;
	
	UPROPERTY(EditAnywhere,
		meta = (EditConditionHides, EditCondition =
			"IsAnimation && ParameterType == EMaterialParamType::Color"
		))
	TObjectPtr<UCurveLinearColor> ColorCurve;
	
	UPROPERTY(EditAnywhere)
	bool HasDelay;

	UPROPERTY(EditAnywhere, meta = (EditConditionHides, EditCondition = "hasDelay"))
	float Delay;

	UPROPERTY(EditAnywhere)
	bool HasLifetime;

	UPROPERTY(EditAnywhere, meta = (EditConditionHides, EditCondition = "hasLifetime"))
	float Lifetime;

	UPROPERTY(EditAnywhere)
	EMaterialParamType ParameterType;

	UPROPERTY(EditAnywhere,
		meta = (EditConditionHides, EditCondition = "ParameterType == EMaterialParamType::Float"))
	float FloatParameterValue;

	UPROPERTY(EditAnywhere,
		meta = (EditConditionHides, EditCondition = "ParameterType == EMaterialParamType::Color"))
	FLinearColor LinearColorParameterValue;

	UPROPERTY(EditAnywhere,
		meta = (EditConditionHides, EditCondition =
			"!IsAnimation && ParameterType == EMaterialParamType::Texture"
		))
	TObjectPtr<UTexture2D> TextureParameterValue;
};

USTRUCT()
struct FMaterialEffectConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	EMatFXGlobalEvent GlobalEventType;

	UPROPERTY(EditAnywhere)
	EMaterialEffectType MaterialEffectType;
	UPROPERTY(EditAnywhere,meta = (EditConditionHides, EditCondition = "MaterialEffectType != EMaterialEffectType::ChangeParameters"))
	UMaterialInterface* EffectMaterial;
	UPROPERTY(EditAnywhere)
	bool EffectAllMeshes;
	UPROPERTY(EditAnywhere, meta = (EditConditionHides, EditCondition = "!EffectAllMeshes"))
	TArray<FString> EffectedMeshNameList;
	UPROPERTY(EditAnywhere)
	bool IsOverlaySlot;
	UPROPERTY(EditAnywhere, meta = (EditConditionHides, EditCondition = "!IsOverlaySlot"))
	bool EffectAllSlots;
	UPROPERTY(EditAnywhere, meta = (EditConditionHides, EditCondition = "!EffectAllSlots && !IsOverlaySlot"))
	TArray<int> EffectedSlotIds;
	UPROPERTY(EditAnywhere,
		meta = (EditConditionHides, EditCondition = "MaterialEffectType == EMaterialEffectType::ChangeParameters"))
	TArray<FMaterialParameterChangeConfig> ParameterConfigs;
	UPROPERTY(EditAnywhere, meta = (EditConditionHides, EditCondition = "MaterialEffectType != EMaterialEffectType::ChangeParameters"))
	bool HasDelay;
	UPROPERTY(EditAnywhere, meta = (EditConditionHides, EditCondition = "hasDelay && MaterialEffectType != EMaterialEffectType::ChangeParameters"))
	float Delay;
	UPROPERTY(EditAnywhere, meta = (EditConditionHides, EditCondition = "MaterialEffectType != EMaterialEffectType::ChangeParameters"))
	bool HasLifetime;
	UPROPERTY(EditAnywhere, meta = (EditConditionHides, EditCondition = "hasLifetime && MaterialEffectType != EMaterialEffectType::ChangeParameters"))
	float Lifetime;
};

USTRUCT()
struct FMaterialEffectConfigContainer
{
	GENERATED_BODY()

	TArray<FMaterialEffectConfig> Configs;
};

USTRUCT()
struct FMaterialChangeHandler
{
	GENERATED_BODY()

	TObjectPtr<UMeshComponent> EffectedMesh;
	bool IsOverlaySlot = false;
	size_t SlotId;
	TObjectPtr<UMaterialInterface> OldMaterial;
	TObjectPtr<UMaterialInterface> NewMaterial;
	bool IsApplied = false;
	float Delay = 0;
	float DelayCounter = 0;
	float Lifetime = 0;
	float LifetimeCounter = 0;
};

USTRUCT()
struct FParameterChangeHandler
{
	GENERATED_BODY()

	TObjectPtr<UMeshComponent> EffectedMesh;
	TObjectPtr<UMaterialInstanceDynamic> MaterialInstance;
	bool IsOverlaySlot = false;
	size_t SlotId;
	float OldFloatValue;
	FLinearColor OldLinearColorValue;
	TObjectPtr<UTexture> OldTextureValue;
	FMaterialParameterChangeConfig Config;
	bool IsApplied = false;
	float DelayCounter = 0;
	float LifetimeCounter = 0;
	float AnimationCounter = 0;
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PVD_API UPVDMaterialEffectControllerComp : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UPVDMaterialEffectControllerComp();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

private:
	UPROPERTY()
	TMap<EMatFXGlobalEvent, FMaterialEffectConfigContainer> EventConfigMap;

public:
	UPROPERTY(EditAnywhere)
	TArray<FMaterialEffectConfig> Configs;

	UPROPERTY(VisibleAnywhere)
	TArray<FMaterialChangeHandler> MaterialChangeHandlers;

	UPROPERTY(VisibleAnywhere)
	TArray<FParameterChangeHandler> ParameterChangeHandlers;

private:
	UFUNCTION()
	const TArray<UMeshComponent*> GetMeshes(const FMaterialEffectConfig& Config) noexcept;

	UFUNCTION()
	const bool SetMaterialsOfMesh(FMaterialEffectConfig& Config, UMaterialInterface* Material,
	                              UMeshComponent* MeshComponent) noexcept;
	UFUNCTION()
	const bool SetParametersOfMaterials(FMaterialEffectConfig& Config, UMeshComponent* MeshComponent) noexcept;

	UFUNCTION()
	void ProcessMaterialsChanges(float DeltaTime) noexcept;

	UFUNCTION()
	void ProcessParameterChanges(float DeltaTime) noexcept;

public:
	UFUNCTION()
	void CategorizeConfigsWithEvents();

	UFUNCTION()
	void Run(TArray<FMaterialEffectConfig> ConfigArray);
	
	void Run(FMaterialEffectConfig& Config);
};
