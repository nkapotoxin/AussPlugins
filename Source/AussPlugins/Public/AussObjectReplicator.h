#pragma once

#include "CoreMinimal.h"
#include "AussLayout.h"

class AUSSPLUGINS_API FAussObjectReplicator
{
public:
	FAussObjectReplicator();
	~FAussObjectReplicator();

	void CleanUp();
	void InitWithObject(UObject* InObject, FAussLayoutHelper* LayoutHelper);
	void InitRecentProperties(uint8* Source);

	FORCEINLINE TWeakObjectPtr<UObject>	GetWeakObjectPtr() const
	{
		return WeakObjectPtr;
	}

	FORCEINLINE UObject* GetObject() const
	{
		return ObjectPtr;
	}

	FORCEINLINE void SetObject(UObject* NewObj)
	{
		ObjectPtr = NewObj;
		WeakObjectPtr = NewObj;
	}

public:
	TSharedPtr<FAussLayout> RepLayout;
	TUniquePtr<FAussState>  RepState;
	TUniquePtr<FAussState> CheckpointRepState;
	UClass* ObjectClass;
	UObject* ObjectPtr;

	FAussLayoutHelper* RepLayoutHelper;

private:
	TWeakObjectPtr<UObject> WeakObjectPtr;
};
