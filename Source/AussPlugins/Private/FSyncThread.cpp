#include "FSyncThread.h"
#include "Log.h"
#include "AussEvent.h"
#include "AussStore.h"
#include <Runtime/Engine/Classes/Kismet/GameplayStatics.h>

FSyncThread::FSyncThread()
{
#if WITH_AUSS
	UE_LOG(LogAussPlugins, Log, TEXT("FSyncThread init"));

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
	UE_LOG(LogAussPlugins, Warning, TEXT("FSyncThread ServerNames:%s"), *ServerName);
	for (FString elem : RemoteServerNames)
	{
		UE_LOG(LogAussPlugins, Warning, TEXT("FSyncThread RemoteServerNames:%s"), *elem);
	}
#else
#endif
}

FSyncThread::~FSyncThread()
{
}

bool FSyncThread::Init()
{
#if WITH_AUSS
	return true;
#else
	return false;
#endif
}

uint32 FSyncThread::Run()
{
	while (true)
	{
		FPlatformProcess::Sleep(0.1);

		try
		{
			DoSync();
		}
		catch (std::exception& e)
		{
			UE_LOG(LogAussPlugins, Warning, TEXT("FSyncThread run error:%s"), e.what());
			FPlatformProcess::Sleep(10);
		}
	}

	return 0;
}

void FSyncThread::Stop()
{
}

void FSyncThread::Exit()
{
}

UWorld* FSyncThread::GetWorld()
{
	return GWorld;
}

void FSyncThread::DoSync()
{
	float DeltaTime = FApp::GetDeltaTime();
	if (waitTicks > 0)
	{
		waitTicks--;
		return;
	}

	UClass* classPtr = AUSS_GET_REGISTEREDCLASS("AMyCharacterBase");
	if (classPtr == NULL)
	{
		UE_LOG(LogAussPlugins, Log, TEXT("FSyncThread plugin not registered"));
		return;
	}

	if (!canTick)
	{
		AllLocalPawns = GetPawns();
		for (TPair<FString, APawn*> elem : AllLocalPawns)
		{
			canTick = true;
			UE_LOG(LogAussPlugins, Warning, TEXT("FSyncThread dosync can start"));
			return;
		}

		UE_LOG(LogAussPlugins, Log, TEXT("FSyncThread dosync wait to start"));
		return;
	}

	InitLocalPawn();

	double start = FDateTime::Now().GetTimeOfDay().GetTotalMilliseconds();

	UpdateRemotePawn();

	double end = FDateTime::Now().GetTimeOfDay().GetTotalMilliseconds();
	double start2 = FDateTime::Now().GetTimeOfDay().GetTotalMilliseconds();

	// Add magic code here
	UpdateLocalPawn();

	double end2 = FDateTime::Now().GetTimeOfDay().GetTotalMilliseconds();
	int32 cost1 = end - start;
	int32 cost2 = end2 - start2;
	int32 cost3 = end2 - start;

	UE_LOG(LogAussPlugins, Warning, TEXT("FSyncThread DeltaTime:%f GetRemotePawn:%d UpdateLocalPawn:%d DoSyncTotal:%d"), DeltaTime, cost1, cost2, cost3);
}

AActor* FSyncThread::SpawnActor(const FString& pawnName, FVector Location, FRotator Rotation)
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

AActor* FSyncThread::SpawnActor(UAussPawnData* pawnData)
{
	return SpawnActor(pawnData->entityClassName, pawnData->position, pawnData->rotation);
}


TMap<FString, APawn*> FSyncThread::GetPawns()
{
	TMap<FString, APawn*> pawns;
	if (UWorld* World = GEngine->GetWorldFromContextObject(GetWorld(), EGetWorldErrorMode::LogAndReturnNull))
	{
		for (FConstPawnIterator Iterator = World->GetPawnIterator(); Iterator; ++Iterator)
		{
			APawn* Pawn = Iterator->Get();
			pawns.Add(ServerName + Pawn->GetName(), Pawn);
		}
	}

	return pawns;
}

TMap<FString, APawn*> FSyncThread::GetPawnsByClassName(const FString& PawnClassName)
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

TMap<FString, UAussPawnData*> FSyncThread::GetLocalPawnsData()
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

