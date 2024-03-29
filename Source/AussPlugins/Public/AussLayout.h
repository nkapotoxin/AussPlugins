#pragma once

#include "AussArchive.h"

class FAussState;
class FAussReceivingState;
class FAussSendingState;
class FAussLayoutCmd;

enum class EAussLayoutCmdType : uint8
{
	DynamicArray = 0,	//! Dynamic array
	Return = 1,			//! Return from array, or end of stream
	Property = 2,		//! Generic property

	PropertyBool = 3,
	PropertyFloat = 4,
	PropertyInt = 5,
	PropertyByte = 6,
	PropertyName = 7,
	PropertyObject = 8,
	PropertyUInt32 = 9,
	PropertyVector = 10,
	PropertyRotator = 11,
	PropertyPlane = 12,
	PropertyVector100 = 13,
	PropertyNetId = 14,
	RepMovement = 15,
	PropertyVectorNormal = 16,
	PropertyVector10 = 17,
	PropertyVectorQ = 18,
	PropertyString = 19,
	PropertyUInt64 = 20,
	PropertyNativeBool = 21,
	PropertySoftObject = 22,
	PropertyWeakObject = 23,
	NetSerializeStructWithObjectReferences = 24,
};

enum class EAussDataBufferType
{
	ObjectBuffer,	
	ShadowBuffer	
};

namespace Auss_RepLayout_Private
{
	template<EAussDataBufferType DataType, typename ConstOrNotType>
	struct TAussDataBufferBase
	{
		static constexpr EAussDataBufferType Type = DataType;
		using ConstOrNotVoid = typename TCopyQualifiersFromTo<ConstOrNotType, void>::Type;

	public:

		TAussDataBufferBase(ConstOrNotVoid* RESTRICT InDataBuffer) :
			Data((ConstOrNotType* RESTRICT)InDataBuffer)
		{}

		friend TAussDataBufferBase operator+(TAussDataBufferBase InBuffer, int32 Offset)
		{
			return InBuffer.Data + Offset;
		}

		explicit operator bool() const
		{
			return Data != nullptr;
		}

		operator ConstOrNotType* () const
		{
			return Data;
		}

		ConstOrNotType* RESTRICT Data;
	};

	template<typename TLayoutCmdType, typename ConstOrNotType>
	static TAussDataBufferBase<EAussDataBufferType::ObjectBuffer, ConstOrNotType> operator+(TAussDataBufferBase<EAussDataBufferType::ObjectBuffer, ConstOrNotType> InBuffer, const TLayoutCmdType& Cmd)
	{
		return InBuffer + Cmd.Offset;
	}

	template<typename TLayoutCmdType, typename ConstOrNotType>
	static TAussDataBufferBase<EAussDataBufferType::ShadowBuffer, ConstOrNotType> operator+(const TAussDataBufferBase<EAussDataBufferType::ShadowBuffer, ConstOrNotType> InBuffer, const TLayoutCmdType& Cmd)
	{
		return InBuffer + Cmd.ShadowOffset;
	}
}

template<EAussDataBufferType DataType> using TAussDataBuffer = Auss_RepLayout_Private::TAussDataBufferBase<DataType, uint8>;
template<EAussDataBufferType DataType> using TConstAussDataBuffer = Auss_RepLayout_Private::TAussDataBufferBase<DataType, const uint8>;

typedef TAussDataBuffer<EAussDataBufferType::ObjectBuffer> FAussObjectDataBuffer;
typedef TAussDataBuffer<EAussDataBufferType::ShadowBuffer> FAussShadowDataBuffer;
typedef TConstAussDataBuffer<EAussDataBufferType::ObjectBuffer> FConstAussObjectDataBuffer;
typedef TConstAussDataBuffer<EAussDataBufferType::ShadowBuffer> FConstAussShadowDataBuffer;

enum class EAussLayoutCmdFlags : uint8
{
	None = 0,		//! No flags.
	IsSharedSerialization = (1 << 0),	//! Indicates the property is eligible for shared serialization.
	IsStruct = (1 << 1)	//! This is a struct property.
};

ENUM_CLASS_FLAGS(EAussLayoutCmdFlags)

