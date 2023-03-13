#pragma once

#include "AussReplication.generated.h"


namespace AussReplication
{

USTRUCT()
struct FUserPlayerInfo
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
struct FUserHumanStyleInfo
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
struct FPlayerState()
{
	GENERATED_USTRUCT_BODY()
	UPROPERTY()
	FUserPlayerInfo userPlayerInfo;
	UPROPERTY()
	FUserHumanStyleInfo userHumanStyleInfo;
};

USTRUCT()
struct AUSSPLUGINS_API FCharacterData
{
	GENERATED_BODY()
	UPROPERTY()
	FVector position;
	UPROPERTY()
	FVector rotation;
	UPROPERTY()
	FPlayerState playerState;
};

USTRUCT()
struct AUSSPLUGINS_API FCharacterReplicationData
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FCharacterData> characterDatas;
};

DECLARE_LOG_CATEGORY_EXTERN(LogAussReplication, Log, All);

inline DEFINE_LOG_CATEGORY(LogAussReplication)

void InitServerData(const FString &serverName);

void UpdateDataToServer(const FString &serverName, const FCharacterReplicationData &replicationData);

FCharacterReplicationData* GetRemoteServerData(const FString &serverName);

}
