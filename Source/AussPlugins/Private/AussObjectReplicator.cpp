#include "AussObjectReplicator.h"
#include "Log.h"


class FAussSerializeCB
{
public:
	FAussSerializeCB() {};

public:

};

FAussObjectReplicator::FAussObjectReplicator()
	: RepLayout(nullptr)
	, RepState(nullptr)
	, CheckpointRepState(nullptr)
	, ObjectClass(nullptr)
	, ObjectPtr(nullptr)
	, RepLayoutHelper(nullptr)
{
}

FAussObjectReplicator::~FAussObjectReplicator()
{
	CleanUp();
}

void FAussObjectReplicator::CleanUp()
{
	ObjectClass = nullptr;
	ObjectPtr = nullptr;
}

void FAussObjectReplicator::InitWithObject(UObject* InObject, FAussLayoutHelper* LayoutHelper)
{
	check(GetObject() == nullptr);
	check(ObjectClass == nullptr);
	check(RepLayoutHelper == nullptr);
	check(!RepState.IsValid());
	check(!RepLayout.IsValid());

	SetObject(InObject);

	if (GetObject() == nullptr)
	{
		UE_LOG(LogAussPlugins, Error, TEXT("InitWithObject: Object == nullptr"));
		return;
	}

	ObjectClass = InObject->GetClass();
	RepState = nullptr;
	RepLayoutHelper = LayoutHelper;

	RepLayout = RepLayoutHelper->GetObjectClassRepLayout(ObjectClass);

	uint8* Source = (uint8*)InObject;
	InitRecentProperties(Source);
}

void FAussObjectReplicator::InitRecentProperties(uint8* Source)
{
	UObject* MyObject = GetObject();

	check(MyObject);
	check(RepLayoutHelper);
	check(!RepState.IsValid());

	const FAussLayout& LocalRepLayout = *RepLayout;
	UClass* InObjectClass = MyObject->GetClass();

	// Initialize the RepState
	RepState = LocalRepLayout.CreateRepState(Source);

	// TODO(nkaptx): custom delta property state
}

bool FAussObjectReplicator::ReplicateProperties(TMap<int32, FString>* properties)
{
	UObject* Object = GetObject();
	if (Object == nullptr)
	{
		UE_LOG(LogAussPlugins, Verbose, TEXT("ReplicateProperties: Object == nullptr"));
		return false;
	}

	check(RepLayout.IsValid());
	check(RepState.IsValid());
	//check(RepState->GetSendingState());
	check(ChangelistMgr.IsValid());
	check(ChangelistMgr->GetRepChangelistState() != nullptr);
	check((ChangelistMgr->GetRepChangelistState()->StaticBuffer.Num() == 0) == RepLayout->IsEmpty());

	FDataStoreWriter writer(properties);
	const FConstAussObjectDataBuffer sourceData = Object;

	FAussSendingState* SendingRepState = RepState->GetSendingState();
	RepLayout->UpdateChangelistMgr(SendingRepState, *ChangelistMgr, Object, 0, false);
	RepLayout->SendProperties(SendingRepState, ChangelistMgr->GetRepChangelistState(), writer, sourceData);

	return true;
}

void FAussObjectReplicator::StartReplicating()
{
	UObject* Object = GetObject();
	if (Object == nullptr)
	{
		UE_LOG(LogAussPlugins, Verbose, TEXT("StartReplicating: Object == nullptr"));
		return;
	}

	if (UClass* const ObjectPtrClass = Object->GetClass())
	{
		ObjectClass = ObjectPtrClass;
	}

	if (FAussSendingState* SendingRepState = RepState.IsValid() ? RepState->GetSendingState() : nullptr)
	{
		SendingRepState->Retirement.SetNum(ObjectClass->ClassReps.Num());
	}

	ChangelistMgr = RepLayoutHelper->GetReplicationChangeListMgr(Object);
}