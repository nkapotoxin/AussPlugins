#include "AussTicker.h"
#include "AussReplication.h"
#include "EngineUtils.h"
#include "Misc/OutputDeviceNull.h"
#include "Log.h"


AAussTicker::AAussTicker()
{
#if WITH_AUSS
	UE_LOG(LogAussPlugins, Log, TEXT("AAussTicker init"));

	waitTicks = 0;
	pawnMovementType = "1";
	canTick = false;

	if (GConfig)
	{
		FString remoteServerNamesStr;
		GConfig->GetString(TEXT("Auss"), TEXT("ServerName"), serverName, GGameIni);
		GConfig->GetString(TEXT("Auss"), TEXT("RemoteServerNames"), remoteServerNamesStr, GGameIni);

		// Split remote server names
		remoteServerNamesStr.ParseIntoArray(remoteServerNames, TEXT(";"), true);
		GConfig->GetString(TEXT("Auss"), TEXT("PawnMovementType"), pawnMovementType, GGameIni);
		GConfig->GetInt(TEXT("Auss"), TEXT("WaitTicks"), waitTicks, GGameIni);
	}

	// Display serverNameand Remote Server Names
	UE_LOG(LogAussPlugins, Warning, TEXT("ServerNames: %s"), *serverName);
	for (FString elem : remoteServerNames)
	{
		UE_LOG(LogAussPlugins, Warning, TEXT("RemoteServerNames: %s"), *elem);
	}


#else
#endif
}

AAussTicker::~AAussTicker()
{
}

void AAussTicker::Tick(float deltaTime)
{
#if WITH_AUSS
	if (waitTicks > 0)
	{
		waitTicks--;
		return;
	}

	if (!canTick)
	{
		allLocalPawns = GetPawns();
		for (TPair<FString, APawn*> elem : allLocalPawns)
		{
			canTick = true;
			UE_LOG(LogAussPlugins, Warning, TEXT("AussTicker tick can start"));
			return;
		}

		UE_LOG(LogAussPlugins, Log, TEXT("AussTicker tick wait to tick"));
		return;
	}

	try
	{
		InitLocalPawn();

		double start = FDateTime::Now().GetTimeOfDay().GetTotalMilliseconds();
		UpdateRemotePawnCache();

		double end = FDateTime::Now().GetTimeOfDay().GetTotalMilliseconds();
		double start2 = FDateTime::Now().GetTimeOfDay().GetTotalMilliseconds();
		UpdateLocalPawn();

		double end2 = FDateTime::Now().GetTimeOfDay().GetTotalMilliseconds();
		int32 cost1 = end - start;
		int32 cost2 = end2 - start2;
		int32 cost3 = end2 - start;

		UE_LOG(LogAussPlugins, Log, TEXT("Auss tick: %f ms, update remote pawn: %d ms, update local pawn: %d ms, tick total: %d ms"),
			deltaTime, cost1, cost2, cost3);
		if (cost3 > 33)
		{
			UE_LOG(LogAussPlugins, Warning, TEXT("Auss tick cost too long, deltaTime: %f ms, update remote pawn: %d ms, update local pawn: %d ms, tick total: %d ms"),
				deltaTime, cost1, cost2, cost3);
		}
	}
	catch (std::exception& e)
	{
		UE_LOG(LogAussPlugins, Warning, TEXT("Auss tick error: %s"), e.what());
	}
#else
#endif
}

bool AAussTicker::IsTickable() const
{
#if WITH_AUSS
	return true;
#else
	return false;
#endif			
}

TStatId AAussTicker::GetStatId() const
{
	return TStatId();
}

bool AAussTicker::IsTickableWhenPaused() const
{
	return false;
}

bool AAussTicker::IsTickableInEditor() const
{
	return false;
}

UWorld* AAussTicker::GetWorld() const
{
	UWorld* world = (GetOuter() != nullptr) ? GetOuter()->GetWorld() : GWorld;
	if (world == nullptr)
	{
		world = GWorld;
	}

	return world;
}

const AGameModeBase* AAussTicker::GetGameMode() const
{
	UWorld* const world = GetWorld();
	const AGameModeBase* gameMode = world ? world->GetAuthGameMode() : nullptr;
	if (gameMode == nullptr)
	{
		const AGameStateBase* const gameState = world ? world->GetGameState() : nullptr;
		gameMode = gameState ? gameState->GetDefaultGameMode() : nullptr;
	}

	return gameMode;
}

