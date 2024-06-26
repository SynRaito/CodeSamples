// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EndMapPortal.h"
#include "PCGStructs.h"
#include "GameFramework/Actor.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "PVD/Data/PCGRoomContainer.h"
#include "MapGenerator.generated.h"

class ASafeRoom;
class URoomConnectionPoint;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMapGenerationCompletedDelegate);

UCLASS()
class PVD_API AMapGenerator : public AActor
{
	GENERATED_BODY()

public:
	AMapGenerator();

protected:
	virtual void BeginPlay() override;
	
public:
	virtual void Tick(float DeltaTime) override;

	UFUNCTION()
	void StartGeneration();
private:
	void PlaceRoom(class ARoom* Room, bool ConnectWithPortal = false , bool NextRoomPassingCondition = false);
	void PlacePlaceholderMeshesOnEntrances();
	bool IsCollisionBlocked(class ARoom* Room , FVector const FinalRootLocation , FRotator const FinalRootRotation);
	
	template <class T>
	TSubclassOf<T> PickRandomRoom(TArray<TSubclassOf<T>> RoomArray, bool RemovePickedRoomFromArray);

public:
	
	int CreateSeed() const;
	UPROPERTY()
	FMapGenerationCompletedDelegate MapGenerationCompletedHandler;
	
	int GetContinuousRandomInRange(unsigned int Min, unsigned int Max);

	UFUNCTION()
	void GenerateMap(const UPCGRoomContainer* RoomContainer, const FMapGenerationParams& MapGenerationParams);
	UFUNCTION()
	void DestroyMap();
	UFUNCTION()
	FORCEINLINE URoomEntrancePoint* GetStartRoomConnectionPoint() const { return StartRoomEntrancePoint; }
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	UPCGRoomContainer* TestRoomContainer;
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	TSubclassOf<AEndMapPortal> EndMapPortalReference;
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	TArray<ANavMeshBoundsVolume*> NavMeshBoundsVolumePool;
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	TArray<AActor*> SafeRooms;
	UPROPERTY(VisibleAnywhere)
	int CurrentNavmeshPoolIndex = 0;
	UPROPERTY(VisibleAnywhere)
	bool IsLastRoomSafeRoom = false;
	
	UPROPERTY()
	class URoomEntrancePoint* StartRoomEntrancePoint;
private:
	UPROPERTY()
	unsigned int InitialSeed;
	UPROPERTY()
	unsigned int EvaluatedSeed;
	UPROPERTY()
	FVector PortalRoomPlacementOffset{100000,100000,0};
	UPROPERTY()
	FVector PortalRoomPlacementCurrentPosition{0,0,0};
	UPROPERTY(VisibleAnywhere , Category="Map generation")
	TArray<ABattleRoom*> GeneratedBattleRooms;
	UPROPERTY()
	TArray<ASafeRoom*> GeneratedSafeRooms;
	UPROPERTY()
	TArray<APuzzleRoom*> GeneratedPuzzleRooms;
	UPROPERTY()
	TArray<APortal*> GeneratedPortals;
	UPROPERTY()
	TArray<AActor*> GeneratedDoors;
	UPROPERTY()
	ABossRoom* GeneratedBossRooms;
	UPROPERTY()
	AStartRoom* GeneratedStartRoom;
};

template <class T>
TSubclassOf<T> AMapGenerator::PickRandomRoom(TArray<TSubclassOf<T>> RoomArray, bool RemovePickedRoomFromArray = false)
{
	int RandomIndex = GetContinuousRandomInRange(0, RoomArray.Num() - 1);
	TSubclassOf<T> PickedRoom = RoomArray[RandomIndex];
	if (RemovePickedRoomFromArray)
	{
		RoomArray.RemoveAt(RandomIndex);
	}
	return PickedRoom;
}