void FSyncThread::UpdateLocalPawn()
{
	// Cache Local Pawn Position
	AllLocalPawns = GetPawns();

	NeedToCreateRemotePawns.RemoveAll([](FString Val) {	return true; });
	for (FString elem : RemotePawnIds)
	{
		FString* value = RemotePawnIdMap.Find(elem);
		if (value == nullptr)
		{
			UE_LOG(LogAussPlugins, Warning, TEXT("Need to create:%s"), *elem);
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
	FAussPaArray localPawnDatas;
	for (TPair<FString, APawn*> elem : AllLocalPawns)
	{
		FString tmpEntityId = elem.Key;
		APawn* tmpPawn = elem.Value;
		
		FString* value = LocalPawnIdMap.Find(tmpEntityId);
		if (value == nullptr)
		{
			LocalPawnIds.Add(tmpEntityId);
			FAussPaData tmp;
			tmp.entityID = tmpEntityId;
			tmp.position = tmpPawn->GetActorLocation();
			tmp.rotation = tmpPawn->GetActorRotation();
			localPawnDatas.pawnDatas.Add(tmp);
		}
		else
		{
			if (NeedToDestroyRemotePawns.Find(*value) == INDEX_NONE)
			{
				NeedToUpdateRemotePawns.Add(*value);
			}
		}
	}

	AussStore::UpdatePawnDataNew(ServerName, localPawnDatas);

	for (FString elem : NeedToCreateRemotePawns)
	{
		FAussPaData* tmpPawnData = RemotePawns.Find(elem);
		if (tmpPawnData == nullptr)
		{
			UE_LOG(LogAussPlugins, Warning, TEXT("NeedToCreateRemotePawns with null pawn id:%s"), *elem);
			continue;
		}

		UAussPawnData* tmp = NewObject<UAussPawnData>();
		tmp->position = tmpPawnData->position;
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

			actor->SetActorTickEnabled(false);

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
		FAussPaData* tmpPawnData = RemotePawns.Find(elem);
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

		FVector Location = (*pawn)->GetActorLocation();
		FVector tmp = (*pawn)->GetActorLocation();

		Location.X = tmpPawnData->position.X;
		Location.Y = tmpPawnData->position.Y;
		Location.Z = tmpPawnData->position.Z;

		FRotator Rotator = (*pawn)->GetActorRotation();
		Rotator.Pitch = tmpPawnData->rotation.Pitch;
		Rotator.Yaw = tmpPawnData->rotation.Yaw;
		Rotator.Roll = tmpPawnData->rotation.Roll;

		UE_LOG(LogAussPlugins, Log, TEXT("NeedToUpdateRemotePawns localpawn:%s, position:%s"), **localPawnId, *Location.ToString());

		// Do update
		// PawnMovementType defines different implementations
		if (PawnMovementType == "1")
		{
			// emulation sync
			(*pawn)->SetActorRotation(Rotator);
			tmp = Location - tmp;
			if (abs(tmp.X) > 0.1 || abs(tmp.Y) > 0.1 || abs(tmp.Z) > 0.1)
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
		else if (PawnMovementType == "3")
		{
			// random move
			float dx = FMath::RandRange(-1.0f, 1.0f);
			float dy = FMath::RandRange(-1.0f, 1.0f);
			float dz = FMath::RandRange(-1.0f, 1.0f);

			(*pawn)->AddMovementInput(FVector(dx, dy, dz), 1);
		}
	}

	// TODO remove logout pawn
}


void FSyncThread::UpdateRemotePawn()
{
	RemotePawnIds.RemoveAll([](FString Val) { return true; });
	for (FString elem : RemoteServerNames)
	{
		UE_LOG(LogAussPlugins, Log, TEXT("Update RemotePawns with servername:%s"), *elem);
		FAussPaArray remotePawnDatas = AussStore::GetRemotePawnDataNew(elem);
		for (FAussPaData tmpElem : remotePawnDatas.pawnDatas)
		{
			FString tmpRemotePawnId = tmpElem.entityID;
			RemotePawnIds.Add(tmpRemotePawnId);
			RemotePawns.Add(tmpRemotePawnId, tmpElem);
		}
	}
}

void FSyncThread::InitLocalPawn()
{
	if (initClean)
	{
		return;
	}

	UE_LOG(LogAussPlugins, Log, TEXT("InitLocalPawn with servername:%s"), *ServerName);
	AussStore::InitPawnData(ServerName);
	initClean = true;
}
