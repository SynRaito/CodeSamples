// Fill out your copyright notice in the Description page of Project Settings.


#include "../Components/PVDMaterialEffectControllerComp.h"

#include "GESDataTypes.h"
#include "GESHandler.h"
#include "Curves/CurveLinearColor.h"
#include "Kismet/GameplayStatics.h"
#include "PVD/PVD.h"
#include "PVD/Characters/PVDCharacter.h"
#include "PVD/Characters/PVDCharacterBase.h"

// Sets default values for this component's properties
UPVDMaterialEffectControllerComp::UPVDMaterialEffectControllerComp()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
	// ...
}


// Called when the game starts
void UPVDMaterialEffectControllerComp::BeginPlay()
{
	Super::BeginPlay();
	// ...
	CategorizeConfigsWithEvents();

	/** Handle any Begin Play triggers, Map will check if any trigger is set as BeginPlay*/
	GES_MATERIAL_EFFECT_EMIT(EMatFXGlobalEvent::MatFX_BeginPlay, GetOwner());
	
}

void UPVDMaterialEffectControllerComp::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	PVD_LOG(Log, TEXT("Material Controller destroyed"));
	
	/** Unbind from GES events */ 
	FGESHandler::DefaultHandler()->RemoveAllListenersForReceiver(this);

	
}

// Called every frame
void UPVDMaterialEffectControllerComp::TickComponent(float DeltaTime, ELevelTick TickType,
                                                     FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	ProcessMaterialsChanges(DeltaTime);
	ProcessParameterChanges(DeltaTime);
}

const TArray<UMeshComponent*> UPVDMaterialEffectControllerComp::GetMeshes(const FMaterialEffectConfig& Config) noexcept
{
	TArray<UMeshComponent*> meshComponents;
	GetOwner()->GetComponents<UMeshComponent>(meshComponents);

	if (!Config.EffectAllMeshes)
	{
		TArray<UMeshComponent*> filteredMeshComponents;
		for (UMeshComponent* MeshComponent : meshComponents)
		{
			if (Config.EffectedMeshNameList.Contains(MeshComponent->GetName()))
			{
				filteredMeshComponents.Add(MeshComponent);
			}
		}
		return filteredMeshComponents;
	}
	return meshComponents;
}

const bool UPVDMaterialEffectControllerComp::SetMaterialsOfMesh(FMaterialEffectConfig& Config,
                                                                UMaterialInterface* Material,
                                                                UMeshComponent* MeshComponent) noexcept
{
	TArray<FName> SlotNames = MeshComponent->GetMaterialSlotNames();

	for (size_t index = 0; index < SlotNames.Num(); ++index)
	{
		if (!Config.EffectedSlotIds.Contains(index) && !Config.EffectAllSlots)
			continue;

		FMaterialChangeHandler MaterialChangeHandler;
		if (Config.HasDelay)
			MaterialChangeHandler.Delay = Config.Delay;
		if (Config.HasLifetime)
			MaterialChangeHandler.Lifetime = Config.Lifetime;
		MaterialChangeHandler.OldMaterial = MeshComponent->GetOverlayMaterial();
		MaterialChangeHandler.NewMaterial = Material;
		MaterialChangeHandler.EffectedMesh = MeshComponent;
		MaterialChangeHandler.IsOverlaySlot = Config.IsOverlaySlot;
		MaterialChangeHandler.SlotId = index;
		MaterialChangeHandlers.Add(MaterialChangeHandler);
	}
	return true;
}

const bool UPVDMaterialEffectControllerComp::SetParametersOfMaterials(FMaterialEffectConfig& Config,
                                                                      UMeshComponent* MeshComponent) noexcept
{
	TArray<FName> SlotNames = MeshComponent->GetMaterialSlotNames();

	for (size_t index = 0; index < SlotNames.Num(); ++index)
	{
		if (!Config.EffectedSlotIds.Contains(index) && !Config.EffectAllSlots && !Config.IsOverlaySlot)
			continue;
		for (FMaterialParameterChangeConfig ParameterConfig : Config.ParameterConfigs)
		{
			FParameterChangeHandler ParameterChangeHandler;
			ParameterChangeHandler.Config = ParameterConfig;
			ParameterChangeHandler.EffectedMesh = MeshComponent;
			ParameterChangeHandler.IsOverlaySlot = Config.IsOverlaySlot;
			ParameterChangeHandler.SlotId = index;

			ParameterChangeHandlers.Add(ParameterChangeHandler);

			if (Config.IsOverlaySlot)
				return true;
		}
	}
	return true;
}

