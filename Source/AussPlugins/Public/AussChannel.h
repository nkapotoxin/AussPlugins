#pragma once

#include "CoreMinimal.h"
#include "AussObjectReplicator.h"
#include "AussChannel.generated.h"

UCLASS()
class AUSSPLUGINS_API UAussChannel : public UObject
{
	GENERATED_BODY()

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
public:
	TSharedPtr<FAussObjectReplicator> ActorReplicator;
	TMap< UObject*, TSharedRef< FAussObjectReplicator > > ReplicationMap;

protected:
	TSharedRef< FAussObjectReplicator >& FindOrCreateReplicator(UObject* Obj, bool* bOutCreated = nullptr);
};

