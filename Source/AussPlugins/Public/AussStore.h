#pragma once

#include "AussEvent.h"
#include "../../ThirdParty/includes/cpp_redis/cpp_redis"
#include "AussStore.generated.h"


class AUSSPLUGINS_API AussStore
{
public:
	AussStore();
	virtual ~AussStore();

	static TMap<FString, UAussPawnData*> GetRemotePawnData(const FString& serverName);
	static void UpdatePawnData(const FString& serverName, const TArray<UAussPawnData*> pawnDatas);
	static void InitPawnData(const FString& serverName);

	static void JsonTest();

	static void UpdatePawnDataNew(const FString& serverName, const FAussPaArray& paArray);
	static FAussPaArray GetRemotePawnDataNew(const FString& serverName);

	static FString RedisIp;
	static FString RedisPw;
	static int32 RedisPort;
	static cpp_redis::client* RedisClient;
};


USTRUCT()
struct AUSSPLUGINS_API FAussPData
{
	GENERATED_BODY()

	UPROPERTY()
	FString entityId;

	UPROPERTY()
	float positionX;

	UPROPERTY()
	float positionY;

	UPROPERTY()
	float positionZ;

	UPROPERTY()
	float rotationP;

	UPROPERTY()
	float rotationY;

	UPROPERTY()
	float rotationR;

	UPROPERTY()
	FVector position;
};


USTRUCT()
struct AUSSPLUGINS_API FAussPArray
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FAussPData> AussPawnDatas;
};
