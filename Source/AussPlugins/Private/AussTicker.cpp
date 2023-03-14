#include "AussTicker.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameState.h"
#include "GameFramework/GameMode.h"
#include <Runtime/Engine/Classes/Kismet/GameplayStatics.h>
#include "AussReplication.h"
#include "Log.h"
//#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "Misc/OutputDeviceNull.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

#if WITH_EDITOR
#include "Editor.h"
#endif


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

	UE_LOG(LogAussPlugins, Warning, TEXT("Auss tick: %f ms, update remote pawn: %d ms, update local pawn: %d ms, tick total: %d ms"), deltaTime, cost1, cost2, cost3);
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
	UWorld* World = (GetOuter() != nullptr) ? GetOuter()->GetWorld() : GWorld;
	if (World == nullptr)
	{
		World = GWorld;
	}

	return World;
}

UWorld* AAussTicker::GetTickableGameObjectWorld() const
{
	return GetWorld();
}

AActor* AAussTicker::SpawnActor(FRepCharacterData* pawnData)
{
	UClass* classPtr = NULL;

	UWorld* const World = GetWorld();
	const AGameModeBase* GameMode = World ? World->GetAuthGameMode() : NULL;
	if (GameMode == NULL)
	{
		const AGameStateBase* const GameState = World ? World->GetGameState() : NULL;
		GameMode = GameState ? GameState->GetDefaultGameMode() : NULL;
	}

	if (GameMode != NULL)
	{
		classPtr = GameMode->DefaultPawnClass;
	}

	if (classPtr == NULL)
	{
		UE_LOG(LogAussPlugins, Warning, TEXT("SpawnActor null cause class not found"));
		return NULL;
	}

	return GetWorld()->SpawnActor(classPtr, &pawnData->position, &pawnData->rotation);
}

APawn* AAussTicker::GetPlayerPawn()
{
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	uint32 Index = 0;
	if (UWorld* World = GEngine->GetWorldFromContextObject(GetWorld(), EGetWorldErrorMode::LogAndReturnNull))
	{
		for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			APlayerController* PlayerController = Iterator->Get();
			UE_LOG(LogAussPlugins, Log, TEXT("GetPlayerPawn return:%s"), *PlayerController->GetPawnOrSpectator()->GetActorLocation().ToString());
			Index++;
		}
	}

	UE_LOG(LogAussPlugins, Log, TEXT("GetPlayerPawn player num:%d"), Index);
	for (FConstPawnIterator Iterator = GetWorld()->GetPawnIterator(); Iterator; ++Iterator)
	{
		APawn* Pawn = Iterator->Get();

		// Do log print
		UE_LOG(LogAussPlugins, Log, TEXT("GetPlayerPawn location:%s"), *Pawn->GetActorLocation().ToString());
		UE_LOG(LogAussPlugins, Log, TEXT("GetPlayerPawn class:%s"), *Pawn->GetClass()->GetName());
		UE_LOG(LogAussPlugins, Log, TEXT("GetPlayerPawn full name:%s"), *Pawn->GetFullName());
		UE_LOG(LogAussPlugins, Log, TEXT("GetPlayerPawn name:%s"), *Pawn->GetName());
	}

	UE_LOG(LogAussPlugins, Log, TEXT("GetPlayerPawn get num pawns:%d"), GetWorld()->GetNumPawns());
	return PlayerPawn;
}

