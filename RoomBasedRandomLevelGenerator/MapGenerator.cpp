#include "../PCG/MapGenerator.h"

#include "EnemySpawner.h"
#include "GESHandler.h"
#include "NavigationSystem.h"
#include "Portal.h"
#include "RoomExitPoint.h"
#include "RoomEntrancePoint.h"
#include "SafeRoomPortal.h"
#include "StartRoom.h"
#include "../PCG/SafeRoom.h"
#include "../PCG/BossRoom.h"
#include "../PCG/BattleRoom.h"
#include "../PCG/PuzzleRoom.h"
#include "AI/NavigationSystemBase.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "PVD/PVD.h"
#include "PVD/PVDPlayerController.h"
#include "PVD/UI/MainHUDWidget.h"
#include "PVD/UI/MinimapWidget.h"
#include "PVD/PCG/PuzzleRoomConnectionPoint.h"


class UNavigationSystemV1;
class ANavMeshBoundsVolume;
class APuzzleRoom;

AMapGenerator::AMapGenerator()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AMapGenerator::BeginPlay()
{
	Super::BeginPlay();
}

int AMapGenerator::CreateSeed() const
{
	constexpr float MINIMUM_SEED_NUMBER = 0;
	constexpr float MAXIMUM_SEED_NUMBER = 10000;
	return FMath::RandRange(MINIMUM_SEED_NUMBER, MAXIMUM_SEED_NUMBER);
}

int AMapGenerator::GetContinuousRandomInRange(unsigned int Min, unsigned int Max)
{
	if (Min >= Max)
	{
		PVD_LOG(Error, TEXT("Min Value is Greater Equal Than Max Value!"));
		return 0;
	}
	FMath::SRandInit(EvaluatedSeed);
	float SeedRandom = FMath::SRand();
	float EvaluatedRandom = SeedRandom / (1.f / (Max + 1 - Min)) + Min;
	EvaluatedSeed += 1;
	return EvaluatedRandom;
}

bool AMapGenerator::IsCollisionBlocked(ARoom* Room , FVector const FinalRootLocation , FRotator const FinalRootRotation)
{
	if (GetWorld()->EncroachingBlockingGeometry(Room, FinalRootLocation, FinalRootRotation))
	{
		// a native component is colliding, that's enough to reject spawning
		UE_LOG(LogSpawn, Log, TEXT("SpawnActor failed because of collision at the spawn location [%s] for [%s]"), *FinalRootLocation.ToString());
		return true;
	}
	return false;
}