void UPVDMaterialEffectControllerComp::ProcessMaterialsChanges(float DeltaTime) noexcept
{
	TArray<int> CompletedHandlerIds;
	for (int index = 0; index < MaterialChangeHandlers.Num(); ++index)
	{
		if (MaterialChangeHandlers[index].Delay > 0)
		{
			if (MaterialChangeHandlers[index].DelayCounter < MaterialChangeHandlers[index].Delay)
			{
				MaterialChangeHandlers[index].DelayCounter += DeltaTime;
				continue;
			}
		}

		if (!MaterialChangeHandlers[index].IsApplied)
		{
			if (MaterialChangeHandlers[index].IsOverlaySlot)
			{
				MaterialChangeHandlers[index].EffectedMesh->SetOverlayMaterial(MaterialChangeHandlers[index].NewMaterial);
			}
			else
			{
				MaterialChangeHandlers[index].EffectedMesh->SetMaterial(MaterialChangeHandlers[index].SlotId,
				                                                        MaterialChangeHandlers[index].NewMaterial);
			}

			MaterialChangeHandlers[index].IsApplied = true;
		}

		if (MaterialChangeHandlers[index].Lifetime > 0)
		{
			if (MaterialChangeHandlers[index].LifetimeCounter < MaterialChangeHandlers[index].Lifetime)
			{
				MaterialChangeHandlers[index].LifetimeCounter += DeltaTime;
			}
			else
			{
				if (MaterialChangeHandlers[index].IsOverlaySlot)
				{
					MaterialChangeHandlers[index].EffectedMesh->SetOverlayMaterial(MaterialChangeHandlers[index].OldMaterial);
				}
				else
				{
					MaterialChangeHandlers[index].EffectedMesh->SetMaterial(MaterialChangeHandlers[index].SlotId,
					                                                        MaterialChangeHandlers[index].OldMaterial);
				}
				CompletedHandlerIds.Add(index);
			}
		}
		else
		{
			CompletedHandlerIds.Add(index);
		}
	}

	for (int index = CompletedHandlerIds.Num() - 1; index >= 0; --index)
	{
		MaterialChangeHandlers.RemoveAt(CompletedHandlerIds[index]);
	}
}

