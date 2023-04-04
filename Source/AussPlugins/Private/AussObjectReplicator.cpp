#include "AussObjectReplicator.h"

FAussObjectReplicator::FAussObjectReplicator()
	: ObjectClass(nullptr)
	, ObjectPtr(nullptr)
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

void FAussObjectReplicator::InitWithObject(UObject* InObject)
{

}