TMap<FString, APawn*> AAussTicker::GetPawns()
{
	TMap<FString, APawn*> pawns;
	if (UWorld* World = GEngine->GetWorldFromContextObject(GetWorld(), EGetWorldErrorMode::LogAndReturnNull))
	{
		for (FConstPawnIterator Iterator = World->GetPawnIterator(); Iterator; ++Iterator)
		{
			APawn* Pawn = Iterator->Get();
			pawns.Add(serverName + Pawn->GetName(), Pawn);

			UE_LOG(LogAussPlugins, Log, TEXT("GetPawns: id:%s, position:%s"), *(serverName + Pawn->GetName()), *Pawn->GetActorLocation().ToString());
		}
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

	UE_LOG(LogAussPlugins, Warning, TEXT("InitLocalPawn with servername:%s"), *serverName);
	initClean = true;
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

	for (FString elem : remotePawnIds)
	{
		UE_LOG(LogAussPlugins, Log, TEXT("Get remote pawns finish, remote pawn id: %s"), *elem);
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
			UE_LOG(LogAussPlugins, Warning, TEXT("Need to spawn:%s"), *elem);
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

		AActor* actor;
		try
		{
			actor = SpawnActor(tmpPawnData);
			if (actor == NULL)
			{
				UE_LOG(LogAussPlugins, Warning, TEXT("UpdateRemotePawns spawn actor null, try to skip"));
				continue;
			}

			APawn* apawn = Cast<APawn>(actor);
			if (pawnMovementType == "01" || pawnMovementType == "02" || pawnMovementType == "03" || pawnMovementType == "04" || pawnMovementType == "05")
			{
				apawn->SpawnDefaultController();
				//apawn->GetController()->InitPlayerState();

				// Set movement speed
				// 1 call set movement speed 
				// FOutputDeviceNull ar;
				// apawn->CallFunctionByNameWithArguments(TEXT("SetMovementSpeedAll 600"), ar, nullptr, true);

				// 2 set max walk speed
				// ACharacter* ach = Cast<ACharacter>(actor);
				// ach->GetCharacterMovement()->MaxWalkSpeed = 600.0f;
			}

			UpdatePawnFromReplicationData(apawn, tmpPawnData);
		}
		catch (std::exception& e)
		{
			UE_LOG(LogAussPlugins, Warning, TEXT("SpawnActor servername: %s, error: %s"), *serverName, e.what());
			continue;
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
		if (pawn == NULL)
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

			if (abs(diff.X) > 1000 || abs(diff.Y) > 1000 || abs(diff.Z) > 1000)
			{
				// teleport
				(*pawn)->SetActorLocation(newLocation, false);
			}
			else if (abs(diff.X) > 1 || abs(diff.Y) > 1 || abs(diff.Z) > 1)
			{
				(*pawn)->AddMovementInput(diff, 5);
				// UAIBlueprintHelperLibrary::SimpleMoveToLocation((*pawn)->GetController(), Location);
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

			if (abs(diff.X) > 0.1 || abs(diff.Y) > 0.1 || abs(diff.Z) > 0.1)
			{
				(*pawn)->Internal_AddMovementInput(diff, 1);
			}
		}
	}

	// TODO(nkaptx): Delete pawns for remote server
}


FRepCharacterData AAussTicker::GetReplicationDataFromPawn(FString entityId, APawn* pawn)
{
	FRepCharacterData result;

	// Generate position and rotation info
	result.entityId = entityId;
	result.position = pawn->GetActorLocation();
	result.rotation = pawn->GetActorRotation();

	APlayerState* ps = pawn->GetPlayerState();
	if (ps == NULL)
	{
		// No need to update player state
		UE_LOG(LogAussPlugins, Warning, TEXT("GetReplicationDataFromPawn no need to update player state, entityId: %s"), *entityId);
		return result;
	}

	UClass* psc = ps->GetClass();

	// TODO(nkaptx): replace with reflection
	if (FProperty* Property = psc->FindPropertyByName("UserPlayerInfo"))
	{
		void* playerInfoAddress = Property->ContainerPtrToValuePtr<void>(ps);
		if (FStructProperty* StructProp = Cast<FStructProperty>(Property))
		{
			UScriptStruct* ScriptStruct = StructProp->Struct;
			for (FProperty* PropertyNew = ScriptStruct->PropertyLink; PropertyNew != NULL; PropertyNew = PropertyNew->PropertyLinkNext)
			{
				if (PropertyNew->GetName().Contains(TEXT("UserID")))
				{
					if (FStrProperty* ChildStrProp = Cast<FStrProperty>(PropertyNew))
					{
						result.playerState.userPlayerInfo.userId = ChildStrProp->GetPropertyValue_InContainer(playerInfoAddress);
					}
				}

				if (PropertyNew->GetName().Contains(TEXT("nickName")))
				{
					if (FStrProperty* ChildStrProp = Cast<FStrProperty>(PropertyNew))
					{
						result.playerState.userPlayerInfo.nickName = ChildStrProp->GetPropertyValue_InContainer(playerInfoAddress);
					}
				}

				if (PropertyNew->GetName().Contains(TEXT("UserName")))
				{
					if (FStrProperty* ChildStrProp = Cast<FStrProperty>(PropertyNew))
					{
						result.playerState.userPlayerInfo.userName = ChildStrProp->GetPropertyValue_InContainer(playerInfoAddress);
					}
				}

				if (PropertyNew->GetName().Contains(TEXT("realName")))
				{
					if (FStrProperty* ChildStrProp = Cast<FStrProperty>(PropertyNew))
					{
						result.playerState.userPlayerInfo.realName = ChildStrProp->GetPropertyValue_InContainer(playerInfoAddress);
					}
				}

				if (PropertyNew->GetName().Contains(TEXT("title")))
				{
					if (FStrProperty* ChildStrProp = Cast<FStrProperty>(PropertyNew))
					{
						result.playerState.userPlayerInfo.title = ChildStrProp->GetPropertyValue_InContainer(playerInfoAddress);
					}
				}

				if (PropertyNew->GetName().Contains(TEXT("company")))
				{
					if (FStrProperty* ChildStrProp = Cast<FStrProperty>(PropertyNew))
					{
						result.playerState.userPlayerInfo.company = ChildStrProp->GetPropertyValue_InContainer(playerInfoAddress);
					}
				}

				if (PropertyNew->GetName().Contains(TEXT("email")))
				{
					if (FStrProperty* ChildStrProp = Cast<FStrProperty>(PropertyNew))
					{
						result.playerState.userPlayerInfo.email = ChildStrProp->GetPropertyValue_InContainer(playerInfoAddress);
					}
				}

				if (PropertyNew->GetName().Contains(TEXT("phone")))
				{
					if (FStrProperty* ChildStrProp = Cast<FStrProperty>(PropertyNew))
					{
						result.playerState.userPlayerInfo.phone = ChildStrProp->GetPropertyValue_InContainer(playerInfoAddress);
					}
				}

				if (PropertyNew->GetName().Contains(TEXT("loginDate")))
				{
					if (FStrProperty* ChildStrProp = Cast<FStrProperty>(PropertyNew))
					{
						result.playerState.userPlayerInfo.loginDate = ChildStrProp->GetPropertyValue_InContainer(playerInfoAddress);
					}
				}
			}
		}
	}

	// TODO(nkaptx): replace with reflection
	if (FProperty* Property = psc->FindPropertyByName("UserHumanStyleInfo"))
	{
		void* humanStyleAddress = Property->ContainerPtrToValuePtr<void>(ps);
		if (FStructProperty* StructProp = Cast<FStructProperty>(Property))
		{
			UScriptStruct* ScriptStruct = StructProp->Struct;
			for (FProperty* PropertyNew = ScriptStruct->PropertyLink; PropertyNew != NULL; PropertyNew = PropertyNew->PropertyLinkNext)
			{
				if (PropertyNew->GetName().Contains(TEXT("Face")))
				{
					if (FIntProperty* ChildIntProp = Cast<FIntProperty>(PropertyNew))
					{
						result.playerState.userHumanStyleInfo.face = ChildIntProp->GetPropertyValue_InContainer(humanStyleAddress);
					}
				}

				if (PropertyNew->GetName().Contains(TEXT("Hair")))
				{
					if (FIntProperty* ChildIntProp = Cast<FIntProperty>(PropertyNew))
					{
						result.playerState.userHumanStyleInfo.hair = ChildIntProp->GetPropertyValue_InContainer(humanStyleAddress);
					}
				}

				if (PropertyNew->GetName().Contains(TEXT("Cloth")))
				{
					if (FIntProperty* ChildIntProp = Cast<FIntProperty>(PropertyNew))
					{
						result.playerState.userHumanStyleInfo.cloth = ChildIntProp->GetPropertyValue_InContainer(humanStyleAddress);
					}
				}
			}
		}
	}

	// Add walk speed
	ACharacter* ach = Cast<ACharacter>(pawn);
	result.walkSpeed = ach->GetCharacterMovement()->MaxWalkSpeed;

	return result;
}

void AAussTicker::UpdatePawnFromReplicationData(APawn* pawn, FRepCharacterData* pawnData)
{
	APlayerState* ps = pawn->GetPlayerState();
	if (ps == NULL)
	{
		// Need to spawn player state for remote pawn
		UE_LOG(LogAussPlugins, Warning, TEXT("UpdatePawnFromReplicationData need to spawn player state: %s"), *(pawnData->entityId));

		UWorld* const World = GetWorld();
		const AGameModeBase* GameMode = World ? World->GetAuthGameMode() : NULL;

		// If the GameMode is null, this might be a network client that's trying to
		// record a replay. Try to use the default game mode in this case
		if (GameMode == NULL)
		{
			const AGameStateBase* const GameState = World ? World->GetGameState() : NULL;
			GameMode = GameState ? GameState->GetDefaultGameMode() : NULL;
		}

		if (GameMode != NULL)
		{
			TSubclassOf<APlayerState> playerStateClassToSpawn = GameMode->PlayerStateClass;
			if (playerStateClassToSpawn.Get() == nullptr)
			{
				UE_LOG(LogAussPlugins, Warning, TEXT("The PlayerStateClass of game mode %s is null, falling back to APlayerState."), *GameMode->GetName());
				playerStateClassToSpawn = APlayerState::StaticClass();
			}

			ps = World->SpawnActor<APlayerState>(playerStateClassToSpawn);
		}

		// Set player state
		pawn->SetPlayerState(ps);
	}

	if (ps == NULL)
	{
		UE_LOG(LogAussPlugins, Warning, TEXT("UpdatePawnFromReplicationData failed, player state cannot be created, pawnId: %s"), *(pawnData->entityId));
	}

	UClass* psc = ps->GetClass();

	// TODO(nkaptx): replace with reflection
	if (FProperty* Property = psc->FindPropertyByName("UserPlayerInfo"))
	{
		void* playerInfoAddress = Property->ContainerPtrToValuePtr<void>(ps);
		if (FStructProperty* StructProp = Cast<FStructProperty>(Property))
		{
			UScriptStruct* ScriptStruct = StructProp->Struct;
			for (FProperty* PropertyNew = ScriptStruct->PropertyLink; PropertyNew != NULL; PropertyNew = PropertyNew->PropertyLinkNext)
			{
				if (PropertyNew->GetName().Contains(TEXT("UserID")))
				{
					if (FStrProperty* ChildStrProp = Cast<FStrProperty>(PropertyNew))
					{
						ChildStrProp->SetPropertyValue_InContainer(playerInfoAddress, pawnData->playerState.userPlayerInfo.userId);
					}
				}

				if (PropertyNew->GetName().Contains(TEXT("nickName")))
				{
					if (FStrProperty* ChildStrProp = Cast<FStrProperty>(PropertyNew))
					{
						ChildStrProp->SetPropertyValue_InContainer(playerInfoAddress, pawnData->playerState.userPlayerInfo.nickName);
					}
				}

				if (PropertyNew->GetName().Contains(TEXT("UserName")))
				{
					if (FStrProperty* ChildStrProp = Cast<FStrProperty>(PropertyNew))
					{
						ChildStrProp->SetPropertyValue_InContainer(playerInfoAddress, pawnData->playerState.userPlayerInfo.userName);
					}
				}

				if (PropertyNew->GetName().Contains(TEXT("realName")))
				{
					if (FStrProperty* ChildStrProp = Cast<FStrProperty>(PropertyNew))
					{
						ChildStrProp->SetPropertyValue_InContainer(playerInfoAddress, pawnData->playerState.userPlayerInfo.realName);
					}
				}

				if (PropertyNew->GetName().Contains(TEXT("title")))
				{
					if (FStrProperty* ChildStrProp = Cast<FStrProperty>(PropertyNew))
					{
						ChildStrProp->SetPropertyValue_InContainer(playerInfoAddress, pawnData->playerState.userPlayerInfo.title);
					}
				}

				if (PropertyNew->GetName().Contains(TEXT("company")))
				{
					if (FStrProperty* ChildStrProp = Cast<FStrProperty>(PropertyNew))
					{
						ChildStrProp->SetPropertyValue_InContainer(playerInfoAddress, pawnData->playerState.userPlayerInfo.company);
					}
				}

				if (PropertyNew->GetName().Contains(TEXT("email")))
				{
					if (FStrProperty* ChildStrProp = Cast<FStrProperty>(PropertyNew))
					{
						ChildStrProp->SetPropertyValue_InContainer(playerInfoAddress, pawnData->playerState.userPlayerInfo.email);
					}
				}

				if (PropertyNew->GetName().Contains(TEXT("phone")))
				{
					if (FStrProperty* ChildStrProp = Cast<FStrProperty>(PropertyNew))
					{
						ChildStrProp->SetPropertyValue_InContainer(playerInfoAddress, pawnData->playerState.userPlayerInfo.phone);
					}
				}

				if (PropertyNew->GetName().Contains(TEXT("loginDate")))
				{
					if (FStrProperty* ChildStrProp = Cast<FStrProperty>(PropertyNew))
					{
						ChildStrProp->SetPropertyValue_InContainer(playerInfoAddress, pawnData->playerState.userPlayerInfo.loginDate);
					}
				}
			}
		}
	}

	// TODO(nkaptx): replace with reflection
	if (FProperty* Property = psc->FindPropertyByName("UserHumanStyleInfo"))
	{
		void* humanStyleAddress = Property->ContainerPtrToValuePtr<void>(ps);
		if (FStructProperty* StructProp = Cast<FStructProperty>(Property))
		{
			UScriptStruct* ScriptStruct = StructProp->Struct;
			for (FProperty* PropertyNew = ScriptStruct->PropertyLink; PropertyNew != NULL; PropertyNew = PropertyNew->PropertyLinkNext)
			{
				if (PropertyNew->GetName().Contains(TEXT("Face")))
				{
					if (FIntProperty* ChildIntProp = Cast<FIntProperty>(PropertyNew))
					{
						ChildIntProp->SetPropertyValue_InContainer(humanStyleAddress, pawnData->playerState.userHumanStyleInfo.face);
					}
				}

				if (PropertyNew->GetName().Contains(TEXT("Hair")))
				{
					if (FIntProperty* ChildIntProp = Cast<FIntProperty>(PropertyNew))
					{
						ChildIntProp->SetPropertyValue_InContainer(humanStyleAddress, pawnData->playerState.userHumanStyleInfo.hair);
					}
				}

				if (PropertyNew->GetName().Contains(TEXT("Cloth")))
				{
					if (FIntProperty* ChildIntProp = Cast<FIntProperty>(PropertyNew))
					{
						ChildIntProp->SetPropertyValue_InContainer(humanStyleAddress, pawnData->playerState.userHumanStyleInfo.cloth);
					}
				}
			}
		}
	}

	// update walk speed
	ACharacter* ach = Cast<ACharacter>(pawn);
	ach->GetCharacterMovement()->MaxWalkSpeed = pawnData->walkSpeed;
}
