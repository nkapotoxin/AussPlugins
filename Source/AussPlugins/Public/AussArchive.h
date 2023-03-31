#pragma once

#include "CoreTypes.h"
#include "Serialization/Archive.h"

/**
* Base class for serializing obj in data store.
*/
class AUSSPLUGINS_API FDataStoreArchive : public FArchive
{
public:
	/**
	 * Default Constructor
	 */
	FDataStoreArchive()
	{
		ArMaxSerializeSize = 16 * 1024 * 1024;
	}

	virtual void SerializeBitsWithOffset(void* Src, int32 SourceBit, int64 LengthBits) PURE_VIRTUAL(FDataStoreArchive::SerializeBitsWithOffset, );
};