enum class EAussParentFlags : uint32
{
	None = 0,
	IsLifetime = (1 << 0),				//! This property is valid for the lifetime of the object (almost always set).
	IsConditional = (1 << 1),			//! This property has a secondary condition to check
	IsConfig = (1 << 2),				//! This property is defaulted from a config file
	IsCustomDelta = (1 << 3),			//! This property uses custom delta compression. Mutually exclusive with IsNetSerialize.
	IsNetSerialize = (1 << 4),			//! This property uses a custom net serializer. Mutually exclusive with IsCustomDelta.
	IsStructProperty = (1 << 5),		//! This property is a FStructProperty.
	IsZeroConstructible = (1 << 6),		//! This property is ZeroConstructible.
	IsFastArray = (1 << 7),				//! This property is a FastArraySerializer. This can't be a ERepLayoutCmdType, because
										//! these Custom Delta structs will have their inner properties tracked.
	HasObjectProperties = (1 << 8),		//! This property is tracking UObjects (may be through nested properties).
	HasNetSerializeProperties = (1 << 9),  //! This property contains Net Serialize properties (may be through nested properties).
	HasDynamicArrayProperties = (1 << 10), //! This property contains Dynamic Array properties (may be through nested properties).
};

ENUM_CLASS_FLAGS(EAussParentFlags)

class FAussLayoutCmd
{
public:
	/** Pointer back to property, used for NetSerialize calls, etc. */
	FProperty* Property;

	/** For arrays, this is the cmd index to jump to, to skip this arrays inner elements. */
	uint16 EndCmd;

	/** For arrays, element size of data. */
	uint16 ElementSize;

	/** Absolute offset of property in Object Memory. */
	int32 Offset;

	/** Absolute offset of property in Shadow Memory. */
	int32 ShadowOffset;

	/** Handle relative to start of array, or top list. */
	uint16 RelativeHandle;

	/** Index into Parents. */
	uint16 ParentIndex;

	/** Used to determine if property is still compatible */
	uint32 CompatibleChecksum;

	EAussLayoutCmdType Type;
	EAussLayoutCmdFlags Flags;
};

class FAussParentCmd
{
public:
	FAussParentCmd(FProperty* InProperty, int32 InArrayIndex) :
		Property(InProperty),
		CachedPropertyName(InProperty ? InProperty->GetFName() : NAME_None),
		ArrayIndex(InArrayIndex),
		ShadowOffset(0),
		CmdStart(0),
		CmdEnd(0),
		Condition(COND_None),
		RepNotifyCondition(REPNOTIFY_OnChanged),
		RepNotifyNumParams(INDEX_NONE),
		Flags(EAussParentFlags::None)
	{}

	FProperty* Property;
	const FName CachedPropertyName;
	int32 ArrayIndex;
	int32 Offset;
	int32 ShadowOffset;
	uint16 CmdStart;
	uint16 CmdEnd;
	ELifetimeCondition Condition;
	ELifetimeRepNotifyCondition RepNotifyCondition;
	int32 RepNotifyNumParams;
	EAussParentFlags Flags;
};

enum class EAussCreateRepStateFlags : uint32
{
	None,
	SkipCreateReceivingState = 0x1,	// Don't create a receiving RepState, as we never expect it to be used.
};
ENUM_CLASS_FLAGS(EAussCreateRepStateFlags);

class FAussHandleToCmdIndex
{
public:
	FAussHandleToCmdIndex() :
		CmdIndex(INDEX_NONE)
	{
	}

	FAussHandleToCmdIndex(const int32 InHandleToCmdIndex) :
		CmdIndex(InHandleToCmdIndex)
	{
	}

	FAussHandleToCmdIndex(FAussHandleToCmdIndex&& Other) :
		CmdIndex(Other.CmdIndex),
		HandleToCmdIndex(MoveTemp(Other.HandleToCmdIndex))
	{
	}

	FAussHandleToCmdIndex& operator=(FAussHandleToCmdIndex&& Other)
	{
		if (this != &Other)
		{
			CmdIndex = Other.CmdIndex;
			HandleToCmdIndex = MoveTemp(Other.HandleToCmdIndex);
		}

		return *this;
	}

	int32 CmdIndex;
	TUniquePtr<TArray<FAussHandleToCmdIndex>> HandleToCmdIndex;
};

class FAussChangelistIterator
{
public:

	FAussChangelistIterator(const TArray<uint16>& InChanged, const int32 InChangedIndex) :
		Changed(InChanged),
		ChangedIndex(InChangedIndex)
	{}

	const TArray<uint16>& Changed;
	int32 ChangedIndex;
};

