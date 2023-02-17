#pragma once

#include "Core.h"
#include "HAL/Runnable.h"


class AUSSPLUGINS_API FSyncThread : public FRunnable
{
public:
	FSyncThread();
	~FSyncThread();

	// must be implemented func
	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;
	virtual void Exit() override;

	// magic logic
	UWorld* GetWorld();
	AActor* SpawnActor(const FString& pawnName, FVector Location, FRotator Rotation);
	AActor* SpawnActor(UAussPawnData* pawnData);
	TMap<FString, APawn*> GetPawns();
	TMap<FString, APawn*> GetPawnsByClassName(const FString& PawnClassName);
	TMap<FString, UAussPawnData*> GetLocalPawnsData();
	void UpdateLocalPawn();
	void UpdateRemotePawn();
	void InitLocalPawn();

	// main logic
	void DoSync();

public:
	FString ServerName;
	FString PawnMovementType;
	TArray<FString> RemoteServerNames;
	TArray<FString> RemotePawnIds;
	TArray<FString> LocalPawnIds;
	TMap<FString, FString> LocalPawnIdMap;
	TMap<FString, FString> RemotePawnIdMap;
	TMap<FString, APawn*> AllLocalPawns;
	TMap<FString, APawn*> AllLocalPawnController;
	TMap<FString, FAussPaData> RemotePawns;
	TArray<FString> NeedToCreateRemotePawns;
	TArray<FString> NeedToDestroyRemotePawns;
	TArray<FString> NeedToUpdateRemotePawns;
	bool initClean;
	int waitTicks;
	bool canTick;
	int FPS;
	bool isInited;
};