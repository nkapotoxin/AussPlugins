#pragma once

#include "CoreMinimal.h"
#include "Tickable.h"
#include "AussEvent.h"
#include "AussReplication.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameState.h"
#include "GameFramework/GameMode.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
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
	virtual void Tick(float deltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;
	virtual UWorld* GetTickableGameObjectWorld() const override;
	UWorld* GetWorld() const override;
	const AGameModeBase* GetGameMode() const;
	TSubclassOf<APlayerState> GetPlayerStateClass() const;
	TSubclassOf<APawn> GetDefaultPawnClass() const;

	AActor* SpawnActor(FRepCharacterData* pawnData);
	TMap<FString, APawn*> GetPawns();
	TMap<FString, APawn*> GetPawnsByClassName(const FString& pawnClassName);

	void InitLocalPawn();
	void UpdateRemotePawnCache();
	void UpdateLocalPawn();
	FRepCharacterData GetReplicationDataFromPawn(FString entityId, APawn* pawn);
	void UpdatePawnFromReplicationData(APawn* pawn, FRepCharacterData* pawnData);

private:
	bool initClean;
	int waitTicks;
	bool canTick;
	FString serverName;
	FString pawnMovementType;
	TArray<FString> remoteServerNames;
	TArray<FString> remotePawnIds;
	TArray<FString> localPawnIds;
	TMap<FString, FString> localPawnIdMap;
	TMap<FString, FString> remotePawnIdMap;
	TMap<FString, APawn*> allLocalPawns;
	TMap<FString, FRepCharacterData> remotePawns;
	TArray<FString> needToCreateRemotePawns;
	TArray<FString> needToDestroyRemotePawns;
	TArray<FString> needToUpdateRemotePawns;
};
