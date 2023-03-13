#include "AussReplication.h"
#include "JsonUtilities/Public/JsonObjectConverter.h"
#include <Policies/CondensedJsonPrintPolicy.h>
#include <aussruntime/Api.h>
#include "Log.h"


using namespace Auss::Runtime;

void AussReplication::InitServerData(const FString &serverName)
{
	// The keys of ServerData is started with <serverName> + ---*
	Api::DelKeys(Api::Keys(TCHAR_TO_UTF8(*(serverName + "---*"))));
}

void AussReplication::UpdateDataToServer(const FString &serverName, const AussReplication::FCharacterReplicationData &replicationData)
{
	FString serverDataStr;
	if (FJsonObjectConverter::UStructToJsonObjectString(replicationData, serverDataStr, 0, 0))
	{
		UE_LOG(LogAussReplication, Log, TEXT("UpdateDataToServer successful: %s"), *serverDataStr);
		Api::SetValue(TCHAR_TO_UTF8(*(serverName + "---PawnDatas")), TCHAR_TO_UTF8(*serverDataStr));
	}
	else
	{
		UE_LOG(LogAussReplication, Warning, TEXT("UpdateDataToServer failed: %s"), *serverName);
	}
}

AussReplication::FCharacterReplicationData* AussReplication::GetRemoteServerData(const FString &serverName)
{
	AussReplication::FCharacterReplicationData result;

	std::string value = Api::GetValue(TCHAR_TO_UTF8(*(serverName + "---PawnDatas"));
	if (value != nullptr && value != "")
	{
		if (FJsonObjectConverter::JsonObjectStringToUStruct(value, &result, 0, 0))
		{
			UE_LOG(LogAussReplication, Log, TEXT("GetRemoteServerData convert successful, CharacterReplication number: %d"), result.characterDatas.Num());
		}
	}
	retrun &result;
}