TSubclassOf<APlayerState> AAussTicker::GetPlayerStateClass() const
{
	const AGameModeBase* gameMode = GetGameMode();
	return gameMode ? gameMode->PlayerStateClass : APlayerState::StaticClass();
}

TSubclassOf<APawn> AAussTicker::GetDefaultPawnClass() const
{
	const AGameModeBase* gameMode = GetGameMode();
	return gameMode ? gameMode->DefaultPawnClass : NULL;
}

UWorld* AAussTicker::GetTickableGameObjectWorld() const
{
	return GetWorld();
}

AActor* AAussTicker::SpawnActor(FRepCharacterData* pawnData)
{
	UClass* classPtr = GetDefaultPawnClass();
	if (classPtr == nullptr)
	{
		UE_LOG(LogAussPlugins, Warning, TEXT("SpawnActor null cause default pawn class not found"));
		return nullptr;
	}

	return GetWorld()->SpawnActor(classPtr, &pawnData->position, &pawnData->rotation);
}

TMap<FString, APawn*> AAussTicker::GetPawns()
{
	TMap<FString, APawn*> pawns;
	for (TActorIterator<APawn> iterator(GetWorld()); iterator; ++iterator)
	{
		pawns.Add(serverName + iterator->GetName(), *iterator);
	}

	return pawns;
}

TMap<FString, APawn*> AAussTicker::GetPawnsByClassName(const FString& pawnClassName)
{
	TMap<FString, APawn*> pawns;
	for (TPair<FString, APawn*> elem : GetPawns())
	{
		if (elem.Value->GetClass()->GetName() == pawnClassName)
		{
			pawns.Add(elem.Key, elem.Value);
		}
	}

	return pawns;
}

void AAussTicker::InitLocalPawn()
{
	if (initClean)
	{
		return;
	}

	AussReplication::InitServerData(serverName);

	UE_LOG(LogAussPlugins, Warning, TEXT("InitLocalPawn with servername: %s"), *serverName);
	initClean = true;

	// add magic code here
	UClass* pcn = GetPlayerStateClass();
	playerStateLayout = FAussLayout::CreateFromClass(pcn);
}

void AAussTicker::UpdateRemotePawnCache()
{
	remotePawnIds.RemoveAll([](FString Val) { return true; });
	for (FString elem : remoteServerNames)
	{
		UE_LOG(LogAussPlugins, Log, TEXT("Update remote pawns with servername: %s"), *elem);
		FRepCharacterReplicationData characterReplicationData = AussReplication::GetRemoteServerData(elem);

		for (FRepCharacterData data : characterReplicationData.characterDatas)
		{
			remotePawnIds.Add(data.entityId);
			remotePawns.Add(data.entityId, data);
		}
	}
}

