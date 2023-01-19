#pragma once

#include "AussEvent.generated.h"

UCLASS(Blueprintable, BlueprintType)
class AUSSPLUGINS_API UAussPawnData : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Auss)
		FString entityClassName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Auss)
		FString entityID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Auss)
		FString remoteEntityID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Auss)
		FVector position;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Auss)
		FVector scale;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Auss)
		FRotator rotation;
};


UCLASS(Blueprintable, BlueprintType)
class AUSSPLUGINS_API UAussEventData : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Auss)
		FString eventName;
};


class AUSSPLUGINS_API AussEvent
{
public:
	AussEvent();
	virtual ~AussEvent();
	static bool registerEvent(const FString& eventName, const FString& funcName, TFunction<void(const UAussEventData*)> func, void* objPtr);
	static void fire(const FString& eventName, UAussEventData* pEventData);
	static void registerClass(const FString& pawnName, UClass* classPtr);
	static UClass* getRegisteredClass(const FString& pawnName);
	static TArray<UAussPawnData*> getRemotePawnData();

protected:
	struct EventObj
	{
		TFunction<void(const UAussEventData*)> method;
		FString funcName;
		void* objPtr;		
	};	

	struct FiredEvent
	{
		EventObj evt;
		FString eventName;
		UAussEventData* args;
	};

	static TMap<FString, TArray<EventObj>> events_;
	static TArray<FiredEvent*> firedEvents_;
	static TMap<FString, UClass*> classes_;
	static bool isPause_;
	static TArray<UAussPawnData*> remotePawnData_;
};


#define AUSS_REGISTER_EVENT(EVENT_NAME, EVENT_FUNC) \
	AussEvent::registerEvent(EVENT_NAME, #EVENT_FUNC, [this](const UAussEventData* pEventData) {	EVENT_FUNC(pEventData); }, (void*)this);

#define AUSS_EVENT_FIRE(EVENT_NAME, EVENT_DATA) AussEvent::fire(EVENT_NAME, EVENT_DATA);

#define AUSS_REGISTER_CLASS(CLASS_NAME, CLASS_PTR) AussEvent::registerClass(CLASS_NAME, CLASS_PTR);

#define AUSS_GET_REGISTEREDCLASS(CLASS_NAME) AussEvent::getRegisteredClass(CLASS_NAME);

#define AUSS_GET_REMOTE_PAWNS() AussEvent::getRemotePawnData();


UCLASS(Blueprintable, BlueprintType)
class AUSSPLUGINS_API UAussEventData_onEnterWorld : public UAussEventData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Auss)
		FString entityClassName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Auss)
		FString entityID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Auss)
		FVector position;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Auss)
		FVector direction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Auss)
		FRotator rotation;			
};


UCLASS(Blueprintable, BlueprintType)
class AUSSPLUGINS_API UAussEventData_onLeaveWorld : public UAussEventData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Auss)
		FString entityClassName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Auss)
		FString entityID;		
};


UCLASS(Blueprintable, BlueprintType)
class AUSSPLUGINS_API UAussEventData_updatePosition : public UAussEventData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Auss)
		FString entityClassName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Auss)
		FString entityID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Auss)
		FVector position;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Auss)
		FVector direction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Auss)
		FRotator rotation;			
};
