#include "AussTicker.h"
#include "Log.h"
#include "AussEvent.h"
#include "AussStore.h"
#include <Runtime/Engine/Classes/Kismet/GameplayStatics.h>

//#include "Blueprint/AIBlueprintHelperLibrary.h"

#if WITH_EDITOR
#include "Editor.h"
#endif


AAussTicker::AAussTicker()
{
#if WITH_AUSS
	UE_LOG(LogAussPlugins, Log, TEXT("AAussTicker init"));

	waitTicks = 0;
	PawnMovementType = "1";
	canTick = false;
	
	if (GConfig)
	{
		FString remoteServerNames;
		GConfig->GetString(TEXT("Auss"), TEXT("ServerName"), ServerName, GGameIni);
		GConfig->GetString(TEXT("Auss"), TEXT("RemoteServerNames"), remoteServerNames, GGameIni);

		// Split remote server names
		remoteServerNames.ParseIntoArray(RemoteServerNames, TEXT(";"), true);
		GConfig->GetString(TEXT("Auss"), TEXT("PawnMovementType"), PawnMovementType, GGameIni);
		GConfig->GetInt(TEXT("Auss"), TEXT("WaitTicks"), waitTicks, GGameIni);
	}

	// Display ServerName and Remote Server Names
	UE_LOG(LogAussPlugins, Warning, TEXT("ServerNames:%s"), *ServerName);
	for (FString elem : RemoteServerNames)
	{
		UE_LOG(LogAussPlugins, Warning, TEXT("RemoteServerNames:%s"), *elem);
	}

	UE_LOG(LogAussPlugins, Warning, TEXT("InitLocalPawn with servername:%s"), *ServerName);
	AUssStore::InitPawnData(ServerName);
	initClean = true;
	//PrimaryActorTick.TickInterval = 1.0f;
#else
#endif
}

AAussTicker::~AAussTicker()
{
}

void AAussTicker::Tick(float DeltaTime)
{
#if WITH_AUSS
	if (waitTicks > 0)
	{
		waitTicks--;
		return;
	}

	if (!canTick) 
	{
		AllLocalPawns = GetPawns();
		for (TPair<FString, APawn*> elem : AllLocalPawns)
		{
			canTick = true;
			UE_LOG(LogAussPlugins, Log, TEXT("AussTicker tick can start"));
			return;
		}

		UE_LOG(LogAussPlugins, Log, TEXT("AussTIcker tick wait to tick"));
		return;
	}

	// InitLocalPawn();
	double start = FDateTime::Now().GetTimeOfDay().GetTotalMilliseconds();
	
	UpdateRemotePawn();
	
	double end = FDateTime::Now().GetTimeOfDay().GetTotalMilliseconds();
	double start2 = FDateTime::Now().GetTimeOfDay().GetTotalMilliseconds();

	// Add magci code here
	UpdateLocalPawn();

	double end2 = FDateTime::Now().GetTimeOfDay().GetTotalMilliseconds();
	int32 cost1 = end - start;
	int32 cost2 = end2 - start2;
	int32 cost3 = end2 - start;

	UE_LOG(LogAussPlugins, Log, TEXT("UpdateRemote %f %d UpdateLocal %d Toal %d"), DeltaTime, cost1, cost2, cost3);
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

AActor* AAussTicker::SpawnActor(const FString& pawnName, FVector Location, FRotator Rotation)
{
	UClass* classPtr = AUSS_GET_REGISTEREDCLASS(pawnName);
	if (classPtr == NULL)
	{
		UE_LOG(LogAussPlugins, Warning, TEXT("SpawnActor null cause class not found:%s"), *pawnName);
		return NULL;
	}

	UE_LOG(LogAussPlugins, Warning, TEXT("SpawnActor find the class:%s"), *pawnName);
	return GetWorld()->SpawnActor(classPtr, &Location, &Rotation);
}

AActor* AAussTicker::SpawnActor(UAussPawnData* pawnData)
{
	FString pawnName = pawnData->entityClassName;
	FVector Location = pawnData->position;
	FRotator Rotation = pawnData->rotation;
	
	UE_LOG(LogAussPlugins, Log, TEXT("SpawnActor entityClassName: %s"), *pawnName);
	UE_LOG(LogAussPlugins, Log, TEXT("SpawnActor entityID: %s"), *pawnData->entityID);
	UE_LOG(LogAussPlugins, Log, TEXT("SpawnActor remote entityID: %s"), *pawnData->remoteEntityID);
	return SpawnActor(pawnName, Location, Rotation);
}

APawn* AAussTicker::GetPlayerPawn()
{
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	unit32 Index = 0;
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
			pawns.Add(ServerName + Pawn->GetName(), Pawn);

			UE_LOG(LogAussPlugins, Log, TEXT("GetPawns: id:%s, position:%s"), *(ServerName + Pawn->GetName()), *Pawn->GetActorLocation().ToString());
		}
	}

	return pawns;
}