class FAussHandleIterator
{
public:

	FAussHandleIterator(
		UStruct const* const InOwner,
		FAussChangelistIterator& InChangelistIterator,
		const TArray<FAussLayoutCmd>& InCmds,
		const TArray<FAussHandleToCmdIndex>& InHandleToCmdIndex,
		const int32 InElementSize,
		const int32 InMaxArrayIndex,
		const int32 InMinCmdIndex,
		const int32 InMaxCmdIndex
	) :
		ChangelistIterator(InChangelistIterator),
		Cmds(InCmds),
		HandleToCmdIndex(InHandleToCmdIndex),
		NumHandlesPerElement(HandleToCmdIndex.Num()),
		ArrayElementSize(InElementSize),
		MaxArrayIndex(InMaxArrayIndex),
		MinCmdIndex(InMinCmdIndex),
		MaxCmdIndex(InMaxCmdIndex),
		Owner(InOwner),
		LastSuccessfulCmdIndex(INDEX_NONE)
	{
		ensureMsgf(MaxCmdIndex >= MinCmdIndex, TEXT("Invalid Min / Max Command Indices. Owner=%s, MinCmdIndex=%d, MaxCmdIndex=%d"), *GetPathNameSafe(Owner), MinCmdIndex, MaxCmdIndex);
	}

	bool NextHandle();
	bool JumpOverArray();
	int32 PeekNextHandle() const;
	FAussChangelistIterator& ChangelistIterator;
	const TArray<FAussLayoutCmd>& Cmds;
	const TArray<FAussHandleToCmdIndex>& HandleToCmdIndex;
	const int32 NumHandlesPerElement;
	const int32	ArrayElementSize;
	const int32	MaxArrayIndex;
	const int32	MinCmdIndex;
	const int32	MaxCmdIndex;
	int32 Handle;
	int32 CmdIndex;
	int32 ArrayIndex;
	int32 ArrayOffset;
	UStruct const* const Owner;

private:

	int32 LastSuccessfulCmdIndex;
};

class FAussChangedHistory
{
public:
	FAussChangedHistory() :
		Resend(false)
	{}

	void CountBytes(FArchive& Ar) const
	{
		Changed.CountBytes(Ar);
	}

	bool WasSent() const
	{
		return OutPacketIdRange.First != INDEX_NONE;
	}

	void Reset()
	{
		OutPacketIdRange = FPacketIdRange();
		Changed.Empty();
		Resend = false;
	}

	FPacketIdRange OutPacketIdRange;
	TArray<uint16> Changed;
	bool Resend;
};

struct FAussStateStaticBuffer : public FNoncopyable
{
private:
	friend class FAussLayout;

	FAussStateStaticBuffer(const TSharedRef<const FAussLayout>& InLayout) :
		Layout(InLayout)
	{}

public:
	FAussStateStaticBuffer(FAussStateStaticBuffer&& InStaticBuffer) :
		Buffer(MoveTemp(InStaticBuffer.Buffer)),
		Layout(MoveTemp(InStaticBuffer.Layout))
	{}

	~FAussStateStaticBuffer();

	uint8* GetData()
	{
		return Buffer.GetData();
	}

	const uint8* GetData() const
	{
		return Buffer.GetData();
	}

	int32 Num() const
	{
		return Buffer.Num();
	}

private:
	TArray<uint8, TAlignedHeapAllocator<16>> Buffer;
	TSharedRef<const FAussLayout> Layout;
};

struct FAussSharedPropertyKey
{
private:
	uint32 CmdIndex = 0;
	uint32 ArrayIndex = 0;
	uint32 ArrayDepth = 0;
	void* DataPtr = nullptr;

public:
	FAussSharedPropertyKey()
		: CmdIndex(0)
		, ArrayIndex(0)
		, ArrayDepth(0)
		, DataPtr(nullptr)
	{
	}

	explicit FAussSharedPropertyKey(uint32 InCmdIndex, uint32 InArrayIndex, uint32 InArrayDepth, void* InDataPtr)
		: CmdIndex(InCmdIndex)
		, ArrayIndex(InArrayIndex)
		, ArrayDepth(InArrayDepth)
		, DataPtr(InDataPtr)
	{
	}

	FString ToDebugString() const
	{
		return FString::Printf(TEXT("{Cmd: %u, Index: %u, Depth: %u, Ptr: %x}"), CmdIndex, ArrayIndex, ArrayDepth, DataPtr);
	}