void AMapGenerator::PlaceRoom(ARoom* Room, bool ConnectWithPortal , bool ApplyNextRoomPassingConditions)
{
#define PortalSpawnOffset 100;

	if(IsLastRoomSafeRoom)
	{
		ConnectWithPortal = true;
	}
	ARoom* RoomToConnect = Room->LastRoom;
	//if its a starter room
	if (RoomToConnect == nullptr)
	{
		Room->SetActorLocation(FVector(10000, 10000, 10000));
		StartRoomEntrancePoint = Room->EntrancePoint;
		if (const auto PlayerController = Cast<APVDPlayerController> (UGameplayStatics::GetPlayerController(GetWorld(), 0)))
		{
			if (const auto MainHUD = PlayerController->GetHUDWidget())
			{
				MainHUD->MiniMap->SpawnMap(Room , Room->GetMinimapTexture(), Room->GetActorLocation() ,Room->GetActorRotation().Yaw);
				MainHUD->InfoMap->SpawnMap(Room , Room->GetMinimapTexture(), Room->GetActorLocation(), Room->GetActorRotation().Yaw);
			}
		}
		return;
	}
	//this is for puzzle rooms and normal rooms that collide
	if(ConnectWithPortal)
	{
		PortalRoomPlacementCurrentPosition += PortalRoomPlacementOffset;
		Room->SetActorLocation(PortalRoomPlacementCurrentPosition);

		URoomConnectionPoint* PointToConnect = nullptr;
		TSubclassOf<APortal> ExitPortal;
		int Random;
		if(Cast<APuzzleRoom>(Room))
		{
			PointToConnect = Cast<URoomConnectionPoint>(RoomToConnect->GetRandomPuzzleRoomPoint(this));
			Random = GetContinuousRandomInRange(0,TestRoomContainer->PuzzleRoomPortals.Num() - 1);
			ExitPortal = TestRoomContainer->PuzzleRoomPortals[Random];			
		}
		if(PointToConnect == nullptr)
		{
			PointToConnect = Cast<URoomConnectionPoint>(RoomToConnect->GetRandomExitPoint(this));
			Random = GetContinuousRandomInRange(0,TestRoomContainer->BattleRoomPortals.Num() - 1);
			ExitPortal = TestRoomContainer->BattleRoomPortals[Random];
		}
		if(IsLastRoomSafeRoom)
		{
			Random = GetContinuousRandomInRange(0,TestRoomContainer->SafeRoomPortals.Num() - 1);
			ExitPortal = TestRoomContainer->SafeRoomPortals[Random];
		}
		if(Cast<AStartRoom>(Room->LastRoom))
		{
			ExitPortal = TestRoomContainer->StartRoomPortal;
		}
		if(PointToConnect == nullptr)
		{
			return;
		}
		
		//Set Portal Transformation
		const FVector ExitPortalSpawnLocation = PointToConnect->GetComponentLocation() - PointToConnect->GetForwardVector() * PortalSpawnOffset;
		const FRotator ExitPortalSpawnRotator = PointToConnect->GetComponentRotation();
		APortal* ExitPortalActor = Cast<APortal>(GetWorld()->SpawnActor(ExitPortal,&ExitPortalSpawnLocation,&ExitPortalSpawnRotator));
		if(IsLastRoomSafeRoom)
		{
			if(auto const SafeRoomPortal = Cast<ASafeRoomPortal>(ExitPortalActor))
			{
				int RandomIndex = GetContinuousRandomInRange(0, SafeRooms.Num() - 1);
				SafeRoomPortal->ConnectedSafeRoom = Cast<ASafeRoom>(SafeRooms[RandomIndex]);
				PointToConnect->SpawnPlaceholderMesh();
			}
		}
		ExitPortalActor->AddActorWorldRotation(FRotator{0,180,0});
		GeneratedPortals.Add(ExitPortalActor);
		//Spawn Portal Minimap Entities
		if (const auto PlayerController = Cast<APVDPlayerController> (UGameplayStatics::GetPlayerController(GetWorld(), 0)))
		{
			if (const auto MainHUD = PlayerController->GetHUDWidget())
			{
				MainHUD->MiniMap->SpawnPortal(RoomToConnect , PointToConnect->GetComponentLocation() , ExitPortalActor->GetActorRotation().Yaw);
			}
		}
		URoomEntrancePoint* EntrancePointToConnect = Room->EntrancePoint;
		TSubclassOf<APortal> EnterPortal = ExitPortal;
		APortal* EnterPortalActor = Cast<APortal>(GetWorld()->SpawnActor(EnterPortal));
		const FVector EnterPortalSpawnLocation = EntrancePointToConnect->GetComponentLocation() + EntrancePointToConnect->GetForwardVector() * PortalSpawnOffset;
		const FRotator EnterPortalSpawnRotator = EntrancePointToConnect->GetComponentRotation();
		EnterPortalActor->SetActorLocation(EnterPortalSpawnLocation);
		EnterPortalActor->SetActorRotation(EnterPortalSpawnRotator);
		
		GeneratedPortals.Add(EnterPortalActor);
		if(auto const PuzzleRoom = Cast<APuzzleRoom>(Room))
		{
			PuzzleRoom->PuzzleRoomEntrancePortalRef = EnterPortalActor;		
		}
		if(IsLastRoomSafeRoom)
		{
			ExitPortalActor->TeleportTargetPoint = Cast<ASafeRoomPortal>(ExitPortalActor)->ConnectedSafeRoom->EnterPortal->TeleportExitPoint;
			EnterPortalActor->Disable();
			EntrancePointToConnect->SpawnPlaceholderMesh();
			Cast<ASafeRoomPortal>(ExitPortalActor)->ConnectedSafeRoom->ConnectedPortals.Add(EnterPortalActor);
			ExitPortalActor->Enable();
		}
		else
		{
			ExitPortalActor->TeleportTargetPoint = EnterPortalActor->TeleportExitPoint;
			EnterPortalActor->TeleportTargetPoint = ExitPortalActor->TeleportExitPoint;
			ExitPortalActor->SetConnectedRoom(RoomToConnect);
			EnterPortalActor->SetConnectedRoom(Room);
		}
	}
	//This is the usual room spawn point
	else
	{
		URoomExitPoint* ExitPointToConnect = RoomToConnect->GetRandomAvailableExitPoint(this ,StartRoomEntrancePoint->GetComponentRotation());
		//Rotation
		if(ExitPointToConnect != nullptr)
		{
			FRotator ExitPointRotation = ExitPointToConnect->GetComponentRotation();
			FRotator EnterPointRotation = Room->EntrancePoint->GetComponentRotation();
			
			FQuat QuatBetweenExitAndEnter = FQuat::FindBetweenNormals(ExitPointRotation.Vector(), EnterPointRotation.Vector());
			float Angle = 0;
			FVector Axis;
		
			QuatBetweenExitAndEnter.ToAxisAndAngle(Axis, Angle);
		
			Angle = FMath::RadiansToDegrees(Angle);
			
			FRotator RoomRotation = Room->GetActorRotation();
		
			RoomRotation.Yaw -= Angle * Axis.Z;
			Room->SetActorRotation(RoomRotation);
			
			FVector OffsetBetweenEnterAndExit = ExitPointToConnect->GetComponentLocation() - Room->EntrancePoint->
			GetComponentLocation();

			if (const auto PlayerController = Cast<APVDPlayerController> (UGameplayStatics::GetPlayerController(GetWorld(), 0)))
			{
				if (const auto MainHUD = PlayerController->GetHUDWidget())
				{
					MainHUD->MiniMap->SpawnPortal(RoomToConnect ,ExitPointToConnect->GetComponentLocation(), ExitPointRotation.Yaw);
				}
			}
			//Location
			Room->SetActorLocation(Room->GetActorLocation() + OffsetBetweenEnterAndExit);

			
			if(IsCollisionBlocked(Room , Room->GetActorLocation(), Room->GetActorRotation()))
			{
				RoomToConnect->AvailableExitPoints.Add(ExitPointToConnect);
				PlaceRoom(Room , true , true);
				return;
			}
			
			//Placeholder Meshes
			ExitPointToConnect->SetActivationOfPlaceholderMeshes(false);
			FVector const DoorSpawnLocation = Room->EntrancePoint->GetComponentLocation();
			FRotator const DoorSpawnRotation = Room->EntrancePoint->GetComponentRotation();
			if(auto const StartRoom = Cast<AStartRoom>(Room->LastRoom))
			{
				if(ADoor* SpawnedDoor = Cast<ADoor>(GetWorld()->SpawnActor(TestRoomContainer->StartRoomPassageWay, &DoorSpawnLocation , &DoorSpawnRotation)))
				{
					SpawnedDoor->SetConnectedRoom(RoomToConnect);
					GeneratedDoors.Add(SpawnedDoor);
				}
			}
			else
			{
				if(ADoor* SpawnedDoor = Cast<ADoor>(GetWorld()->SpawnActor(TestRoomContainer->PassageWays, &DoorSpawnLocation , &DoorSpawnRotation)))
				{
					SpawnedDoor->SetConnectedRoom(RoomToConnect);
					GeneratedDoors.Add(SpawnedDoor);
				}
			}
		}
	}

	/** set minimap texture and location*/
	if (const auto PlayerController = Cast<APVDPlayerController> (UGameplayStatics::GetPlayerController(GetWorld(), 0)))
	{
		if (const auto MainHUD = PlayerController->GetHUDWidget())
		{
			MainHUD->MiniMap->SpawnMap(Room, Room->GetMinimapTexture(), Room->GetActorLocation() ,Room->GetActorRotation().Yaw);
		}
	}

	if(Cast<ABattleRoom>(Room) || Cast<AMainBossRoom>(Room))
	{
		if(auto const NavBox = Room->NavMeshContentsBox)
		{
			if (ANavMeshBoundsVolume* NewNavVolume = NavMeshBoundsVolumePool[CurrentNavmeshPoolIndex])
			{
				CurrentNavmeshPoolIndex++;

				NewNavVolume->SetActorTransform(NavBox->GetComponentTransform());
				
				if (UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
				{
					NavSystem->OnNavigationBoundsUpdated(NewNavVolume);
					DrawDebugBox(GetWorld(),NavBox->GetComponentLocation() , NavBox->GetScaledBoxExtent(), FColor::Green, false, 500, 0, 5.0f);
				}
			}
		}
	}
}

void AMapGenerator::PlacePlaceholderMeshesOnEntrances()
{
	for (auto GeneratedBattleRoom : GeneratedBattleRooms)
	{
		if(IsValid(GeneratedBattleRoom))
		{
			for (const auto Element : GeneratedBattleRoom->AvailableExitPoints)
			{
				Element->SpawnPlaceholderMesh();
			}
		}
	}
}

void AMapGenerator::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AMapGenerator::StartGeneration()
{
	FMapGenerationParams Params = FMapGenerationParams(TestRoomContainer->MapSize, TestRoomContainer->SafeRoomFreq, TestRoomContainer->PuzzleRoomFreq, CreateSeed());
	Params.HasBossRoom = TestRoomContainer->HasBossRoom;
	Params.MakeBattleRoomsUnique = TestRoomContainer->MakeBattleRoomsUnique;
	Params.MakePuzzleRoomsUnique = TestRoomContainer->MakePuzzleRoomsUnique;
	Params.MakeSafeRoomsUnique = TestRoomContainer->MakeSafeRoomsUnique;
	GenerateMap(Cast<UPCGRoomContainer>(TestRoomContainer), Params);

	MapGenerationCompletedHandler.Broadcast();
}

void AMapGenerator::GenerateMap(const UPCGRoomContainer* const RoomContainer,const FMapGenerationParams& MapGenerationParams)
{
	CurrentNavmeshPoolIndex = 0;
	if (RoomContainer == nullptr)
	{
		return;
	}

	InitialSeed = MapGenerationParams.Seed;
	EvaluatedSeed = InitialSeed;
	
	TArray<TSubclassOf<ABattleRoom>> BattleRooms = RoomContainer->BattleRooms;
	TArray<TSubclassOf<ABossRoom>> BossRooms = RoomContainer->BossRooms;
	TArray<TSubclassOf<APuzzleRoom>> PuzzleRooms = RoomContainer->PuzzleRooms;
	TArray<TSubclassOf<AStartRoom>> StartRooms = RoomContainer->StartRooms;

	const bool HasSafeRoom = MapGenerationParams.SafeRoomFrequency > 0;
	const bool HasPuzzleRoom = MapGenerationParams.PuzzleRoomFrequency > 0;
	const bool HasBossRoom = MapGenerationParams.HasBossRoom;

	int LinearRoomCount = MapGenerationParams.BattleRoomCount;

	if (HasSafeRoom)
	{
		LinearRoomCount += MapGenerationParams.BattleRoomCount / MapGenerationParams.SafeRoomFrequency;
	}

	ARoom* LastRoom = nullptr;
	ARoom* StartRoomRef = nullptr;
	int SpawnedBattleRoomCount = 0;
	bool SpawnedPuzzleRoom = false;
	IsLastRoomSafeRoom = false;

	for (int Index = 0; Index <= LinearRoomCount; Index++)
	{
		if(Index == 0)
		{
			TSubclassOf<AStartRoom> BPStartRoom = PickRandomRoom<AStartRoom>(StartRooms, MapGenerationParams.MakeSafeRoomsUnique);
			
			AStartRoom* StartRoom = Cast<AStartRoom>(GetWorld()->SpawnActor(BPStartRoom));
			PlaceRoom(StartRoom);
			LastRoom = StartRoom;
			StartRoomRef = StartRoom;
			GeneratedStartRoom = StartRoom;
			StartRoom->SafeRoomEntitySpawner->SpawnEntities();
			if(LinearRoomCount == 0)
			{
				if (Index == LinearRoomCount)
				{
					ABossRoom* BossRoom = nullptr;
					while(BossRoom == nullptr)
					{
						TSubclassOf<ABossRoom> BPBossRoom = PickRandomRoom<ABossRoom>(BossRooms);
						BossRoom = Cast<ABossRoom>(GetWorld()->SpawnActor(BPBossRoom));
					}
					if (LastRoom != nullptr)
					{
						if (BossRoom != nullptr)
						{
							LastRoom->NextRoom = BossRoom;
							BossRoom->LastRoom = LastRoom;
							GeneratedBossRooms = BossRoom;
							LastRoom = BossRoom;
						}
					}
					else
					{
						GeneratedBossRooms = BossRoom;
						LastRoom = BossRoom;
					}
				
					PlaceRoom(BossRoom);
					BossRoom->SetUpPrepRoom();
				}
			}
			continue;
		}
		
		if (HasSafeRoom && Index > 0 && !IsLastRoomSafeRoom && SpawnedBattleRoomCount % MapGenerationParams.SafeRoomFrequency == 0 && SpawnedBattleRoomCount != 0)
		{
            IsLastRoomSafeRoom = true;
		}
		else
		{
			TSubclassOf<ABattleRoom> BPBattleRoom = PickRandomRoom<ABattleRoom>(
				BattleRooms, MapGenerationParams.MakeBattleRoomsUnique);
			ABattleRoom* BattleRoom = Cast<ABattleRoom>(GetWorld()->SpawnActor(BPBattleRoom));
			
			SpawnedBattleRoomCount++;

			if (LastRoom != nullptr)
			{
				if (BattleRoom != nullptr)
				{
					LastRoom->NextRoom = BattleRoom;
					BattleRoom->LastRoom = LastRoom;
					GeneratedBattleRooms.Add(BattleRoom);
					LastRoom = BattleRoom;
				}
			}
			else
			{
				GeneratedBattleRooms.Add(BattleRoom);
				LastRoom = BattleRoom;
			}
			PlaceRoom(BattleRoom);
			BattleRoom->EnemySpawner->SpawnEnemies();
			IsLastRoomSafeRoom = false;

			if(HasPuzzleRoom)
			{
				if(MapGenerationParams.PuzzleRoomFrequency != 0 && GeneratedBattleRooms.Num() % MapGenerationParams.PuzzleRoomFrequency == 0)
				{
					SpawnedPuzzleRoom = false;
				}
				//Try to spawn a puzzle room if it has not spawned in frequency
				if(!SpawnedPuzzleRoom)
				{
					//Check if room has available puzzle points
					if(!BattleRoom->AvailablePuzzlePoints.IsEmpty())
					{
						TSubclassOf<APuzzleRoom> BPPuzzleRoom = PickRandomRoom<APuzzleRoom>(
						PuzzleRooms, MapGenerationParams.MakePuzzleRoomsUnique);
			
						APuzzleRoom* PuzzleRoom = Cast<APuzzleRoom>(GetWorld()->SpawnActor(BPPuzzleRoom));

						if (LastRoom != nullptr)
						{
							if (PuzzleRoom != nullptr)
							{
								LastRoom->SideRooms.Add(PuzzleRoom);
								GeneratedPuzzleRooms.Add(PuzzleRoom);
								PuzzleRoom->LastRoom = LastRoom;
							}
						}
						else
						{
							GeneratedPuzzleRooms.Add(PuzzleRoom);
						}
						PlaceRoom(PuzzleRoom , true);
						PuzzleRoom->EntitySpawner->SpawnEntities();
						//Reset Puzzle Room Spawn Mechanic in every frequency
						SpawnedPuzzleRoom = true;
					}
				}
			}
		}
		if (HasBossRoom)
		{
			if (Index == LinearRoomCount)
			{
				ABossRoom* BossRoom = nullptr;
				while(BossRoom == nullptr)
				{
					TSubclassOf<ABossRoom> BPBossRoom = PickRandomRoom<ABossRoom>(BossRooms);
					BossRoom = Cast<ABossRoom>(GetWorld()->SpawnActor(BPBossRoom));
				}
				if (LastRoom != nullptr)
				{
					if (BossRoom != nullptr)
					{
						LastRoom->NextRoom = BossRoom;
						BossRoom->LastRoom = LastRoom;
						GeneratedBossRooms = BossRoom;
						LastRoom = BossRoom;
					}
				}
				else
				{
					GeneratedBossRooms = BossRoom;
					LastRoom = BossRoom;
				}
				
				PlaceRoom(BossRoom);
				BossRoom->SetUpPrepRoom();
			}
		}
		
	}
	if(IsValid(StartRoomRef))
	{
		StartRoomRef->HandleMinimap();
	}
	PlacePlaceholderMeshesOnEntrances();
}

void AMapGenerator::DestroyMap()
{
	GES_EMIT_CONTEXT(this, "pvd.gameplay", "mapgenerator.destroyingmap");
	FGESHandler::DefaultHandler()->EmitEvent(GESEmitContext, this);
	
	for (auto Element : GeneratedBattleRooms)
	{
		Element->EnemySpawner->DestroyEnemies(); //Doesnt work perfectly
	}
	for (AActor* BattleRoom : GeneratedBattleRooms)
	{
		if (BattleRoom && BattleRoom->IsValidLowLevel())
		{
			BattleRoom->Destroy();
		}
	}
	for (AActor* Portal : GeneratedPortals)
	{
		if (Portal && Portal->IsValidLowLevel())
		{
			Portal->Destroy();
		}
	}
	for (APuzzleRoom* PuzzleRoom : GeneratedPuzzleRooms)
	{
		if (PuzzleRoom && PuzzleRoom->IsValidLowLevel())
		{
			PuzzleRoom->Destroy();
		}
	}
	for (ASafeRoom* SafeRoom : GeneratedSafeRooms)
	{
		if (SafeRoom && SafeRoom->IsValidLowLevel())
		{
			SafeRoom->Destroy();
		}
	}
	for (AActor* GeneratedDoor : GeneratedDoors)
	{
		if (GeneratedDoor && GeneratedDoor->IsValidLowLevel())
		{
			GeneratedDoor->Destroy();
		}
	}

	GeneratedStartRoom->Destroy();
	GeneratedBattleRooms.Empty();
	GeneratedPortals.Empty();
	GeneratedPuzzleRooms.Empty();
	GeneratedSafeRooms.Empty();
	GeneratedDoors.Empty();

	if(IsValid(GeneratedBossRooms))
	{
		GeneratedBossRooms->EntitySpawner->DestroyEntities();
		GeneratedBossRooms->BossRoomEntrancePortalRef->Destroy();
		GeneratedBossRooms->Destroy();
	}

	if(IsValid(GeneratedStartRoom))
	{
		GeneratedStartRoom->Destroy();
	}
	
	if (const auto PlayerController = Cast<APVDPlayerController> (UGameplayStatics::GetPlayerController(GetWorld(), 0)))
	{
		if (const auto MainHUD = PlayerController->GetHUDWidget())
		{
			MainHUD->MiniMap->ClearMapPanel();
			MainHUD->InfoMap->ClearMapPanel();
			MainHUD->MiniMap->SpawnHubMap();
			MainHUD->InfoMap->SpawnHubMap();
		}
	}
	//Add Desctruction of entities here.
}
