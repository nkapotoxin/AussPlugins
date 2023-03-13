#include "AussStore.h"
#include "Log.h"
#include "../../ThirdParty/includes/cpp_redis/cpp_redis"
#include <iostream>
#include "JsonUtilities/Public/JsonObjectConverter.h"
#include <Policies/CondensedJsonPrintPolicy.h>


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
	cpp_redis::reply newTmp = remotePawnEntityIds.get();
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


void AussStore::JsonTest()
{
	FString json = TEXT("{\"AussPawnDatas\": [{\"entityId\":\"eid001\",\"positionX\" : 0.1,\"positionY\" : 0.2 },{\"entityId\":\"eid002\"}]}");

	FString json2 = TEXT("{\"entityId\":\"eid001\",\"positionX\": 1.1, \"position\": {\"X\": 1.5, \"Y\": 1.2, \"Z\": 1.3 }}");

	FAussPData testStruct;

	FJsonObjectConverter::JsonObjectStringToUStruct(json2, &testStruct, 0, 0);

	UE_LOG(LogAussPlugins, Log, TEXT("JsonTest entityId :%s"), *testStruct.entityId);
	UE_LOG(LogAussPlugins, Log, TEXT("JsonTest positionX :%f"), testStruct.positionX);
	UE_LOG(LogAussPlugins, Log, TEXT("JsonTest position_X :%f"), testStruct.position.X);

	FAussPArray testArray;

	if (FJsonObjectConverter::JsonObjectStringToUStruct(json, &testArray, 0, 0))
	{
		UE_LOG(LogAussPlugins, Log, TEXT("JsonTest num :%d"), testArray.AussPawnDatas.Num());
	}

	FAussPArray newTestArray;
	FAussPData pData;
	pData.entityId = "1";
	pData.positionX = 1.2f;
	pData.position = FVector(1.1f, 1.2f, 3.8f);
	newTestArray.AussPawnDatas.Add(pData);
	FString strToJson;
	if (FJsonObjectConverter::UStructToJsonObjectString(FAussPArray::StaticStruct(), &newTestArray, strToJson, 0, 0))
	{
		UE_LOG(LogAussPlugins, Log, TEXT("JsonTest successful :%s"), *strToJson);
	}

	FAussPaData uData;
	uData.entityID = "udadtaid001";
	uData.position = FVector(1.1f, 1.2f, 3.8f);
	FString strToUdataJson;
	if (FJsonObjectConverter::UStructToJsonObjectString(uData, strToUdataJson, 0, 0))
	{
		UE_LOG(LogAussPlugins, Log, TEXT("JsonTest successful :%s"), *strToUdataJson);
	}

	FAussPaArray pawnDatas;
	pawnDatas.pawnDatas.Add(uData);
	FString pawnString = "";
	if (FJsonObjectConverter::UStructToJsonObjectString(pawnDatas, pawnString, 0, 0))
	{
		UE_LOG(LogAussPlugins, Log, TEXT("UpdatePawnDataNew successful :%s"), *pawnString);
	}
} 


void AussStore::UpdatePawnDataNew(const FString& serverName, const FAussPaArray& paArray)
{
	FString pawnString = "";
	if (FJsonObjectConverter::UStructToJsonObjectString(paArray, pawnString, 0, 0))
	{
		UE_LOG(LogAussPlugins, Log, TEXT("UpdatePawnDataNew successful :%s"), *pawnString);
	
		RedisClient->set(TCHAR_TO_UTF8(*(serverName + "---PawnDatas")), TCHAR_TO_UTF8(*pawnString));
		RedisClient->sync_commit();
	}
	else
	{
		UE_LOG(LogAussPlugins, Warning, TEXT("UpdatePawnDataNew failed"));
	}
}

FAussPaArray AussStore::GetRemotePawnDataNew(const FString& serverName)
{
	FAussPaArray paArray;
	auto pawnDataOri = RedisClient->get(TCHAR_TO_UTF8(*(serverName + "---PawnDatas")));
	RedisClient->sync_commit();

	cpp_redis::reply newTmp = pawnDataOri.get();
	if (newTmp.is_null())
	{
		UE_LOG(LogAussPlugins, Warning, TEXT("GetRemotePawnDataNew redis get null"));
		return paArray;
	}

	FString pawnDataStr = UTF8_TO_TCHAR(newTmp.as_string().c_str());

	UE_LOG(LogAussPlugins, Log, TEXT("GetRemotePawnDataNew get for server%s, %s"), *serverName, *pawnDataStr);

	if (FJsonObjectConverter::JsonObjectStringToUStruct(pawnDataStr, &paArray, 0, 0))
	{
		UE_LOG(LogAussPlugins, Log, TEXT("GetRemotePawnDataNew convert success, pa num :%d"), paArray.pawnDatas.Num());
	}

	return paArray;
}