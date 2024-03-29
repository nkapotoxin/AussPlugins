#include "AussArchive.h"

FDataStoreWriter::FDataStoreWriter()
{
}

FDataStoreWriter::FDataStoreWriter(TMap<int32, FString>* properties)
	: properties(properties)
{
}

void FDataStoreWriter::Serialize(const FString* src, const int32 index)
{
	properties->Add(index, *src);
}

FDataStoreReader::FDataStoreReader()
{
}

FDataStoreReader::FDataStoreReader(TMap<int32, FString>* properties)
	: properties(properties)
{
}

TMap<int32, FString>* FDataStoreReader::GetProperties()
{
	return properties;
}

void FDataStoreReader::Serialize(const FString* src, const int32 index)
{
	// TODO
	(*properties)[index] = *src;
}

