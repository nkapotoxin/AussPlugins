#pragma once

#include "CoreMinimal.h"
#include "AussChannel.generated.h"

UCLASS()
class AUSSPLUGINS_API UAussChannel : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	AActor* Actor;

	FString AussGUID;

	UAussChannel();

	~UAussChannel();

	AActor* GetActor() { return Actor; }

	void SetChannelActor(AActor* InActor);
};

