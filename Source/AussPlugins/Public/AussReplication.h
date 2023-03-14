#pragma once

#include "AussReplication.generated.h"

USTRUCT()
struct AUSSPLUGINS_API FRepUserPlayerInfo
{
	GENERATED_USTRUCT_BODY()
	UPROPERTY()
		FString userId;
	UPROPERTY()
		FString userName;
	UPROPERTY()
		FString realName;
	UPROPERTY()
		FString nickName;
	UPROPERTY()
		FString title;
	UPROPERTY()
		FString company;
	UPROPERTY()
		FString email;
	UPROPERTY()
		FString phone;
	UPROPERTY()
		FString loginDate;
};

USTRUCT()
struct AUSSPLUGINS_API FRepUserHumanStyleInfo
{
	GENERATED_USTRUCT_BODY()
	UPROPERTY()
		int32 face;
	UPROPERTY()
		int32 hair;
	UPROPERTY()
		int32 cloth;
};

USTRUCT()
struct AUSSPLUGINS_API FRepUserPlayerState
{
	GENERATED_USTRUCT_BODY()
	UPROPERTY()
	FRepUserPlayerInfo userPlayerInfo;
	UPROPERTY()
	FRepUserHumanStyleInfo userHumanStyleInfo;
};

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
	FRepUserPlayerState playerState;
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
