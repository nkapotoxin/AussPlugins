#include "AussChannel.h"
#include "Log.h"

UAussChannel::UAussChannel()
	: Actor(nullptr)
{}

UAussChannel::~UAussChannel()
{}

void UAussChannel::SetChannelActor(AActor* InActor)
{
	check(Actor == nullptr);
	Actor = InActor;

	if (Actor)
	{
		ActorReplicator = FindOrCreateReplicator(Actor);
	}
}

TSharedRef<FAussObjectReplicator>& UAussChannel::FindOrCreateReplicator(UObject* Obj, bool* bOutCreated)
{
	TSharedRef<FAussObjectReplicator>* ReplicatorRefPtr = ReplicationMap.Find(Obj);
	if (ReplicatorRefPtr != nullptr)
	{
		if (!ReplicatorRefPtr->Get().GetWeakObjectPtr().IsValid())
		{
			ReplicatorRefPtr = nullptr;
			ReplicationMap.Remove(Obj);
		}
	}

	if (bOutCreated != nullptr)
	{
		*bOutCreated = (ReplicatorRefPtr == nullptr);
	}

	if (ReplicatorRefPtr == nullptr)
	{
		TSharedPtr<FAussObjectReplicator> NewReplicator = CreateReplicatorForNewActorChannel(Obj, RepLayoutHelper);
		TSharedRef<FAussObjectReplicator>& NewRef = ReplicationMap.Add(Obj, NewReplicator.ToSharedRef());
		NewRef->StartReplicating();
		return NewRef;
	}

	return *ReplicatorRefPtr;
}

TSharedPtr<FAussObjectReplicator> UAussChannel::CreateReplicatorForNewActorChannel(UObject* Object, FAussLayoutHelper* LayoutHelper)
{
	TSharedPtr<FAussObjectReplicator> NewReplicator = MakeShareable(new FAussObjectReplicator());
	NewReplicator->InitWithObject(Object, LayoutHelper);
	return NewReplicator;
}

int64 UAussChannel::ReplicateActor(TMap<int32, FString>* properties)
{
	check(Actor);
	check(RepLayoutHelper);
	check(ActorReplicator);

	UE_LOG(LogAussPlugins, Log, TEXT("ReplicateActor Actor: %s"), Actor->StaticConfigName());

	ActorReplicator->ReplicateProperties(properties);

	return 0;
}
