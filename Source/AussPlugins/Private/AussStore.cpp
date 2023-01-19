#include "AussStore.h"
#include "Log.h"
#include "../../ThirdParty/includes/cpp_redis/cpp_redis"
#include <iostream>

FString AussStore::RedisIp;
FString AussStore::RedisPw;
int32 AussStore::RedisPort;
cpp_redis::client* AussStore::RedisClient;

AussStore::AussStore()
{
	UE_LOG(LogAussPlugins, Warning, TEXT("AussStore init"));
}
	
AussStore::~AussStore()
{
}

TMap<FString, UAussPawnData*> AussStore::GetRemotePawnData(const FString& serverName)
{
	auto remotePawnEntityIds = RedisClient->get(TCHAR_TO_UTF8(*(serverName + "---LocalPawn---EntityIDs")));
	RedisClient->sync_commit();

	TMap<FString, UAussPawnData*> remotePawnData;
	cpp_redis::reply newTmp == remotePawnEntityIds.get();
	if (newTmp.is_null())
	{
		return remotePawnData;
	}

	TArray<FString> remotePawnIds;
	FString tmpPawnIds = "";
	try
	{
		tmpPawnIds = newTmp.as_string().c_str();
		tmpPawnIds.ParseIntoArray(remotePawnIds, TEXT(";"), true);
	}
	catch (std::exception& e)
	{
		UE_LOG(LogAussPlugins, Warning, TEXT("GetRemotePawnData servername:%s, tmpPawnIds:%s, error:%s"), *serverName, *tmpPawnIds, e.what());
		return remotePawnData;
	}

	for (FString elem : remotePawnIds)
	{
		auto px = RedisClient->get(TCHAR_TO_UTF8(*(serverName + "---LocalPawn---" + elem + "---PX")));
		auto py = RedisClient->get(TCHAR_TO_UTF8(*(serverName + "---LocalPawn---" + elem + "---PY")));
		auto pz = RedisClient->get(TCHAR_TO_UTF8(*(serverName + "---LocalPawn---" + elem + "---PZ")));

		auto lx = RedisClient->get(TCHAR_TO_UTF8(*(serverName + "---LocalPawn---" + elem + "---LX")));
		auto ly = RedisClient->get(TCHAR_TO_UTF8(*(serverName + "---LocalPawn---" + elem + "---LY")));
		auto lz = RedisClient->get(TCHAR_TO_UTF8(*(serverName + "---LocalPawn---" + elem + "---LZ")));

		RedisClient->sync_commit();

		UAussPawnData* tmp = NewObject<UAussPawnData>();
		tmp->remoteEntityID = elem;
		FString ppx = px.get().as_string().c_str();
		FString ppy = py.get().as_string().c_str();
		FString ppz = pz.get().as_string().c_str();
		FString plx = lx.get().as_string().c_str();
		FString ply = ly.get().as_string().c_str();
		FString plz = lz.get().as_string().c_str();

		tmp->position = FVector(FCString::Atof(*ppx), FCString::Atof(*ppy), FCString::Atof(*ppz));
		tmp->rotation = FRotator(FCString::Atof(*plx), FCString::Atof(*ply), FCString::Atof(*plz));
		tmp->scale = FVector(0.0f, 0.0f, 0.0f);

		remotePawnData.Add(elem, tmp);
	}

	return remotePawnData;
}

void AussStore::UpdatePawnData(const FString& serverName, const TArray<UAussPawnData*> pawnDatas)
{
	FString localPawnIds = "";
	for (UAussPawnData* elem : pawnDatas)
	{
		RedisClient->set(TCHAR_TO_UTF8(*(serverName + "---LocalPawn---" + elem->entityID + "---PX")),
			TCHAR_TO_UTF8(*FString::SanitizeFloat(elem->position.X)));
		RedisClient->set(TCHAR_TO_UTF8(*(serverName + "---LocalPawn---" + elem->entityID + "---PY")),
			TCHAR_TO_UTF8(*FString::SanitizeFloat(elem->position.Y)));
		RedisClient->set(TCHAR_TO_UTF8(*(serverName + "---LocalPawn---" + elem->entityID + "---PZ")),
			TCHAR_TO_UTF8(*FString::SanitizeFloat(elem->position.Z)));

		RedisClient->set(TCHAR_TO_UTF8(*(serverName + "---LocalPawn---" + elem->entityID + "---LX")),
			TCHAR_TO_UTF8(*FString::SanitizeFloat(elem->rotation.Pitch)));
		RedisClient->set(TCHAR_TO_UTF8(*(serverName + "---LocalPawn---" + elem->entityID + "---LY")),
			TCHAR_TO_UTF8(*FString::SanitizeFloat(elem->rotation.Yaw)));
		RedisClient->set(TCHAR_TO_UTF8(*(serverName + "---LocalPawn---" + elem->entityID + "---LZ")),
			TCHAR_TO_UTF8(*FString::SanitizeFloat(elem->rotation.Roll)));

		if (localPawnIds != "")
		{
			localPawnIds += ";" + elem->entityID;
		}
		else
		{
			localPawnIds += elem->entityID;
		}
	}

	RedisClient->set(TCHAR_TO_UTF8(*(serverName + "---LocalPawn---EntityIDs")), TCHAR_TO_UTF8(*localPawnIds));
	RedisClient->sync_commit();	
}

void AussStore::InitPawnData(const FString& serverName)
{
	FString RedisPortString;
	GConfig->GetString(TEXT("Auss"), TEXT("RedisIp"), RedisIp, GGameIni);
	GConfig->GetString(TEXT("Auss"), TEXT("RedisPw"), RedisPw, GGameIni);
	GConfig->GetString(TEXT("Auss"), TEXT("RedisPort"), RedisPortString, GGameIni);

	RedisPort = FCString::Atoi(*RedisPortString);
	static cpp_redis::client client;
	RedisClient = &client;
	RedisClient->connect(std::string(TCHAR_TO_UTF8(*RedisIp)), RedisPort);
	RedisClient->auth(std::string(TCHAR_TO_UTF8(*RedisPw)));

	UE_LOG(LogAussPlugins, Warning, TEXT("AussStore init pawn data"));
	auto allPawnData = RedisClient->keys(TCHAR_TO_UTF8(*(serverName + "---*")));
	RedisClient->sync_commit();

	std::vector<cpp_redis::reply> tmpArray = allPawnData.get().as_array();
	std::vector<std::string> tmpList;
	for (cpp_redis::reply tmp : tmpArray)
	{
		FString key = tmp.as_string().c_str();
		tmpList.push_back(TCHAR_TO_UTF8(*key));
	}

	RedisClient->del(tmpList);
	RedisClient->sync_commit();
}
