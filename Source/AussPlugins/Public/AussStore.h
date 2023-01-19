#pragma once

#include "AussEvent.h"
#include "../../ThirdParty/includes/cpp_redis/cpp_redis"


class AUSSPLUGINS_API AussStore
{
public:
	AussStore();
	virtual ~AussStore();

	static TMap<FString, UAussPawnData*> GetRemotePawnData(const FString& serverId);
	static void UpdatePawnData(const FString& serverName, const TArray<UAussPawnData*> pawnDatas);
	static void InitPawnData(const FString& serverName);

	static FString RedisIp;
	static FString RedisPw;
	static int32 RedisPort;
	static cpp_redis::client* RedisClient;	
};