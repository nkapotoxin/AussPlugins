#pragma once

#include "CoreMinimal.h"
#include "AussLayout.h"

class AUSSPLUGINS_API FAussObjectReplicator
{
public:
	FAussObjectReplicator();
	~FAussObjectReplicator();

	void CleanUp();
	void InitWithObject(UObject* InObject);

public:
	TSharedPtr<FAussLayout> RepLayout;
	UClass* ObjectClass;
	UObject* ObjectPtr;
};
