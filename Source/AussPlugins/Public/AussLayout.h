#pragma once

#include "AussArchive.h"
#include "Net/RepLayout.h"

class FAussLayout : public FGCObject, public TSharedFromThis<FAussLayout>
{
private:
	FAussLayout();

public:
	virtual ~FAussLayout();

	/** All Layout Commands. */
	TArray<FRepLayoutCmd> Cmds;

	/** Top level Layout Commands. */
	TArray<FRepParentCmd> Parents;

	UStruct* Owner;

	static TSharedPtr<FAussLayout> CreateFromClass(UClass* InObjectClass);

	void InitFromClass(UClass* InObjectClass);

	void SendProperties(FDataStoreWriter& Ar, const FConstRepObjectDataBuffer SourceData) const;

	void ReceiveProperties(FDataStoreReader& Ar, const FConstRepObjectDataBuffer SourceData) const;

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
};