	friend bool operator==(const FAussSharedPropertyKey& A, const FAussSharedPropertyKey& B)
	{
		return (A.CmdIndex == B.CmdIndex) && (A.ArrayIndex == B.ArrayIndex) && (A.ArrayDepth == B.ArrayDepth) && (A.DataPtr == B.DataPtr);
	}

	friend uint32 GetTypeHash(const FAussSharedPropertyKey& Key)
	{
		return uint32(CityHash64((char*)&Key, sizeof(FAussSharedPropertyKey)));
	}
};

struct FAussSerializedPropertyInfo
{
	FAussSerializedPropertyInfo() :
		BitOffset(0),
		BitLength(0),
		PropBitOffset(0),
		PropBitLength(0)
	{}

	FAussSharedPropertyKey PropertyKey;
	int32 BitOffset;
	int32 BitLength;
	int32 PropBitOffset;
	int32 PropBitLength;
};

class FAussReceivingState : public FNoncopyable
{
private:
	friend class FAussLayout;

	FAussReceivingState(FAussStateStaticBuffer&& InStaticBuffer);

public:
	FAussStateStaticBuffer StaticBuffer;
};

class FAussSendingState : public FNoncopyable
{
private:
	friend class FAussLayout;

	FAussSendingState():
		LastChangelistIndex(0),
		LastCompareIndex(0),
		InactiveChangelist({ 0 })
	{}

public:
	int32 LastChangelistIndex;
	int32 LastCompareIndex;
	static constexpr int32 MAX_CHANGE_HISTORY = 32;
	FAussChangedHistory ChangeHistory[MAX_CHANGE_HISTORY];
	TArray<uint16> LifetimeChangelist;
	TArray<uint16> InactiveChangelist;
	TArray<FPropertyRetirement> Retirement;
};

struct FAussSerializationSharedInfo
{
	FAussSerializationSharedInfo() :
		SerializedProperties(MakeUnique<FNetBitWriter>(0)),
		bIsValid(false)
	{}

	void SetValid()
	{
		bIsValid = true;
	}

	bool IsValid() const
	{
		return bIsValid;
	}

	void Reset()
	{
		if (bIsValid)
		{
			SharedPropertyInfo.Reset();
			SerializedProperties->Reset();

			bIsValid = false;
		}
	}

	TArray<FAussSerializedPropertyInfo> SharedPropertyInfo;
	TUniquePtr<FNetBitWriter> SerializedProperties;

private:
	bool bIsValid;
};

class FAussChangelistState : public FNoncopyable
{
private:

	friend class FAussChangelistMgr;

	FAussChangelistState(
		const TSharedRef<const FAussLayout>& InRepLayout,
		const uint8* Source,
		const UObject* InRepresenting);

public:

	~FAussChangelistState() {};

	static const int32 MAX_CHANGE_HISTORY = 64;
	FAussChangedHistory ChangeHistory[MAX_CHANGE_HISTORY];
	int32 HistoryStart;
	int32 HistoryEnd;
	int32 CompareIndex;
	FAussStateStaticBuffer StaticBuffer;
	FAussSerializationSharedInfo SharedSerialization;
};

class FAussChangelistMgr : public FNoncopyable
{
private:

	friend class FAussLayout;

	FAussChangelistMgr(
		const TSharedRef<const FAussLayout>& InRepLayout,
		const uint8* Source,
		const UObject* InRepresenting);

public:

	~FAussChangelistMgr() {};

	FAussChangelistState* GetRepChangelistState() const
	{
		return const_cast<FAussChangelistState*>(&RepChangelistState);
	}

private:

	uint32 LastReplicationFrame;
	uint32 LastInitialReplicationFrame;

	FAussChangelistState RepChangelistState;
};

class FAussState : public FNoncopyable
{
private:
	friend class FAussLayout;

	FAussState() {}

	TUniquePtr<FAussReceivingState> ReceivingState;
	TUniquePtr<FAussSendingState> SendingState;

public:
	FAussReceivingState* GetReceivingState()
	{
		return ReceivingState.Get();
	}

	const FAussReceivingState* GetReceivingState() const
	{
		return ReceivingState.Get();
	}

	FAussSendingState* GetSendingState()
	{
		return SendingState.Get();
	}

	const FAussSendingState* GetSendingState() const
	{
		return SendingState.Get();
	}
};