void UPVDMaterialEffectControllerComp::ProcessParameterChanges(float DeltaTime) noexcept
{
	TArray<int> CompletedHandlerIds;
	for (int index = 0; index < ParameterChangeHandlers.Num(); ++index)
	{
		if (ParameterChangeHandlers[index].Config.Delay > 0 && ParameterChangeHandlers[index].Config.HasDelay)
		{
			if (ParameterChangeHandlers[index].DelayCounter < ParameterChangeHandlers[index].Config.Delay)
			{
				ParameterChangeHandlers[index].DelayCounter += DeltaTime;
				continue;
			}
		}

		if (ParameterChangeHandlers[index].MaterialInstance == nullptr)
		{
			UMaterialInstanceDynamic* MaterialInstance;
			if (ParameterChangeHandlers[index].IsOverlaySlot)
			{
				MaterialInstance = Cast<UMaterialInstanceDynamic>(
					ParameterChangeHandlers[index].EffectedMesh->GetOverlayMaterial());
			}
			else
			{
				MaterialInstance = Cast<UMaterialInstanceDynamic>(
					ParameterChangeHandlers[index].EffectedMesh->GetMaterial(ParameterChangeHandlers[index].SlotId));
			}
			if (MaterialInstance == nullptr)
			{
				if (ParameterChangeHandlers[index].IsOverlaySlot)
				{
					MaterialInstance = UMaterialInstanceDynamic::Create(
						ParameterChangeHandlers[index].EffectedMesh->GetOverlayMaterial(), this);
					ParameterChangeHandlers[index].EffectedMesh->SetOverlayMaterial(MaterialInstance);
				}
				else
				{
					MaterialInstance = UMaterialInstanceDynamic::Create(
						ParameterChangeHandlers[index].EffectedMesh->GetMaterial(ParameterChangeHandlers[index].SlotId), this);
					ParameterChangeHandlers[index].EffectedMesh->SetMaterial(ParameterChangeHandlers[index].SlotId, MaterialInstance);
				}
			}

			ParameterChangeHandlers[index].MaterialInstance = MaterialInstance;

			switch (ParameterChangeHandlers[index].Config.ParameterType)
			{
			case EMaterialParamType::Float:
				ParameterChangeHandlers[index].OldFloatValue = MaterialInstance->K2_GetScalarParameterValue(
					ParameterChangeHandlers[index].Config.ParameterName);
				if(ParameterChangeHandlers[index].Config.ReturnToDefaultValue)
				{
					ParameterChangeHandlers[index].OldFloatValue = ParameterChangeHandlers[index].Config.DefaultValue;
				}
				switch (ParameterChangeHandlers[index].Config.ParameterChangeType)
				{
				case EMaterialParameterChangeType::Additive:
					ParameterChangeHandlers[index].Config.FloatParameterValue += ParameterChangeHandlers[index].
						OldFloatValue;
					break;

				case EMaterialParameterChangeType::Multiply:
					ParameterChangeHandlers[index].Config.FloatParameterValue *= ParameterChangeHandlers[index].
						OldFloatValue;
					break;
				}
				break;

			case EMaterialParamType::Color:
				ParameterChangeHandlers[index].OldLinearColorValue = MaterialInstance->K2_GetVectorParameterValue(
					ParameterChangeHandlers[index].Config.ParameterName);

				switch (ParameterChangeHandlers[index].Config.ParameterChangeType)
				{
				case EMaterialParameterChangeType::Additive:
					ParameterChangeHandlers[index].Config.LinearColorParameterValue += ParameterChangeHandlers[index].
						OldLinearColorValue;
					break;
				case EMaterialParameterChangeType::Multiply:
					ParameterChangeHandlers[index].Config.LinearColorParameterValue *= ParameterChangeHandlers[index].
						OldLinearColorValue;
					break;
				}

				break;

			case EMaterialParamType::Texture:
				ParameterChangeHandlers[index].OldTextureValue = MaterialInstance->K2_GetTextureParameterValue(
					ParameterChangeHandlers[index].Config.ParameterName);
				break;
			}
		}

		if (!ParameterChangeHandlers[index].IsApplied)
		{
			if (ParameterChangeHandlers[index].Config.AnimationTime > 0 && ParameterChangeHandlers[index].Config.
			                                                                                              IsAnimation && ParameterChangeHandlers[index].Config.ParameterType != EMaterialParamType::Texture)
			{
				if (ParameterChangeHandlers[index].AnimationCounter < ParameterChangeHandlers[index].Config.
				                                                                                     AnimationTime)
				{
					float FloatCurveValue;
					FLinearColor ColorCurveValue;
					switch (ParameterChangeHandlers[index].Config.ParameterType)
					{
					case EMaterialParamType::Float:
						FloatCurveValue = ParameterChangeHandlers[index].Config.FloatCurve->GetFloatValue(
							ParameterChangeHandlers[index].AnimationCounter / ParameterChangeHandlers[index].Config.
							                                                                                 AnimationTime);
						ParameterChangeHandlers[index].MaterialInstance->SetScalarParameterValue(
							ParameterChangeHandlers[index].Config.ParameterName, FMath::Lerp(
								ParameterChangeHandlers[index].OldFloatValue,
								ParameterChangeHandlers[index].Config.FloatParameterValue,
								ParameterChangeHandlers[index].AnimationCounter / ParameterChangeHandlers[index].Config.
								                                                                                 AnimationTime) * FloatCurveValue);
						break;
					case EMaterialParamType::Color:
						ColorCurveValue = ParameterChangeHandlers[index].Config.ColorCurve->GetLinearColorValue(
							ParameterChangeHandlers[index].AnimationCounter / ParameterChangeHandlers[index].Config.
							                                                                                 AnimationTime);
						ParameterChangeHandlers[index].MaterialInstance->SetVectorParameterValue(
							ParameterChangeHandlers[index].Config.ParameterName, FMath::Lerp(
								ParameterChangeHandlers[index].OldLinearColorValue,
								ParameterChangeHandlers[index].Config.LinearColorParameterValue,
								ParameterChangeHandlers[index].AnimationCounter / ParameterChangeHandlers[index].Config.
								                                                                                 AnimationTime) * ColorCurveValue);
						break;
					}
					ParameterChangeHandlers[index].AnimationCounter += DeltaTime;
					continue;
				}
			}
			else
			{
				switch (ParameterChangeHandlers[index].Config.ParameterType)
				{
				case EMaterialParamType::Float:
					ParameterChangeHandlers[index].MaterialInstance->SetScalarParameterValue(
						ParameterChangeHandlers[index].Config.ParameterName,
						ParameterChangeHandlers[index].Config.FloatParameterValue);
					break;
				case EMaterialParamType::Color:
					ParameterChangeHandlers[index].MaterialInstance->SetVectorParameterValue(
						ParameterChangeHandlers[index].Config.ParameterName,
						ParameterChangeHandlers[index].Config.LinearColorParameterValue);
					break;
				case EMaterialParamType::Texture:
					ParameterChangeHandlers[index].MaterialInstance->SetTextureParameterValue(
						ParameterChangeHandlers[index].Config.ParameterName,
						ParameterChangeHandlers[index].Config.TextureParameterValue);
					break;
				}
			}
		}

		if (!ParameterChangeHandlers[index].IsApplied)
		{
			ParameterChangeHandlers[index].IsApplied = true;
		}

		if (ParameterChangeHandlers[index].Config.Lifetime > 0 && ParameterChangeHandlers[index].Config.HasLifetime)
		{
			if (ParameterChangeHandlers[index].LifetimeCounter < ParameterChangeHandlers[index].Config.Lifetime)
			{
				ParameterChangeHandlers[index].LifetimeCounter += DeltaTime;
			}
			else
			{
				switch (ParameterChangeHandlers[index].Config.ParameterType)
				{
				case EMaterialParamType::Float:
					ParameterChangeHandlers[index].MaterialInstance->SetScalarParameterValue(
						ParameterChangeHandlers[index].Config.ParameterName,
						ParameterChangeHandlers[index].OldFloatValue);
					break;
				case EMaterialParamType::Color:
					ParameterChangeHandlers[index].MaterialInstance->SetVectorParameterValue(
						ParameterChangeHandlers[index].Config.ParameterName,
						ParameterChangeHandlers[index].OldLinearColorValue);
					break;
				case EMaterialParamType::Texture:
					ParameterChangeHandlers[index].MaterialInstance->SetTextureParameterValue(
						ParameterChangeHandlers[index].Config.ParameterName,
						ParameterChangeHandlers[index].OldTextureValue);
					break;
				}
				CompletedHandlerIds.Add(index);
			}
		}
		else
		{
			CompletedHandlerIds.Add(index);
		}
	}

	for (int index = CompletedHandlerIds.Num() - 1; index >= 0; --index)
	{
		ParameterChangeHandlers.RemoveAt(CompletedHandlerIds[index]);
	}
}


