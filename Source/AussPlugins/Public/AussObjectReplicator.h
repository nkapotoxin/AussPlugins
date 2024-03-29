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
	bool ReplicateProperties(TMap<int32, FString>* properties);
	void StartReplicating();

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
	TSharedPtr<class FAussChangelistMgr> ChangelistMgr;
	TSharedPtr<FAussLayout> RepLayout;
	TUniquePtr<FAussState>  RepState;
	TUniquePtr<FAussState> CheckpointRepState;
	UClass* ObjectClass;
	UObject* ObjectPtr;

	FAussLayoutHelper* RepLayoutHelper;

private:
	TWeakObjectPtr<UObject> WeakObjectPtr;
};
