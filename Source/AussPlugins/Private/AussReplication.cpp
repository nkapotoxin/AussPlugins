#include "AussReplication.h"
#include "JsonUtilities/Public/JsonObjectConverter.h"
#include <Policies/CondensedJsonPrintPolicy.h>
#include "AussUtils.h"
#include "Log.h"


using namespace Auss::Runtime;

void AussReplication::InitServerData(const FString &serverName)
{
	// Init Server Sdk
	AussUtils::InitSDK();

	// The keys of ServerData is started with <serverName> + ---*
	AussUtils::DeleteFieldsFromAuss(AussUtils::ReadKeysFromAuss(TCHAR_TO_UTF8(*(serverName + "---*"))));
}

void AussReplication::UpdateDataToServer(const FString &serverName, const FRepCharacterReplicationData &replicationData)
{
	FString serverDataStr;
	if (FJsonObjectConverter::UStructToJsonObjectString(replicationData, serverDataStr, 0, 0))
	{
		UE_LOG(LogAussPlugins, Log, TEXT("UpdateDataToServer successful: %s"), *serverDataStr);
		AussUtils::WriteFieldToAuss(TCHAR_TO_UTF8(*(serverName + "---PawnDatas")), TCHAR_TO_UTF8(*serverDataStr));
	}
	else
	{
		UE_LOG(LogAussPlugins, Warning, TEXT("UpdateDataToServer failed: %s"), *serverName);
	}
}

FRepCharacterReplicationData AussReplication::GetRemoteServerData(const FString &serverName)
{
	FRepCharacterReplicationData result;

	std::string value = AussUtils::ReadFieldFromAuss(TCHAR_TO_UTF8(*(serverName + "---PawnDatas")));
	if (value != "")
	{
		if (FJsonObjectConverter::JsonObjectStringToUStruct(UTF8_TO_TCHAR(value.c_str()), &result, 0, 0))
		{
			UE_LOG(LogAussPlugins, Log, TEXT("GetRemoteServerData convert successful, CharacterReplication number: %d"), result.characterDatas.Num());
		}
	}
	return result;
}
