#pragma once

#include "CoreTypes.h"
#include "AussReplication.h"
#include "Serialization/Archive.h"

/**
* Base class for serializing object in data store.
*/
class AUSSPLUGINS_API FDataStoreWriter
{
public:
	FDataStoreWriter();
	FDataStoreWriter(TMap<int32, FString>* properties);

	virtual void Serialize(const FString* src, const int32 index);

private:
	TMap<int32, FString>* properties;
};

class AUSSPLUGINS_API FDataStoreReader
{
public:
	FDataStoreReader();
	FDataStoreReader(TMap<int32, FString>* properties);

	TMap<int32, FString>* GetProperties();

	virtual void Serialize(const FString* src, const int32 index);

private:
	TMap<int32, FString>* properties;
};