TMap<FString, APawn*> AAussTicker::GetPawnsByClassName(const FString& PawnClassName)
{
	TMap<FString, APawn*> pawns;
	for (TPair<FString, APawn*> elem : GetPawns())
	{
		if (elem.Value->GetClass()->GetName() == PawnClassName)
		{
			pawns.Add(elem.Key, elem.Value);
		}
	}

	return pawns;
}

TMap<FString, UAussPawnData*> AAussTicker::GetLocalPawnsData()
{
	TMap<FString, UAussPawnData*> pawnsData;
	for (TPair<FString, APawn*> elem : GetPawns())
	{
		UAussPawnData* tmpData = NewObject<UAussPawnData>();
		tmpData->entityClassName = elem.Value->GetClass()->GetName();
		tmpData->entityID = elem.Value->GetName();
		tmpData->rotation = elem.Value->GetActorRotation();
		tmpData->position = elem.Value->GetActorLocation();
		tmpData->scale = elem.Value->GetActorScale();

		pawnsData.Add(tmpData->entityID, tmpData);
	}

	return pawnsData;
}

void AAussTicker::UpdateLocalPawn()
{
	// Cache Local Pawn Position
	AllLocalPawns = GetPawns();

	NeedToCreateRemotePawns.RemoveAll([](FString Val) {	return true; });
	for (FString elem : RemotePawnIds)
	{
		FString* value = RemotePawnIdMap.Find(elem);
		if (value = nullptr)
		{
			UE_LOG(LogAussPlugins, Warning，TEXT("Need to create:%s"), *elem);
			NeedToCreateRemotePawns.Add(elem);
		}
	}

	for (TPair<FString, FString> elem : RemotePawnIdMap)
	{
		FString tmpRemotePawnId = elem.Key;
		if (RemotePawnIds.Find(tmpRemotePawnId) == INDEX_NONE)
		{
			NeedToDestroyRemotePawns.Add(tmpRemotePawnId);
		}
	}

	LocalPawnIds.RemoveAll([](FString Val) { return true; });
	NeedToUpdateRemotePawns.RemoveAll([](FString Val) { return true; });
	TArray<UAussPawnData*> localPawnDatas;
	for (TPair<FString, APawn*> elem : AllLocalPawns)
	{
		FString tmpEntityId = elem.Key;
		APawn* tmpPawn = elem.Value;
		
		FString* value = LocalPawnIdMap.Find(tmpEntityId);
		if (value = nullptr)
		{
			LocalPawnIds.Add(tmpEntityId);
			UAussPawnData* tmp = NewObject<UAussPawnData>();
			tmp->entityID = tmpEntityId;
			tmp->position = tmpPawn->GetActorLocation();
			tmp->rotation = tmpPawn->GetActorRotation();
			localPawnDatas.Add(tmp);
		}
		else
		{
			if (NeedToDestroyRemotePawns.Find(*value) == INDEX_NONE)
			{
				NeedToUpdateRemotePawns.Add(*value);
			}
		}
	}

	AussStore::UpdatePawnData(ServerName, localPawnDatas);

	for (FString elem : NeedToCreateRemotePawns)
	{
		UAussPawnData** tmpPawnData = RemotePawns.Find(elem);
		if (tmpPawnData == nullptr)
		{
			UE_LOG(LogAussPlugins, Warning, TEXT("NeedToCreateRemotePawns with null pawn id:%s"), *elem);
			continue;
		}

		UAussPawnData* tmp = NewObject<UAussPawnData>();
		tmp->position = (*tmpPawnData)->position;
		tmp->entityClassName = "AMyCharacterBase";
		AActor* actor;

		try
		{
			actor = SpawnActor(tmp);
			if (actor == NULL)
			{
				UE_LOG(LogAussPlugins, Warning, TEXT("UpdateRemotePawns spawn actor null, try to skip"));
				continue;
			}

			APawn* apawn = Cast<APawn>(actor);
			apawn->SpawnDefaultController();
		}
		catch (std::exception& e)
		{
			UE_LOG(LogAussPlugins, Warning, TEXT("SpawnActor servername:%s, error:%s"), *ServerName, e.what());
			continue;
		}

		LocalPawnIdMap.Add(ServerName + actor->GetName(), elem);
		RemotePawnIdMap.Add(elem, ServerName + actor->GetName());
	}

	for (FString elem : NeedToUpdateRemotePawns)
	{
		UAussPawnData** tmpPawnData = RemotePawns.Find(elem);
		if (tmpPawnData == nullptr)
		{
			UE_LOG(LogAussPlugins, Warning, TEXT("NeedToUpdateRemotePawns with null pawn id:%s"), *elem);
			continue;
		}

		FString* localPawnId = RemotePawnIdMap.Find(elem);
		if (localPawnId == nullptr)
		{
			UE_LOG(LogAussPlugins, Warning, TEXT("NeedToUpdateRemotePawns with null local pawn id:%s"), *elem);
			continue;
		}

		APawn** pawn = AllLocalPawns.Find(*localPawnId);
		if (pawn == NULL)
		{
			UE_LOG(LogAussPlugins, Warning, TEXT("NeedToUpdateRemotePawns can not find local pawns, local pawn id:%s"), **localPawnId);
			continue;
		}

		FVector Location == (*pawn)->GetActorLocation();
		FVector tmp = (*pawn)->GetActorLocation();

		Location.X = (*tmpPawnData)->position.X;
		Location.Y = (*tmpPawnData)->position.Y;
		Location.Z = (*tmpPawnData)->position.Z;

		FRotator Rotator = (*pawn)->GetActorRotation();
		Rotator.Pitch = (*tmpPawnData)->rotation.Pitch;
		Rotator.Yaw = (*tmpPawnData)->rotation.Yaw;
		Rotator.Roll = (*tmpPawnData)->rotation.Roll;

		UE_LOG(LogAussPlugins, Log, TEXT("NeedToUpdateRemotePawns localpawn:%s, position:%s"), **localPawnId, *Location.ToString());

		// Do update
		// PawnMovementTyupe defines different implementations
		if (PawnMovementType == "1")
		{
			// emulation sync
			(*pawn)->SetActorRotation(Rotator);
			tmp = Location - tmp;
			if (tmp.X > 0.0001 || tmp.Y > 0.0001 || tmp.Z > 0.0001)
			{
				(*pawn)->AddMovementInput(tmp, 1);
			}

			// UAIBlueprintHelperLibrary::SimpleMoveToLocation((*pawn)->GetController(), Location);
		}
		else if (PawnMovementType == "2")
		{
			// just sync position
			(*pawn)->SetActorLocation(Location, true);
			(*pawn)->SetActorRotation(Rotator);
		}
	}

	// TODO remove logout pawn
}

void AAussTicker::UpdateRemotePawn()
{
	RemotePawnIds.RemoveAll([](FString Val) { return true; });
	for (FString elem : RemoteServerNames)
	{
		UE_LOG(LogAussPlugins, Log, TEXT("Update RemotePawns with servername:%s"), *elem);
		TMap<FString, UAussPawnData*> remotePawnDatas = AussStore::GetRemotePawnData(elem);
		for (TPair<FSTring, UAussPawnData*> tmpElem : remotePawnDatas)
		{
			FString tmpRemotePawnId = tmpElem.Key;
			UAussPawnData* tmpRemotePawnData = tmpElem.Value;
			RemotePawnIds.Add(tmpRemotePawnId);
			RemotePawns.Add(tmpRemotePawnId, tmpRemotePawnData);
		}
	}

	for (FString elem : RemotePawnIds)
	{
		UE_LOG(LogAussPlugins, Log, TEXT("Update RemotePawns remoteIds:%s"), *elem);
	}
}

void AAussTicker::InitLocalPawn()
{
	if (initClean)
	{
		return;
	}

	UE_LOG(LogAussPlugins, Log, TEXT("InitLocalPawn with servername:%s"), *ServerName);
	AussStore::InitPawnData(ServerName);
	initClean = true;
}