void AAussTicker::UpdateLocalPawn()
{
	// Cache local pawn data
	allLocalPawns = GetPawns();

	needToCreateRemotePawns.RemoveAll([](FString Val) {	return true; });
	for (FString elem : remotePawnIds)
	{
		FString* value = remotePawnIdMap.Find(elem);
		if (value == nullptr)
		{
			UE_LOG(LogAussPlugins, Warning, TEXT("Need to spawn: %s"), *elem);
			needToCreateRemotePawns.Add(elem);
		}
	}

	for (TPair<FString, FString> elem : remotePawnIdMap)
	{
		FString tmpRemotePawnId = elem.Key;
		if (remotePawnIds.Find(tmpRemotePawnId) == INDEX_NONE)
		{
			needToDestroyRemotePawns.Add(tmpRemotePawnId);
		}
	}

	localPawnIds.RemoveAll([](FString Val) { return true; });
	needToUpdateRemotePawns.RemoveAll([](FString Val) { return true; });

	FRepCharacterReplicationData localPawnDatas;
	for (TPair<FString, APawn*> elem : allLocalPawns)
	{
		FString tmpEntityId = elem.Key;
		APawn* tmpPawn = elem.Value;

		FString* value = localPawnIdMap.Find(tmpEntityId);
		if (value != nullptr)
		{
			if (needToDestroyRemotePawns.Find(*value) == INDEX_NONE)
			{
				needToUpdateRemotePawns.Add(*value);
			}
		}
		else
		{
			localPawnIds.Add(tmpEntityId);
			FRepCharacterData tmp = GetReplicationDataFromPawn(tmpEntityId, tmpPawn);
			localPawnDatas.characterDatas.Add(tmp);
		}
	}

	// Update local pawn data to server
	AussReplication::UpdateDataToServer(serverName, localPawnDatas);

	// Create pawns for remote server
	for (FString elem : needToCreateRemotePawns)
	{
		FRepCharacterData* tmpPawnData = remotePawns.Find(elem);
		if (tmpPawnData == nullptr)
		{
			UE_LOG(LogAussPlugins, Warning, TEXT("NeedToCreateRemotePawns with null pawn, id: %s"), *elem);
			continue;
		}

		AActor* actor = SpawnActor(tmpPawnData);
		if (actor == NULL)
		{
			UE_LOG(LogAussPlugins, Warning, TEXT("UpdateRemotePawns spawn actor null, id: %s"), *elem);
			continue;
		}

		if (APawn* apawn = Cast<APawn>(actor))
		{
			// pawn movement type 01/02/03/04/05 with controller and player state
			if (pawnMovementType == "01" || pawnMovementType == "02" || pawnMovementType == "03" || 
				pawnMovementType == "04" || pawnMovementType == "05")
			{
				apawn->SpawnDefaultController();
				apawn->GetController()->InitPlayerState();
			}

			UpdatePawnFromReplicationData(apawn, tmpPawnData);
		}

		localPawnIdMap.Add(serverName + actor->GetName(), elem);
		remotePawnIdMap.Add(elem, serverName + actor->GetName());
	}

	// Update pawns for remote server
	for (FString elem : needToUpdateRemotePawns)
	{
		FRepCharacterData* tmpPawnData = remotePawns.Find(elem);
		if (tmpPawnData == nullptr)
		{
			UE_LOG(LogAussPlugins, Warning, TEXT("NeedToUpdateRemotePawns with null pawn id: %s"), *elem);
			continue;
		}

		FString* localPawnId = remotePawnIdMap.Find(elem);
		if (localPawnId == nullptr)
		{
			UE_LOG(LogAussPlugins, Warning, TEXT("NeedToUpdateRemotePawns with null local pawn id: %s"), *elem);
			continue;
		}

		APawn** pawn = allLocalPawns.Find(*localPawnId);
		if (pawn == nullptr)
		{
			UE_LOG(LogAussPlugins, Warning, TEXT("NeedToUpdateRemotePawns can not find local pawns, local pawn id: %s"), **localPawnId);
			continue;
		}

		FVector oldLocation = (*pawn)->GetActorLocation();
		FVector newLocation = tmpPawnData->position;
		FRotator newRotator = tmpPawnData->rotation;

		UE_LOG(LogAussPlugins, Log, TEXT("NeedToUpdateRemotePawns localpawn: %s, position: %s"), **localPawnId, *newLocation.ToString());

		// PawnMovementType defines different implementations
		if (pawnMovementType == "1" || pawnMovementType == "01")
		{
			// emulation sync
			(*pawn)->SetActorRotation(newRotator);
			FVector diff = newLocation - oldLocation;

			if (abs(diff.X) > 500 || abs(diff.Y) > 500 || abs(diff.Z) > 500)
			{
				// teleport
				(*pawn)->SetActorLocation(newLocation, false);
			}
			else if (abs(diff.X) > 5 || abs(diff.Y) > 5 || abs(diff.Z) > 5)
			{
				(*pawn)->AddMovementInput(diff, 5);
			}

			UpdatePawnFromReplicationData(*pawn, tmpPawnData);
		}
		else if (pawnMovementType == "2" || pawnMovementType == "02")
		{
			// just sync position
			(*pawn)->SetActorLocation(newLocation, true);
			(*pawn)->SetActorRotation(newRotator);
		}
		else if (pawnMovementType == "3" || pawnMovementType == "03")
		{
			// random move
			float dx = FMath::RandRange(-1.0f, 1.0f);
			float dy = FMath::RandRange(-1.0f, 1.0f);
			float dz = FMath::RandRange(-1.0f, 1.0f);

			(*pawn)->AddMovementInput(FVector(dx, dy, dz), 1);
		}
		else if (pawnMovementType == "4" || pawnMovementType == "04")
		{
			// random move
			float dx = FMath::RandRange(-1.0f, 1.0f);
			float dy = FMath::RandRange(-1.0f, 1.0f);
			float dz = FMath::RandRange(-1.0f, 1.0f);

			(*pawn)->Internal_AddMovementInput(FVector(dx, dy, dz), true);
		}
		else if (pawnMovementType == "5" || pawnMovementType == "05")
		{
			(*pawn)->SetActorRotation(newRotator);
			FVector diff = newLocation - oldLocation;

			if (abs(diff.X) > 5 || abs(diff.Y) > 5 || abs(diff.Z) > 5)
			{
				(*pawn)->Internal_AddMovementInput(diff, true);
			}
		}
	}

	// TODO(nkaptx): Delete pawns for remote server
}


