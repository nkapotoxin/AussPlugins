#include "AussEvent.h"

TMap<FString, TArray<AussEvent::EventObj>> AussEvent::events_;
TArray<AussEvent::FiredEvent*> AussEvent::firedEvents_;
TMap<FString, UClass*> AussEvent::classes_;
TArray<UAussPawnData*> AussEvent::remotePawnData_;
bool AussEvent::isPause_ = false;

AussEvent::AussEvent()
{
}

AussEvent::~AussEvent()
{
}

bool AussEvent::registerEvent(const FString& eventName, const FString& funcName, TFunction<void(const UAussEventData*)> func, void* objPtr)
{
	TArray<EventObj>* eo_array = NULL;
	TArray<EventObj>* eo_array_find = events_.Find(eventName);

	if (!eo_array_find)
	{
		events_.Add(eventName, TArray<EventObj>());
		eo_array = &(*events_.Find(eventName));
	}
	else
	{
		eo_array = &(*eo_array_find);
	}

	EventObj eo;
	eo.funcName = funcName;
	eo.method = func;
	eo.objPtr = objPtr;
	eo_array->Add(eo);
	return true;
}

void AussEvent::fire(const FString& eventName, UAussEventData* pEventData)
{
	TArray<EventObj>* eo_array_find = events_.Find(eventName);
	if (!eo_array_find || (*eo_array_find).Num() == 0)
	{
		return;
	}

	pEventData->eventName = eventName;
	for (auto& item : (*eo_array_find))
	{
		if (!isPause_)
		{
			item.method(pEventData);
			pEventData->ConditionalBeginDestroy();
		}
		else
		{
			FiredEvent* event = new FiredEvent;
			event->evt = item;
			event->eventName = eventName;
			event->args = pEventData;
			firedEvents_.Emplace(event);
		}
	}
}

void AussEvent::registerClass(const FString& pawnName, UClass* classPtr)
{
	classes_.Add(pawnName, classPtr);
}

UClass* AussEvent::getRegisteredClass(const FString& pawnName)
{
	UClass** classPtr = classes_.Find(pawnName);
	if (classPtr == NULL)
	{
		return NULL;
	}

	return *classPtr;
}

TArray<UAussPawnData*> AussEvent::getRemotePawnData()
{
	TArray<UAussPawnData*> tmpResult;
	return tmpResult;
}