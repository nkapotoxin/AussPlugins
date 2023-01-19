#pragma once

#include "CoreMinimal.h"
#include "Tickable.h"
#include "AussEvent.h"
#include "AussTicker.generated.h"


UCLASS()
class AUSSPLUGINS_API AAussTicker : public AActor, public FTickableGameObject
{
	GENERATED_BODY()

public:
	AAussTicker();
	~AAussTicker();

	virtual bool IsTickableWhenPaused() const override;
	virtual bool IsTickableInEditor() const override;
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;
	virtual UWorld* GetTickableGameObjectWorld() const override;

	UWorld* GetWorld() const override;
	AActor* SpawnActor(const FString& pawnName, FVector Location, FRotator Rotation);
	AActor* SpawnActor(UAussPawnData* pawnData);
	APawn* GetPlayerPawn();
	TMap<FString, APawn*> GetPawns();
	TMap<FString, APawn*> GetPawnsByClassName(const FString& PawnClassName);
	TMap<FString, UAussPawnData*> GetLocalPawnsData();

	void UpdateLocalPawn();
	void UpdateRemotePawn();
	void InitLocalPawn();

	FString ServerName;
	FString PawnMovementType;
	TArray<FString> RemoteServerNames;
	TArray<FString> RemotePawnIds;
	TArray<FString> LocalPawnIds;
	TMap<FString, FString> LocalPawnIdMap;
	TMap<FString, FString> RemotePawnIdMap;
	TMap<FString, APawn*> AllLocalPawns;
	TMap<FString, APawn*> AllLocalPawnController;
	TMap<FString, UAussPawnData*> RemotePawns;
	TArray<FString> NeedToCreateRemotePawns;
	TArray<FString> NeedToDestroyRemotePawns;
	TArray<FString> NeedToUpdateRemotePawns;
	bool initClean;
	int waitTicks;
	bool canTick;
};
