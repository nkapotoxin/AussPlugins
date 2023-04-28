#pragma once

#include "UObject/UnrealType.h"
#include "AussReplication.generated.h"


USTRUCT()
struct AUSSPLUGINS_API FRepCharacterData
{
	GENERATED_BODY()
	UPROPERTY()
	FString entityId;
	UPROPERTY()
	FVector position;
	UPROPERTY()
	FRotator rotation;
	UPROPERTY()
	TMap<int32, FString> dynamicProperties;
	UPROPERTY()
	float walkSpeed;
};

USTRUCT()
struct AUSSPLUGINS_API FRepCharacterReplicationData
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FRepCharacterData> characterDatas;
};


namespace AussReplication
{

void InitServerData(const FString& serverName);

void UpdateDataToServer(const FString& serverName, const FRepCharacterReplicationData& replicationData);

FRepCharacterReplicationData GetRemoteServerData(const FString& serverName);

}