class FAussLayoutHelper
{
private:
	friend class FAussLayout;

	TMap<UObject*, TSharedPtr<FAussChangelistMgr>>	ReplicationChangeListMap;

public:
	FAussLayoutHelper() {}
	~FAussLayoutHelper() {}

	TMap<TWeakObjectPtr<UObject>, TSharedPtr<FAussLayout>, FDefaultSetAllocator, TWeakObjectPtrMapKeyFuncs<TWeakObjectPtr<UObject>, TSharedPtr<FAussLayout> > >	RepLayoutMap;
public:
	TSharedPtr<FAussLayout>	GetObjectClassRepLayout(UClass* InClass);
	TSharedPtr<FAussChangelistMgr> GetReplicationChangeListMgr(UObject* Object);
};


class FAussLayout : public FGCObject, public TSharedFromThis<FAussLayout>
{
private:
	friend struct FAussStateStaticBuffer;

	FAussLayout();

public:
	virtual ~FAussLayout();

	int32 ShadowDataBufferSize;

	/** All Layout Commands. */
	TArray<FAussLayoutCmd> Cmds;

	/** Top level Layout Commands. */
	TArray<FAussParentCmd> Parents;

	TArray<FAussHandleToCmdIndex> BaseHandleToCmdIndex;

	UStruct* Owner;

	TMap<FAussLayoutCmd*, TArray<FAussLayoutCmd>> NetSerializeLayouts;

	static TSharedPtr<FAussLayout> CreateFromClass(UClass* InObjectClass);

	void InitFromClass(UClass* InObjectClass);

	void BuildHandleToCmdIndexTable_r(
		const int32 CmdStart,
		const int32 CmdEnd,
		TArray<FAussHandleToCmdIndex>& HandleToCmdIndex);

	void MergeChangeList(
		const FConstAussObjectDataBuffer Data,
		const TArray<uint16>& Dirty1,
		const TArray<uint16>& Dirty2,
		TArray<uint16>& MergedDirty) const;

	void MergeChangeList_r(
		FAussHandleIterator& RepHandleIterator1,
		FAussHandleIterator& RepHandleIterator2,
		const FConstAussObjectDataBuffer SourceData,
		TArray<uint16>& OutChanged) const;

	void PruneChangeList_r(
		FAussHandleIterator& RepHandleIterator,
		const FConstAussObjectDataBuffer SourceData,
		TArray<uint16>& OutChanged) const;

	void SendProperties(FAussSendingState* RESTRICT RepState, FAussChangelistState* RESTRICT RepChangelistState, 
		FDataStoreWriter& Ar, const FConstAussObjectDataBuffer SourceData) const;

	void SendProperties_r(
		FAussSendingState* RESTRICT RepState,
		FDataStoreWriter& Ar,
		FAussHandleIterator& HandleIterator,
		const FConstAussObjectDataBuffer SourceData,
		const int32	 ArrayDepth,
		const bool forceSend) const;

	void ReceiveProperties(FDataStoreReader& Ar, 
		const FConstAussObjectDataBuffer SourceData) const;

	void CompareProperties(
		FAussSendingState* RESTRICT RepState,
		FAussChangelistState* RESTRICT RepChangelistState,
		const FConstAussObjectDataBuffer Data) const;

	TUniquePtr<FAussState> CreateRepState(const FConstAussObjectDataBuffer Source) const;

	void InitRepStateStaticBuffer(FAussStateStaticBuffer& ShadowData, 
		const FConstAussObjectDataBuffer Source) const;

	void ConstructProperties(FAussStateStaticBuffer& ShadowData) const;

	void CopyProperties(FAussStateStaticBuffer& ShadowData, 
		const FConstAussObjectDataBuffer Source) const;

	void DestructProperties(FAussStateStaticBuffer& InShadowData) const;

	void UpdateChangelistMgr(FAussSendingState* RESTRICT RepState,
		FAussChangelistMgr& InChangelistMgr,
		const UObject* InObject,
		const uint32 ReplicationFrame,
		const bool bForceCompare) const;

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

	TSharedPtr<FAussChangelistMgr> CreateReplicationChangelistMgr(const UObject* InObject) const;

	FAussStateStaticBuffer CreateShadowBuffer(const FConstAussObjectDataBuffer Source) const;

public:

	const bool IsEmpty() const
	{
		return 0 == Parents.Num();
	}
};