/** Trigger setup and run */
void UPVDMaterialEffectControllerComp::CategorizeConfigsWithEvents()
{
	for (auto Config : Configs)
	{
		if(!EventConfigMap.Contains(Config.GlobalEventType))
		{
			EventConfigMap.Add(Config.GlobalEventType);
		}
		
		EventConfigMap[Config.GlobalEventType].Configs.Add(Config);

		GES_MATERIAL_EFFECT_EVENT_CONTEXT(Config.GlobalEventType);
		FGESHandler::DefaultHandler()->AddLambdaListener(GESEventContext, [this,Config] (UObject* InTarget)
		{
			if(InTarget == GetOwner())
			{
				Run(EventConfigMap[Config.GlobalEventType].Configs);
			}
		});
	}
}

void UPVDMaterialEffectControllerComp::Run(TArray<FMaterialEffectConfig> ConfigArray)
{
	for (FMaterialEffectConfig Config : ConfigArray)
	{
		Run(Config);
	}
}

void UPVDMaterialEffectControllerComp::Run(FMaterialEffectConfig& Config)
{
	TArray<UMeshComponent*> MeshComponents = GetMeshes(Config);
	switch (Config.MaterialEffectType)
	{
	case EMaterialEffectType::OverrideMaterial:
		for (UMeshComponent* MeshComponent : MeshComponents)
		{
			SetMaterialsOfMesh(Config, Config.EffectMaterial, MeshComponent);
		}
		break;
	case EMaterialEffectType::ChangeParameters:
		for (UMeshComponent* MeshComponent : MeshComponents)
		{
			SetParametersOfMaterials(Config, MeshComponent);
		}
		break;
	default:
		throw std::logic_error("EffectType not yet implemented!");
	}
}
