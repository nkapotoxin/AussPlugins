#pragma once

#include "CoreMinimal.h"
#include "AussObjectReplicator.h"


class AUSSPLUGINS_API UAussChannel
{
public:
	UPROPERTY()
	AActor* Actor;

	FString AussGUID;

	FAussLayoutHelper* RepLayoutHelper;

	UAussChannel();
	~UAussChannel();

	AActor* GetActor() { return Actor; }

	void SetLayoutHelper(FAussLayoutHelper* InRepLayoutHelper) { RepLayoutHelper = InRepLayoutHelper; }

	void SetChannelActor(AActor* InActor);

	TSharedPtr<FAussObjectReplicator> CreateReplicatorForNewActorChannel(UObject* Object, FAussLayoutHelper* LayoutHelper);

	int64 ReplicateActor(TMap<int32, FString>* properties);
public:
	TSharedPtr<FAussObjectReplicator> ActorReplicator;
	TMap< UObject*, TSharedRef< FAussObjectReplicator > > ReplicationMap;

protected:
	TSharedRef<FAussObjectReplicator>& FindOrCreateReplicator(UObject* Obj, bool* bOutCreated = nullptr);
};