void AAussTicker::GetPawnRepData(FString entityId, APawn* pawn, FRepCharacterData* rcd)
{
	UAussChannel** channelPtr = allChannels.Find(entityId);
	if (channelPtr == nullptr || *channelPtr == nullptr)
	{
		// create channel
		UAussChannel* tmp = NewObject<UAussChannel>();
		FAussLayoutHelper* InRepLayoutHelper = new FAussLayoutHelper();

		// setup
		tmp->SetLayoutHelper(InRepLayoutHelper);
		tmp->SetChannelActor(pawn->GetPlayerState());
		allChannels.Add(entityId, tmp);

		channelPtr = &tmp;
	}

	if (channelPtr != nullptr && *channelPtr != nullptr)
	{
		UAussChannel* ch = *channelPtr;
		ch->ReplicateActor(&rcd->dynamicProperties);
	}
	else
	{
		UE_LOG(LogAussPlugins, Warning, TEXT("GetPawnRepData with null channel, entityId: %s"), *entityId);
	}

	//// add magic code here
	//const FConstAussObjectDataBuffer sourceData = pawn->GetPlayerState();
	//FDataStoreWriter writer( &rcd->dynamicProperties );

	//playerStateLayout->SendProperties(writer, sourceData);
}

void AAussTicker::UpdatePawnRepData(APawn* pawn, FRepCharacterData* rcd)
{
	// add magic code here
	const FConstAussObjectDataBuffer sourceData = pawn->GetPlayerState();
	FDataStoreReader reader( &rcd->dynamicProperties );

	playerStateLayout->ReceiveProperties(reader, sourceData);
}

FRepCharacterData AAussTicker::GetReplicationDataFromPawn(FString entityId, APawn* pawn)
{
	FRepCharacterData result;

	// Generate position and rotation info
	result.entityId = entityId;
	result.position = pawn->GetActorLocation();
	result.rotation = pawn->GetActorRotation();

	APlayerState* ps = pawn->GetPlayerState();
	if (ps == nullptr)
	{
		// No need to update player state
		UE_LOG(LogAussPlugins, Warning, TEXT("GetReplicationDataFromPawn no need to update player state, entityId: %s"), *entityId);
		return result;
	}

	GetPawnRepData(entityId, pawn, &result);

	// Add walk speed
	if (ACharacter* ach = Cast<ACharacter>(pawn))
	{
		result.walkSpeed = ach->GetCharacterMovement()->MaxWalkSpeed;
	}

	return result;
}

void AAussTicker::UpdatePawnFromReplicationData(APawn* pawn, FRepCharacterData* pawnData)
{
	APlayerState* ps = pawn->GetPlayerState();
	if (ps == NULL)
	{
		// Need to spawn player state for remote pawn first time
		UE_LOG(LogAussPlugins, Warning, TEXT("UpdatePawnFromReplicationData need to spawn player state: %s"), *(pawnData->entityId));

		TSubclassOf<APlayerState> playerStateClassToSpawn = GetPlayerStateClass();
		if (playerStateClassToSpawn != nullptr)
		{
			ps = GetWorld()->SpawnActor<APlayerState>(playerStateClassToSpawn);

			// Set player state first time
			pawn->SetPlayerState(ps);
		}
	}

	if (ps == NULL)
	{
		UE_LOG(LogAussPlugins, Warning, TEXT("UpdatePawnFromReplicationData failed, player state cannot be spawned, pawnId: %s"), 
			*(pawnData->entityId));
		return;
	}

	UpdatePawnRepData(pawn, pawnData);

	// update walk speed
	if (ACharacter* ach = Cast<ACharacter>(pawn))
	{
		ach->GetCharacterMovement()->MaxWalkSpeed = pawnData->walkSpeed;
	}
}
