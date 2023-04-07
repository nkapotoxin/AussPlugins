#include "AussLayout.h"
#include "Log.h"

FAussLayout::FAussLayout()
	: Owner(nullptr)
{}

FAussLayout::~FAussLayout()
{
}

struct FInitFromPropertySharedParams
{
	TArray<FAussLayoutCmd>& Cmds;
	const UNetConnection* ServerConnection;
	const int32 ParentIndex;
	FAussParentCmd& Parent;
	bool bHasObjectProperties = false;
	bool bHasNetSerializeProperties = false;
	TMap<int32, TArray<FAussLayoutCmd>>* NetSerializeLayouts = nullptr;
};

struct FInitFromPropertyStackParams
{
	FProperty* Property;
	int32 Offset;
	int32 RelativeHandle;
	uint32 ParentChecksum;
	int32 StaticArrayIndex;
	FName RecursingNetSerializeStruct = NAME_None;
	bool bNetSerializeStructWithObjects = false;
};

struct FAussComparePropertiesSharedParams
{
	const bool bForceFail;
	const TArray<FAussParentCmd>& Parents;
	const TArray<FAussLayoutCmd>& Cmds;
	FAussSendingState* const RepState;
	FAussChangelistState* const RepChangelistState;
	const TMap<FAussLayoutCmd*, TArray<FAussLayoutCmd>>& NetSerializeLayouts;
	const bool bValidateProperties = false;
	const bool bIsNetworkProfilerActive = false;
	const bool bChangedNetOwner = false;
};

struct FAussComparePropertiesStackParams
{
	const FConstAussObjectDataBuffer Data;
	FAussShadowDataBuffer ShadowData;
	TArray<uint16>& Changed;
};

static FName NAME_Vector_NetQuantize100(TEXT("Vector_NetQuantize100"));
static FName NAME_Vector_NetQuantize10(TEXT("Vector_NetQuantize10"));
static FName NAME_Vector_NetQuantizeNormal(TEXT("Vector_NetQuantizeNormal"));
static FName NAME_Vector_NetQuantize(TEXT("Vector_NetQuantize"));
static FName NAME_UniqueNetIdRepl(TEXT("UniqueNetIdRepl"));
static FName NAME_RepMovement(TEXT("RepMovement"));

bool GbTrackNetSerializeObjectReferences = false;

enum class EAussBuildType
{
	Class,
	Function,
	Struct
};

#define ENABLE_PROPERTY_CHECKSUMS

#define USE_CUSTOM_COMPARE

#ifdef USE_CUSTOM_COMPARE
static FORCEINLINE bool CompareBool(
	const FAussLayoutCmd& Cmd,
	const void* A,
	const void* B)
{
	return Cmd.Property->Identical(A, B);
}

static FORCEINLINE bool CompareObject(
	const FAussLayoutCmd& Cmd,
	const void* A,
	const void* B)
{
	FObjectPropertyBase* ObjProperty = CastFieldChecked<FObjectPropertyBase>(Cmd.Property);

	UObject* ObjectA = ObjProperty->GetObjectPropertyValue(A);
	UObject* ObjectB = ObjProperty->GetObjectPropertyValue(B);

	return ObjectA == ObjectB;
}

static FORCEINLINE bool CompareSoftObject(
	const FAussLayoutCmd& Cmd,
	const void* A,
	const void* B)
{
	return Cmd.Property->Identical(A, B);
}

static FORCEINLINE bool CompareWeakObject(
	const FAussLayoutCmd& Cmd,
	const void* A,
	const void* B)
{
	const FWeakObjectProperty* const WeakObjectProperty = CastFieldChecked<FWeakObjectProperty>(Cmd.Property);
	const FWeakObjectPtr ObjectA = WeakObjectProperty->GetPropertyValue(A);
	const FWeakObjectPtr ObjectB = WeakObjectProperty->GetPropertyValue(B);

	return ObjectA.HasSameIndexAndSerialNumber(ObjectB);
}

static FORCEINLINE bool CompareNetSerializeStructWithObjectProperties(
	const TArray<FAussLayoutCmd>& Cmds,
	const TMap<FAussLayoutCmd*, TArray<FAussLayoutCmd>>& NetSerializeLayouts,
	const int32 CmdStart,
	const int32 CmdEnd,
	const void* A,
	const void* B);

template<typename T>
bool CompareValue(const T* A, const T* B)
{
	return *A == *B;
}

template<typename T>
bool CompareValue(const void* A, const void* B)
{
	return CompareValue((T*)A, (T*)B);
}

static FORCEINLINE bool PropertiesAreIdenticalNative(
	const FAussLayoutCmd& Cmd,
	const void* A,
	const void* B,
	const TMap<FAussLayoutCmd*, TArray<FAussLayoutCmd>>& NetSerializeLayouts)
{
	switch (Cmd.Type)
	{
	case EAussLayoutCmdType::PropertyBool:
		return CompareBool(Cmd, A, B);

	case EAussLayoutCmdType::PropertyNativeBool:
		return CompareValue<bool>(A, B);

	case EAussLayoutCmdType::PropertyByte:
		return CompareValue<uint8>(A, B);

	case EAussLayoutCmdType::PropertyFloat:
		return CompareValue<float>(A, B);

	case EAussLayoutCmdType::PropertyInt:
		return CompareValue<int32>(A, B);

	case EAussLayoutCmdType::PropertyName:
		return CompareValue<FName>(A, B);

	case EAussLayoutCmdType::PropertyObject:
		return CompareObject(Cmd, A, B);

	case EAussLayoutCmdType::PropertySoftObject:
		return CompareSoftObject(Cmd, A, B);

	case EAussLayoutCmdType::PropertyWeakObject:
		return CompareWeakObject(Cmd, A, B);

	case EAussLayoutCmdType::PropertyUInt32:
		return CompareValue<uint32>(A, B);

	case EAussLayoutCmdType::PropertyUInt64:
		return CompareValue<uint64>(A, B);

	case EAussLayoutCmdType::PropertyVector:
		return CompareValue<FVector>(A, B);

	case EAussLayoutCmdType::PropertyVector100:
		return CompareValue<FVector_NetQuantize100>(A, B);

	case EAussLayoutCmdType::PropertyVectorQ:
		return CompareValue<FVector_NetQuantize>(A, B);

	case EAussLayoutCmdType::PropertyVectorNormal:
		return CompareValue<FVector_NetQuantizeNormal>(A, B);

	case EAussLayoutCmdType::PropertyVector10:
		return CompareValue<FVector_NetQuantize10>(A, B);

	case EAussLayoutCmdType::PropertyPlane:
		return CompareValue<FPlane>(A, B);

	case EAussLayoutCmdType::PropertyRotator:
		return CompareValue<FRotator>(A, B);

	case EAussLayoutCmdType::PropertyNetId:
		return CompareValue<FUniqueNetIdRepl>(A, B);

	case EAussLayoutCmdType::RepMovement:
		return CompareValue<FRepMovement>(A, B);

	case EAussLayoutCmdType::PropertyString:
		return CompareValue<FString>(A, B);

	case EAussLayoutCmdType::NetSerializeStructWithObjectReferences:
		return CompareNetSerializeStructWithObjectProperties(NetSerializeLayouts.FindChecked(&Cmd), NetSerializeLayouts, 0, INDEX_NONE, A, B);

	case EAussLayoutCmdType::Property:
		return Cmd.Property->Identical(A, B);

	default:
		UE_LOG(LogRep, Fatal, TEXT("PropertiesAreIdentical: Unsupported type! %i (%s)"), (uint8)Cmd.Type, *Cmd.Property->GetName());
	}

	return false;
}

static FORCEINLINE bool CompareNetSerializeStructWithObjectProperties(
	const TArray<FAussLayoutCmd>& Cmds,
	const TMap<FAussLayoutCmd*, TArray<FAussLayoutCmd>>& NetSerializeLayouts,
	const int32 CmdStart,
	const int32 CmdEnd,
	const void* A,
	const void* B)
{
	const int32 RealCmdEnd = (CmdEnd == INDEX_NONE) ? Cmds.Num() : CmdEnd;
	for (int32 CmdIndex = 0; CmdIndex < RealCmdEnd; ++CmdIndex)
	{
		const FAussLayoutCmd& Cmd = Cmds[CmdIndex];
		check(Cmd.Type != EAussLayoutCmdType::Return);

		if (EAussLayoutCmdType::DynamicArray == Cmd.Type)
		{
			FScriptArrayHelper AArray((FArrayProperty*)Cmd.Property, ((const uint8*)A + Cmd.Offset));
			FScriptArrayHelper BArray((FArrayProperty*)Cmd.Property, ((const uint8*)B + Cmd.Offset));

			if (AArray.Num() == BArray.Num())
			{
				for (int32 ArrayIndex = 0; ArrayIndex < AArray.Num(); ++ArrayIndex)
				{
					if (!CompareNetSerializeStructWithObjectProperties(Cmds, NetSerializeLayouts, CmdIndex + 1, Cmd.EndCmd - 1, AArray.GetRawPtr(ArrayIndex), BArray.GetRawPtr(ArrayIndex)))
					{
						return false;
					}
				}

				// The -1 to handle the ++ in the for loop
				CmdIndex = Cmd.EndCmd - 1;
			}
			else
			{
				return false;
			}
		}
		else if (!PropertiesAreIdenticalNative(Cmd, (const uint8*)A + Cmd.Offset, (const uint8*)B + Cmd.Offset, NetSerializeLayouts))
		{
			return false;
		}
	}

	return true;
}

static FORCEINLINE bool PropertiesAreIdentical(
	const FAussLayoutCmd& Cmd,
	const void* A,
	const void* B,
	const TMap<FAussLayoutCmd*, TArray<FAussLayoutCmd>>& NetSerializeLayouts)
{
	const bool bIsIdentical = PropertiesAreIdenticalNative(Cmd, A, B, NetSerializeLayouts);
#if 0
	// Sanity check result
	if (bIsIdentical != Cmd.Property->Identical(A, B))
	{
		UE_LOG(LogRep, Fatal, TEXT("PropertiesAreIdentical: Result mismatch! (%s)"), *Cmd.Property->GetFullName());
	}
#endif
	return bIsIdentical;
}
#else
static FORCEINLINE bool PropertiesAreIdentical(
	const FAussLayoutCmd& Cmd,
	const void* A,
	const void* B,
	const TMap<FAussLayoutCmd*, TArray<FAussLayoutCmd>>& NetSerializeLayouts)
{
	return Cmd.Property->Identical(A, B);
}
#endif

static FORCEINLINE void StoreProperty(const FAussLayoutCmd& Cmd, void* A, const void* B)
{
	Cmd.Property->CopySingleValue(A, B);
}


template<EAussBuildType BuildType>
static FORCEINLINE const int32 GetOffsetForProperty(const FProperty& Property)
{
	return Property.GetOffset_ForGC();
}

template<>
const FORCEINLINE int32 GetOffsetForProperty<EAussBuildType::Function>(const FProperty& Property)
{
	return Property.GetOffset_ForUFunction();
}

static uint16 CompareProperties_r(
	const FAussComparePropertiesSharedParams& SharedParams,
	FAussComparePropertiesStackParams& StackParams,
	const uint16 CmdStart,
	const uint16 CmdEnd,
	uint16 Handle)
{
	for (int32 CmdIndex = CmdStart; CmdIndex < CmdEnd; ++CmdIndex)
	{
		const FAussLayoutCmd& Cmd = SharedParams.Cmds[CmdIndex];

		if (Cmd.Property->GetName() == "ping")
		{
			continue;
		}

		check(Cmd.Type != EAussLayoutCmdType::Return);

		++Handle;

		const FConstAussObjectDataBuffer Data = StackParams.Data + Cmd;
		FAussShadowDataBuffer ShadowData = StackParams.ShadowData + Cmd;

		// later to deal with array
		if (Cmd.Type != EAussLayoutCmdType::DynamicArray)
		{
			if (SharedParams.bForceFail || !PropertiesAreIdentical(Cmd, ShadowData.Data, Data.Data, SharedParams.NetSerializeLayouts))
			{
				// modify cache
				StoreProperty(Cmd, ShadowData.Data, Data.Data);
				StackParams.Changed.Add(Handle);
			}
		}
	}

	return Handle;
}

static bool CompareParentProperty(
	const int32 ParentIndex,
	const FAussComparePropertiesSharedParams& SharedParams,
	FAussComparePropertiesStackParams& StackParams)
{
	const FAussParentCmd& Parent = SharedParams.Parents[ParentIndex];
	const FAussLayoutCmd& Cmd = SharedParams.Cmds[Parent.CmdStart];
	const int32 NumChanges = StackParams.Changed.Num();

	CompareProperties_r(SharedParams, StackParams, Parent.CmdStart, Parent.CmdEnd, Cmd.RelativeHandle - 1);

	return !!(StackParams.Changed.Num() - NumChanges);
}

namespace Auss_RepLayout_Private
{
	static bool CompareParentPropertyHelper(
		const int32 ParentIndex,
		const FAussComparePropertiesSharedParams& SharedParams,
		FAussComparePropertiesStackParams& StackParams)
	{
		const bool bDidPropertyChange = CompareParentProperty(ParentIndex, SharedParams, StackParams);
		return bDidPropertyChange;
	}
}

static void CompareParentProperties(
	const FAussComparePropertiesSharedParams& SharedParams,
	FAussComparePropertiesStackParams& StackParams)
{
	check(StackParams.ShadowData);

	for (int32 ParentIndex = 0; ParentIndex < SharedParams.Parents.Num(); ++ParentIndex)
	{
		Auss_RepLayout_Private::CompareParentPropertyHelper(ParentIndex, SharedParams, StackParams);
	}
}

template<EAussBuildType BuildType>
static int32 InitFromProperty_r(
	FInitFromPropertySharedParams& SharedParams,
	FInitFromPropertyStackParams StackParams);

template<bool bAlreadyAligned>
static void BuildShadowOffsets_r(TArray<FAussLayoutCmd>::TIterator& CmdIt, int32& ShadowOffset)
{
	check(CmdIt);
	check(EAussLayoutCmdType::Return != CmdIt->Type);

	if (CmdIt->Type == EAussLayoutCmdType::DynamicArray || EnumHasAnyFlags(CmdIt->Flags, EAussLayoutCmdFlags::IsStruct))
	{
		if (!bAlreadyAligned)
		{
			// Note, we can't use the Commands reported element size, as Array Commands
			// will have that set to their inner property size.

			ShadowOffset = Align(ShadowOffset, CmdIt->Property->GetMinAlignment());
			CmdIt->ShadowOffset = ShadowOffset;
			ShadowOffset += CmdIt->Property->GetSize();
		}

		if (CmdIt->Type == EAussLayoutCmdType::DynamicArray)
		{
			// Iterator into the array's layout.
			++CmdIt;

			for (; EAussLayoutCmdType::Return != CmdIt->Type; ++CmdIt)
			{
				CmdIt->ShadowOffset = CmdIt->Offset;
				BuildShadowOffsets_r</*bAlreadyAligned=*/true>(CmdIt, CmdIt->ShadowOffset);
			}

			check(CmdIt);
		}
	}
	else if (!bAlreadyAligned)
	{
		// This property is already aligned, and ShadowOffset should be correct and managed elsewhere.
		if (ShadowOffset > 0)
		{
			// Bools may be packed as bitfields, and if so they can be stored in the same location
			// as a previous property.
			if (EAussLayoutCmdType::PropertyBool == CmdIt->Type && CmdIt.GetIndex() > 0)
			{
				const TArray<FAussLayoutCmd>::TIterator PrevCmdIt = CmdIt - 1;
				if (EAussLayoutCmdType::PropertyBool == PrevCmdIt->Type && PrevCmdIt->Offset == CmdIt->Offset)
				{
					ShadowOffset = PrevCmdIt->ShadowOffset;
				}
			}
			else
			{
				ShadowOffset = Align(ShadowOffset, CmdIt->Property->GetMinAlignment());
			}
		}

		CmdIt->ShadowOffset = ShadowOffset;
		ShadowOffset += CmdIt->ElementSize;
	}
}

template<EAussBuildType ShadowType>
static void BuildShadowOffsets(
	UStruct* Owner,
	TArray<FAussParentCmd>& Parents,
	TArray<FAussLayoutCmd>& Cmds,
	int32& ShadowOffset)
{
	if (ShadowType == EAussBuildType::Class)
	{
		ShadowOffset = 0;
		if (0 != Parents.Num())
		{
			// we'll sort the Parent Commands by alignment. It should generally improve cache hit rate when iterating over commands.
			struct FAussParentCmdIndexAndAlignment
			{
				FAussParentCmdIndexAndAlignment(int32 ParentIndex, const FAussParentCmd& Parent) :
					Index(ParentIndex),
					Alignment(Parent.Property->GetMinAlignment())
				{
				}

				const int32 Index;
				const int32 Alignment;

				// Needed for sorting.
				bool operator< (const FAussParentCmdIndexAndAlignment& RHS) const
				{
					return Alignment < RHS.Alignment;
				}
			};

			TArray<FAussParentCmdIndexAndAlignment> IndexAndAlignmentArray;
			IndexAndAlignmentArray.Reserve(Parents.Num());
			for (int32 i = 0; i < Parents.Num(); ++i)
			{
				IndexAndAlignmentArray.Emplace(i, Parents[i]);
			}

			IndexAndAlignmentArray.StableSort();
			for (int32 i = 0; i < IndexAndAlignmentArray.Num(); ++i)
			{
				const FAussParentCmdIndexAndAlignment& IndexAndAlignment = IndexAndAlignmentArray[i];
				FAussParentCmd& Parent = Parents[IndexAndAlignment.Index];

				if (Parent.Property->ArrayDim > 1 || EnumHasAnyFlags(Parent.Flags, EAussParentFlags::IsStructProperty))
				{
					const int32 ArrayStartParentOffset = GetOffsetForProperty<ShadowType>(*Parent.Property);
					ShadowOffset = Align(ShadowOffset, IndexAndAlignment.Alignment);

					for (int32 j = 0; j < Parent.Property->ArrayDim; ++j, ++i)
					{
						const FAussParentCmdIndexAndAlignment& NextIndexAndAlignment = IndexAndAlignmentArray[i];
						FAussParentCmd& NextParent = Parents[NextIndexAndAlignment.Index];

						NextParent.ShadowOffset = ShadowOffset + (GetOffsetForProperty<ShadowType>(*NextParent.Property) - ArrayStartParentOffset);

						for (auto CmdIt = Cmds.CreateIterator() + NextParent.CmdStart; CmdIt.GetIndex() < NextParent.CmdEnd; ++CmdIt)
						{
							CmdIt->ShadowOffset = ShadowOffset + (CmdIt->Offset - ArrayStartParentOffset);
							BuildShadowOffsets_r</*bAlreadyAligned*/true>(CmdIt, CmdIt->ShadowOffset);
						}
					}

					--i;
					ShadowOffset += Parent.Property->GetSize();
				}
				else
				{
					check(Parent.CmdEnd > Parent.CmdStart);

					for (auto CmdIt = Cmds.CreateIterator() + Parent.CmdStart; CmdIt.GetIndex() < Parent.CmdEnd; ++CmdIt)
					{
						BuildShadowOffsets_r</*bAlreadyAligned=*/false>(CmdIt, ShadowOffset);
					}

					// We update this after we build child commands offsets, to make sure that
					// if there's any extra packing (like bitfield packing), we are aware of it.
					Parent.ShadowOffset = Cmds[Parent.CmdStart].ShadowOffset;
				}
			}
		}
	}
	else
	{
		ShadowOffset = Owner->GetPropertiesSize();

		for (auto ParentIt = Parents.CreateIterator(); ParentIt; ++ParentIt)
		{
			ParentIt->ShadowOffset = GetOffsetForProperty<ShadowType>(*ParentIt->Property);
		}

		for (auto CmdIt = Cmds.CreateIterator(); CmdIt; ++CmdIt)
		{
			CmdIt->ShadowOffset = CmdIt->Offset;
		}
	}
}


TSharedPtr<FAussLayout> FAussLayout::CreateFromClass(UClass* InClass)
{
	TSharedPtr<FAussLayout> aussLayout = MakeShareable<FAussLayout>(new FAussLayout());
	aussLayout->InitFromClass(InClass);
	return aussLayout;
}

static FORCEINLINE uint16 AddParentProperty(
	TArray<FAussParentCmd>& Parents,
	FProperty* Property,
	int32 ArrayIndex)
{
	return Parents.Emplace(Property, ArrayIndex);
}

template<EAussBuildType BuildType>
static int32 InitFromStructProperty(
	FInitFromPropertySharedParams& SharedParams,
	FInitFromPropertyStackParams StackParams,
	const FStructProperty* const StructProp,
	const UScriptStruct* const Struct)
{
	TArray<FProperty*> NetProperties;

	for (TFieldIterator<FProperty> It(Struct); It; ++It)
	{
		if ((It->PropertyFlags & CPF_RepSkip))
		{
			continue;
		}

		NetProperties.Add(*It);
	}

	// Sort NetProperties by memory offset
	struct FCompareUFieldOffsets
	{
		FORCEINLINE bool operator()(FProperty& A, FProperty& B) const
		{
			const int32 AOffset = GetOffsetForProperty<BuildType>(A);
			const int32 BOffset = GetOffsetForProperty<BuildType>(B);

			// Ensure stable sort
			if (AOffset == BOffset)
			{
				return A.GetName() < B.GetName();
			}

			return AOffset < BOffset;
		}
	};

	Sort(NetProperties.GetData(), NetProperties.Num(), FCompareUFieldOffsets());

	const uint32 StructChecksum = GetLayoutCmdCompatibleChecksum(SharedParams, StackParams);

	for (int32 i = 0; i < NetProperties.Num(); i++)
	{
		for (int32 j = 0; j < NetProperties[i]->ArrayDim; j++)
		{
			const int32 ArrayElementOffset = j * NetProperties[i]->ElementSize;

			FInitFromPropertyStackParams NewStackParams{
				/*Property=*/NetProperties[i],
				/*Offset=*/StackParams.Offset + ArrayElementOffset,
				/*RelativeHandle=*/StackParams.RelativeHandle,
				/*ParentChecksum=*/StructChecksum,
				/*StaticArrayIndex=*/j,
				/*RecursingNetSerializeStruct=*/StackParams.RecursingNetSerializeStruct
			};

			StackParams.RelativeHandle = InitFromProperty_r<BuildType>(SharedParams, NewStackParams);
		}
	}

	return StackParams.RelativeHandle;
}

static uint32 GetLayoutCmdCompatibleChecksum(
	const FProperty* Property,
	const UNetConnection* ServerConnection,
	const uint32			StaticArrayIndex,
	const uint32			InChecksum)
{
	// TODO
	return 0;
}

static uint32 GetLayoutCmdCompatibleChecksum(
	FInitFromPropertySharedParams& SharedParams,
	const FInitFromPropertyStackParams& StackParams)
{
	return GetLayoutCmdCompatibleChecksum(StackParams.Property, SharedParams.ServerConnection, StackParams.StaticArrayIndex, StackParams.ParentChecksum);
}

static uint32 AddPropertyCmd(
	FInitFromPropertySharedParams& SharedParams,
	const FInitFromPropertyStackParams& StackParams)
{
	FAussLayoutCmd& Cmd = SharedParams.Cmds.AddZeroed_GetRef();

	Cmd.Property = StackParams.Property;
	Cmd.Type = EAussLayoutCmdType::Property;		// Initially set to generic type
	Cmd.Offset = StackParams.Offset;
	Cmd.ElementSize = Cmd.Property->ElementSize;
	Cmd.RelativeHandle = StackParams.RelativeHandle;
	Cmd.ParentIndex = SharedParams.ParentIndex;
	Cmd.CompatibleChecksum = GetLayoutCmdCompatibleChecksum(SharedParams, StackParams);

	FProperty* UnderlyingProperty = Cmd.Property;
	if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(UnderlyingProperty))
	{
		UnderlyingProperty = EnumProperty->GetUnderlyingProperty();
	}

	// Try to special case to custom types we know about
	if (UnderlyingProperty->IsA(FStructProperty::StaticClass()))
	{
		FStructProperty* StructProp = CastField<FStructProperty>(UnderlyingProperty);
		UScriptStruct* Struct = StructProp->Struct;
		Cmd.Flags |= EAussLayoutCmdFlags::IsStruct;

		if (Struct->GetFName() == NAME_Vector)
		{
			Cmd.Type = EAussLayoutCmdType::PropertyVector;
		}
		else if (Struct->GetFName() == NAME_Rotator)
		{
			Cmd.Type = EAussLayoutCmdType::PropertyRotator;
		}
		else if (Struct->GetFName() == NAME_Plane)
		{
			Cmd.Type = EAussLayoutCmdType::PropertyPlane;
		}
		else if (Struct->GetFName() == NAME_Vector_NetQuantize100)
		{
			Cmd.Type = EAussLayoutCmdType::PropertyVector100;
		}
		else if (Struct->GetFName() == NAME_Vector_NetQuantize10)
		{
			Cmd.Type = EAussLayoutCmdType::PropertyVector10;
		}
		else if (Struct->GetFName() == NAME_Vector_NetQuantizeNormal)
		{
			Cmd.Type = EAussLayoutCmdType::PropertyVectorNormal;
		}
		else if (Struct->GetFName() == NAME_Vector_NetQuantize)
		{
			Cmd.Type = EAussLayoutCmdType::PropertyVectorQ;
		}
		else if (Struct->GetFName() == NAME_UniqueNetIdRepl)
		{
			Cmd.Type = EAussLayoutCmdType::PropertyNetId;
		}
		else if (Struct->GetFName() == NAME_RepMovement)
		{
			Cmd.Type = EAussLayoutCmdType::RepMovement;
		}
		else if (StackParams.bNetSerializeStructWithObjects)
		{
			Cmd.Type = EAussLayoutCmdType::NetSerializeStructWithObjectReferences;
		}
		else
		{
			UE_LOG(LogRep, VeryVerbose, TEXT("AddPropertyCmd: Falling back to default type for property [%s]"), *Cmd.Property->GetFullName());
		}
	}
	else if (UnderlyingProperty->IsA(FBoolProperty::StaticClass()))
	{
		const FBoolProperty* BoolProperty = static_cast<FBoolProperty*>(UnderlyingProperty);
		Cmd.Type = BoolProperty->IsNativeBool() ? EAussLayoutCmdType::PropertyNativeBool : EAussLayoutCmdType::PropertyBool;
	}
	else if (UnderlyingProperty->IsA(FFloatProperty::StaticClass()))
	{
		Cmd.Type = EAussLayoutCmdType::PropertyFloat;
	}
	else if (UnderlyingProperty->IsA(FIntProperty::StaticClass()))
	{
		Cmd.Type = EAussLayoutCmdType::PropertyInt;
	}
	else if (UnderlyingProperty->IsA(FByteProperty::StaticClass()))
	{
		Cmd.Type = EAussLayoutCmdType::PropertyByte;
	}
	else if (UnderlyingProperty->IsA(FObjectPropertyBase::StaticClass()))
	{
		SharedParams.bHasObjectProperties = true;
		if (UnderlyingProperty->IsA(FSoftObjectProperty::StaticClass()))
		{
			Cmd.Type = EAussLayoutCmdType::PropertySoftObject;
		}
		else if (UnderlyingProperty->IsA(FWeakObjectProperty::StaticClass()))
		{
			Cmd.Type = EAussLayoutCmdType::PropertyWeakObject;
		}
		else
		{
			Cmd.Type = EAussLayoutCmdType::PropertyObject;
		}
	}
	else if (UnderlyingProperty->IsA(FNameProperty::StaticClass()))
	{
		Cmd.Type = EAussLayoutCmdType::PropertyName;
	}
	else if (UnderlyingProperty->IsA(FUInt32Property::StaticClass()))
	{
		Cmd.Type = EAussLayoutCmdType::PropertyUInt32;
	}
	else if (UnderlyingProperty->IsA(FUInt64Property::StaticClass()))
	{
		Cmd.Type = EAussLayoutCmdType::PropertyUInt64;
	}
	else if (UnderlyingProperty->IsA(FStrProperty::StaticClass()))
	{
		Cmd.Type = EAussLayoutCmdType::PropertyString;
	}
	else
	{
		UE_LOG(LogRep, VeryVerbose, TEXT("AddPropertyCmd: Falling back to default type for property [%s]"), *Cmd.Property->GetFullName());
	}

	if (Cmd.Property->SupportsNetSharedSerialization() && (Cmd.Property->GetFName() != NAME_RemoteRole))
	{
		Cmd.Flags |= EAussLayoutCmdFlags::IsSharedSerialization;
	}

	return Cmd.CompatibleChecksum;
}

static FORCEINLINE uint32 AddArrayCmd(
	FInitFromPropertySharedParams& SharedParams,
	const FInitFromPropertyStackParams StackParams)
{
	FAussLayoutCmd& Cmd = SharedParams.Cmds.AddZeroed_GetRef();

	Cmd.Type = EAussLayoutCmdType::DynamicArray;
	Cmd.Property = StackParams.Property;
	Cmd.Offset = StackParams.Offset;
	Cmd.ElementSize = static_cast<FArrayProperty*>(StackParams.Property)->Inner->ElementSize;
	Cmd.RelativeHandle = StackParams.RelativeHandle;
	Cmd.ParentIndex = SharedParams.ParentIndex;
	Cmd.CompatibleChecksum = GetLayoutCmdCompatibleChecksum(SharedParams, StackParams);

	return Cmd.CompatibleChecksum;
}

static FORCEINLINE void AddReturnCmd(TArray<FAussLayoutCmd>& Cmds)
{
	Cmds.AddZeroed_GetRef().Type = EAussLayoutCmdType::Return;
}

template<EAussBuildType BuildType>
static int32 InitFromProperty_r(
	FInitFromPropertySharedParams& SharedParams,
	FInitFromPropertyStackParams StackParams)
{
	if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(StackParams.Property))
	{
		const int32 CmdStart = SharedParams.Cmds.Num();
		SharedParams.Parent.Flags |= EAussParentFlags::HasDynamicArrayProperties;

		++StackParams.RelativeHandle;
		StackParams.Offset += GetOffsetForProperty<BuildType>(*ArrayProp);

		const uint32 ArrayChecksum = AddArrayCmd(SharedParams, StackParams);

		FInitFromPropertyStackParams NewStackParams{
			/*Property=*/ArrayProp->Inner,
			/*Offset=*/0,
			/*RelativeHandle=*/0,
			/*ParentChecksum=*/ArrayChecksum,
			/*StaticArrayIndex=*/0,
			/*RecursingNetSerializeStruct=*/StackParams.RecursingNetSerializeStruct
		};

		InitFromProperty_r<BuildType>(SharedParams, NewStackParams);

		AddReturnCmd(SharedParams.Cmds);

		SharedParams.Cmds[CmdStart].EndCmd = SharedParams.Cmds.Num();
	}
	else if (FStructProperty* StructProp = CastField<FStructProperty>(StackParams.Property))
	{
		UScriptStruct* Struct = StructProp->Struct;

		StackParams.Offset += GetOffsetForProperty<BuildType>(*StructProp);
		if (EnumHasAnyFlags(Struct->StructFlags, STRUCT_NetSerializeNative))
		{
			UE_CLOG(EnumHasAnyFlags(Struct->StructFlags, STRUCT_NetDeltaSerializeNative), LogRep, Warning, TEXT("RepLayout InitFromProperty_r: Struct marked both NetSerialize and NetDeltaSerialize: %s"), *StructProp->GetName());

			SharedParams.bHasNetSerializeProperties = true;
			if (EAussBuildType::Class == BuildType && GbTrackNetSerializeObjectReferences && nullptr != SharedParams.NetSerializeLayouts && !EnumHasAnyFlags(Struct->StructFlags, STRUCT_IdenticalNative))
			{
				const int32 PrevCmdNum = SharedParams.Cmds.Num();

				TArray<FAussLayoutCmd> TempCmds;
				TArray<FAussLayoutCmd>* NewCmds = &TempCmds;

				FInitFromPropertyStackParams NewStackParams{
					/*Property=*/StackParams.Property,
					/*Offset=*/0,
					/*RelativeHandle=*/StackParams.RelativeHandle,
					/*ParentChecksum=*/StackParams.ParentChecksum,
					/*StaticArrayIndex=*/StackParams.StaticArrayIndex,
					/*RecursingNetSerialize=*/StructProp->GetFName()

				};

				if (StackParams.RecursingNetSerializeStruct != NAME_None)
				{
					NewCmds = &SharedParams.Cmds;
					NewStackParams.RelativeHandle = 0;
				}

				FInitFromPropertySharedParams NewSharedParams{
					/*Cmds=*/*NewCmds,
					/*ServerConnection=*/SharedParams.ServerConnection,
					/*ParentIndex=*/SharedParams.ParentIndex,
					/*Parent=*/SharedParams.Parent,
					/*bHasObjectProperties=*/false,
					/*bHasNetSerializeProperties=*/false,
					/*NetSerializeLayouts=*/SharedParams.NetSerializeLayouts
				};

				const int32 NetSerializeStructOffset = InitFromStructProperty<BuildType>(NewSharedParams, NewStackParams, StructProp, Struct);

				if (StackParams.RecursingNetSerializeStruct == NAME_None)
				{
					if (NewSharedParams.bHasObjectProperties)
					{
						SharedParams.NetSerializeLayouts->Add(SharedParams.Cmds.Num(), MoveTemp(TempCmds));
						StackParams.bNetSerializeStructWithObjects = true;
					}
				}
				else if (!NewSharedParams.bHasObjectProperties)
				{
					SharedParams.Cmds.SetNum(PrevCmdNum);
				}
				else
				{
					return NetSerializeStructOffset;
				}
			}

			++StackParams.RelativeHandle;
			AddPropertyCmd(SharedParams, StackParams);

			return StackParams.RelativeHandle;
		}


		return InitFromStructProperty<BuildType>(SharedParams, StackParams, StructProp, Struct);
	}
	else
	{
		// Add actual property
		++StackParams.RelativeHandle;
		StackParams.Offset += GetOffsetForProperty<BuildType>(*StackParams.Property);

		AddPropertyCmd(SharedParams, StackParams);

		if (StackParams.RecursingNetSerializeStruct != NAME_None &&
			EAussLayoutCmdType::Property == SharedParams.Cmds.Last().Type)
		{
			TArray<const FStructProperty*> SubProperties;
			if (StackParams.Property->ContainsObjectReference(SubProperties))
			{
				UE_LOG(LogRep, Warning,
					TEXT("InitFromProperty_r: Found NetSerialize Struct Property that contains a nested UObject reference that is not tracked for replication. StructProperty=%s, NestedProperty=%s"),
					*StackParams.RecursingNetSerializeStruct.ToString(), *StackParams.Property->GetPathName());
			}
		}
	}

	return StackParams.RelativeHandle;
}

void FAussLayout::InitFromClass(UClass* InObjectClass)
{
	const bool bIsObjectActor = InObjectClass->IsChildOf(AActor::StaticClass());

	int32 RelativeHandle = 0;
	int32 LastOffset = INDEX_NONE;
	int32 HighestCustomDeltaRepIndex = INDEX_NONE;
	TMap<int32, TArray<FAussLayoutCmd>> TempNetSerializeLayouts;

	InObjectClass->SetUpRuntimeReplicationData();
	Parents.Empty(InObjectClass->ClassReps.Num());

	for (int32 i = 0; i < InObjectClass->ClassReps.Num(); i++)
	{
		FProperty* Property = InObjectClass->ClassReps[i].Property;
		const int32 ArrayIdx = InObjectClass->ClassReps[i].Index;
		
		check(Property->PropertyFlags & CPF_Net);
		const int32 ParentHandle = AddParentProperty(Parents, Property, ArrayIdx);

		check(ParentHandle == i);
		check(Parents[i].Property->RepIndex + Parents[i].ArrayIndex == i);

		const int32 ParentOffset = Property->ElementSize * ArrayIdx;
		FInitFromPropertySharedParams SharedParams
		{
			/*Cmds=*/Cmds,
			/*ServerConnection=*/nullptr,
			/*ParentIndex=*/ParentHandle,
			/*Parent=*/Parents[ParentHandle],
			/*bHasObjectProperties=*/false,
			/*bHasNetSerializeProperties=*/false,
			/*NetSerializeLayouts=*/nullptr,
		};

		FInitFromPropertyStackParams StackParams
		{
			/*Property=*/Property,
			/*Offset=*/ParentOffset,
			/*RelativeHandle=*/RelativeHandle,
			/*ParentChecksum=*/0,
			/*StaticArrayIndex=*/ArrayIdx
		};

		Parents[ParentHandle].CmdStart = Cmds.Num();
		RelativeHandle = InitFromProperty_r<EAussBuildType::Class>(SharedParams, StackParams);
		Parents[ParentHandle].CmdEnd = Cmds.Num();
		Parents[ParentHandle].Flags |= EAussParentFlags::IsConditional;
		Parents[ParentHandle].Offset = GetOffsetForProperty<EAussBuildType::Class>(*Property) + ParentOffset;

		if (Parents[i].CmdEnd > Parents[i].CmdStart)
		{
			check(Cmds[Parents[i].CmdStart].Offset >= LastOffset);		//>= since bool's can be combined
			LastOffset = Cmds[Parents[i].CmdStart].Offset;
		}
	}

	AddReturnCmd(Cmds);

	BuildHandleToCmdIndexTable_r(0, Cmds.Num() - 1, BaseHandleToCmdIndex);

	BuildShadowOffsets<EAussBuildType::Class>(InObjectClass, Parents, Cmds, ShadowDataBufferSize);

	Owner = InObjectClass;
}

void FAussLayout::BuildHandleToCmdIndexTable_r(
	const int32 CmdStart,
	const int32 CmdEnd,
	TArray<FAussHandleToCmdIndex>& HandleToCmdIndex)
{
	for (int32 CmdIndex = CmdStart; CmdIndex < CmdEnd; CmdIndex++)
	{
		const FAussLayoutCmd& Cmd = Cmds[CmdIndex];

		check(Cmd.Type != EAussLayoutCmdType::Return);

		const int32 Index = HandleToCmdIndex.Add(FAussHandleToCmdIndex(CmdIndex));

		if (Cmd.Type == EAussLayoutCmdType::DynamicArray)
		{
			HandleToCmdIndex[Index].HandleToCmdIndex = TUniquePtr<TArray<FAussHandleToCmdIndex>>(new TArray<FAussHandleToCmdIndex>());

			TArray<FAussHandleToCmdIndex>& ArrayHandleToCmdIndex = *HandleToCmdIndex[Index].HandleToCmdIndex;

			BuildHandleToCmdIndexTable_r(CmdIndex + 1, Cmd.EndCmd - 1, ArrayHandleToCmdIndex);
			CmdIndex = Cmd.EndCmd - 1;		// The -1 to handle the ++ in the for loop
		}
	}
}

void FAussLayout::MergeChangeList(
	const FConstAussObjectDataBuffer Data,
	const TArray<uint16>& Dirty1,
	const TArray<uint16>& Dirty2,
	TArray<uint16>& MergedDirty) const
{
	check(Dirty1.Num() > 0);
	MergedDirty.Empty(1);

	if (!IsEmpty())
	{
		if (Dirty2.Num() == 0)
		{
			FAussChangelistIterator ChangelistIterator(Dirty1, 0);
			FAussHandleIterator HandleIterator(Owner, ChangelistIterator, Cmds, BaseHandleToCmdIndex, 0, 1, 0, Cmds.Num() - 1);
			PruneChangeList_r(HandleIterator, Data, MergedDirty);
		}
		else
		{
			FAussChangelistIterator ChangelistIterator1(Dirty1, 0);
			FAussHandleIterator HandleIterator1(Owner, ChangelistIterator1, Cmds, BaseHandleToCmdIndex, 0, 1, 0, Cmds.Num() - 1);

			FAussChangelistIterator ChangelistIterator2(Dirty2, 0);
			FAussHandleIterator HandleIterator2(Owner, ChangelistIterator2, Cmds, BaseHandleToCmdIndex, 0, 1, 0, Cmds.Num() - 1);

			MergeChangeList_r(HandleIterator1, HandleIterator2, Data, MergedDirty);
		}
	}

	MergedDirty.Add(0);
}

class FAussScopedIteratorArrayTracker
{
public:
	FAussScopedIteratorArrayTracker(FAussHandleIterator* InCmdIndexIterator)
	{
		CmdIndexIterator = InCmdIndexIterator;

		if (CmdIndexIterator)
		{
			ArrayChangedCount = CmdIndexIterator->ChangelistIterator.Changed[CmdIndexIterator->ChangelistIterator.ChangedIndex++];
			OldChangedIndex = CmdIndexIterator->ChangelistIterator.ChangedIndex;
		}
	}

	~FAussScopedIteratorArrayTracker()
	{
		if (CmdIndexIterator)
		{
			check(CmdIndexIterator->ChangelistIterator.ChangedIndex - OldChangedIndex <= ArrayChangedCount);
			CmdIndexIterator->ChangelistIterator.ChangedIndex = OldChangedIndex + ArrayChangedCount;
			check(CmdIndexIterator->PeekNextHandle() == 0);
			CmdIndexIterator->ChangelistIterator.ChangedIndex++;
		}
	}

	FAussHandleIterator* CmdIndexIterator;
	int32 ArrayChangedCount;
	int32 OldChangedIndex;
};

void FAussLayout::MergeChangeList_r(
	FAussHandleIterator& RepHandleIterator1,
	FAussHandleIterator& RepHandleIterator2,
	const FConstAussObjectDataBuffer SourceData,
	TArray<uint16>& OutChanged) const
{
	while (true)
	{
		const int32 NextHandle1 = RepHandleIterator1.PeekNextHandle();
		const int32 NextHandle2 = RepHandleIterator2.PeekNextHandle();

		if (NextHandle1 == 0 && NextHandle2 == 0)
		{
			// Done
			break;
		}

		if (NextHandle2 == 0)
		{
			PruneChangeList_r(RepHandleIterator1, SourceData, OutChanged);
			return;
		}
		else if (NextHandle1 == 0)
		{
			PruneChangeList_r(RepHandleIterator2, SourceData, OutChanged);
			return;
		}

		FAussHandleIterator* ActiveIterator1 = nullptr;
		FAussHandleIterator* ActiveIterator2 = nullptr;

		int32 CmdIndex = INDEX_NONE;
		int32 ArrayOffset = INDEX_NONE;

		if (NextHandle1 < NextHandle2)
		{
			if (!RepHandleIterator1.NextHandle())
			{
				// Array overflow
				break;
			}

			OutChanged.Add(NextHandle1);

			CmdIndex = RepHandleIterator1.CmdIndex;
			ArrayOffset = RepHandleIterator1.ArrayOffset;

			ActiveIterator1 = &RepHandleIterator1;
		}
		else if (NextHandle2 < NextHandle1)
		{
			if (!RepHandleIterator2.NextHandle())
			{
				// Array overflow
				break;
			}

			OutChanged.Add(NextHandle2);

			CmdIndex = RepHandleIterator2.CmdIndex;
			ArrayOffset = RepHandleIterator2.ArrayOffset;

			ActiveIterator2 = &RepHandleIterator2;
		}
		else
		{
			check(NextHandle1 == NextHandle2);

			if (!RepHandleIterator1.NextHandle())
			{
				// Array overflow
				break;
			}

			if (!ensure(RepHandleIterator2.NextHandle()))
			{
				// Array overflow
				break;
			}

			check(RepHandleIterator1.CmdIndex == RepHandleIterator2.CmdIndex);

			OutChanged.Add(NextHandle1);

			CmdIndex = RepHandleIterator1.CmdIndex;
			ArrayOffset = RepHandleIterator1.ArrayOffset;

			ActiveIterator1 = &RepHandleIterator1;
			ActiveIterator2 = &RepHandleIterator2;
		}

		const FAussLayoutCmd& Cmd = Cmds[CmdIndex];

		if (Cmd.Type == EAussLayoutCmdType::DynamicArray)
		{
			const FConstAussObjectDataBuffer Data = (SourceData + Cmd) + ArrayOffset;
			const FScriptArray* Array = (FScriptArray*)Data.Data;
			const FConstAussObjectDataBuffer ArrayData(Array->GetData());

			FAussScopedIteratorArrayTracker ArrayTracker1(ActiveIterator1);
			FAussScopedIteratorArrayTracker ArrayTracker2(ActiveIterator2);

			const int32 OriginalChangedNum = OutChanged.AddUninitialized();

			TArray<FAussHandleToCmdIndex>& ArrayHandleToCmdIndex = ActiveIterator1 ? *ActiveIterator1->HandleToCmdIndex[Cmd.RelativeHandle - 1].HandleToCmdIndex : *ActiveIterator2->HandleToCmdIndex[Cmd.RelativeHandle - 1].HandleToCmdIndex; //-V595

			if (!ActiveIterator1)
			{
				FAussHandleIterator ArrayIterator2(ActiveIterator2->Owner, ActiveIterator2->ChangelistIterator, Cmds, ArrayHandleToCmdIndex, Cmd.ElementSize, Array->Num(), CmdIndex + 1, Cmd.EndCmd - 1);
				PruneChangeList_r(ArrayIterator2, ArrayData, OutChanged);
			}
			else if (!ActiveIterator2)
			{
				FAussHandleIterator ArrayIterator1(ActiveIterator1->Owner, ActiveIterator1->ChangelistIterator, Cmds, ArrayHandleToCmdIndex, Cmd.ElementSize, Array->Num(), CmdIndex + 1, Cmd.EndCmd - 1);
				PruneChangeList_r(ArrayIterator1, ArrayData, OutChanged);
			}
			else
			{
				FAussHandleIterator ArrayIterator1(ActiveIterator1->Owner, ActiveIterator1->ChangelistIterator, Cmds, ArrayHandleToCmdIndex, Cmd.ElementSize, Array->Num(), CmdIndex + 1, Cmd.EndCmd - 1);
				FAussHandleIterator ArrayIterator2(ActiveIterator2->Owner, ActiveIterator2->ChangelistIterator, Cmds, ArrayHandleToCmdIndex, Cmd.ElementSize, Array->Num(), CmdIndex + 1, Cmd.EndCmd - 1);

				MergeChangeList_r(ArrayIterator1, ArrayIterator2, ArrayData, OutChanged);
			}

			// Patch in the jump offset
			OutChanged[OriginalChangedNum] = OutChanged.Num() - (OriginalChangedNum + 1);

			// Add the array terminator
			OutChanged.Add(0);
		}
	}
}

void FAussLayout::PruneChangeList_r(
	FAussHandleIterator& RepHandleIterator,
	const FConstAussObjectDataBuffer SourceData,
	TArray<uint16>& OutChanged) const
{
	while (RepHandleIterator.NextHandle())
	{
		OutChanged.Add(RepHandleIterator.Handle);

		const int32 CmdIndex = RepHandleIterator.CmdIndex;
		const int32 ArrayOffset = RepHandleIterator.ArrayOffset;

		const FAussLayoutCmd& Cmd = Cmds[CmdIndex];

		if (Cmd.Type == EAussLayoutCmdType::DynamicArray)
		{
			const FConstAussObjectDataBuffer Data = (SourceData + Cmd) + ArrayOffset;
			const FScriptArray* Array = (FScriptArray*)Data.Data;
			const FConstAussObjectDataBuffer ArrayData(Array->GetData());

			FAussScopedIteratorArrayTracker ArrayTracker(&RepHandleIterator);

			const int32 OriginalChangedNum = OutChanged.AddUninitialized();

			TArray<FAussHandleToCmdIndex>& ArrayHandleToCmdIndex = *RepHandleIterator.HandleToCmdIndex[Cmd.RelativeHandle - 1].HandleToCmdIndex;

			FAussHandleIterator ArrayIterator(RepHandleIterator.Owner, RepHandleIterator.ChangelistIterator, Cmds, ArrayHandleToCmdIndex, Cmd.ElementSize, Array->Num(), CmdIndex + 1, Cmd.EndCmd - 1);
			PruneChangeList_r(ArrayIterator, ArrayData, OutChanged);

			// Patch in the jump offset
			OutChanged[OriginalChangedNum] = OutChanged.Num() - (OriginalChangedNum + 1);

			// Add the array terminator
			OutChanged.Add(0);
		}
	}
}

void FAussLayout::SendProperties_r(
	FAussSendingState* RESTRICT RepState,
	FDataStoreWriter& Ar,
	FAussHandleIterator& HandleIterator,
	const FConstAussObjectDataBuffer SourceData,
	const int32	ArrayDepth,
	const bool ForceSend) const
{
	if (ForceSend)
	{
		// Force send all properties
		for (int32 i = 0; i < Cmds.Num(); i++)
		{
			const FAussLayoutCmd& Cmd = Cmds[i];
			const FAussParentCmd& ParentCmd = Parents[Cmd.ParentIndex];
			if (Cmd.Property != nullptr && ParentCmd.Property != nullptr)
			{
				// Auss no need to replicate these internal properties
				const FString propertyName = ParentCmd.Property->GetName();
				if (propertyName == "bReplicateMovement" ||
					propertyName == "bHidden" ||
					propertyName == "bTearOff" ||
					propertyName == "bCanBeDamaged" ||
					propertyName == "RemoteRole" ||
					propertyName == "ReplicatedMovement" ||
					propertyName == "AttachmentReplication" ||
					propertyName == "Owner" ||
					propertyName == "Role" ||
					propertyName == "Instigator" ||
					propertyName == "bIsSpectator" ||
					propertyName == "bOnlySpectator" ||
					propertyName == "bIsABot" ||
					propertyName == "bIsInactive" ||
					propertyName == "bFromPreviousLevel" ||
					propertyName == "StartTime" ||
					propertyName == "UniqueId")
				{
					UE_LOG(LogAussPlugins, Log, TEXT("SendProperties scape internal property name: %s"), *propertyName);
					continue;
				}

				const FConstAussObjectDataBuffer Data = (SourceData + Cmd);
				if (Cmd.Type == EAussLayoutCmdType::PropertyString)
				{
					Ar.Serialize((const FString*)Data.Data, i);
				}
				else if (Cmd.Type == EAussLayoutCmdType::PropertyInt)
				{
					const FString tmp = FString::FromInt(*(const int32*)Data.Data);
					Ar.Serialize(&tmp, i);
				}
				else if (Cmd.Type == EAussLayoutCmdType::PropertyFloat)
				{
					// TODO(nkpatx): jindu optimize
					const FString tmp = FString::SanitizeFloat(*(const float*)Data.Data);
					Ar.Serialize(&tmp, i);
				}
				else
				{
					UE_LOG(LogAussPlugins, Warning, TEXT("SendProperties with not supported property name: %s, type: %d"), *propertyName, Cmd.Type);
				}
			}
		}
	}
	else
	{
		while (HandleIterator.NextHandle())
		{
			const FAussLayoutCmd& Cmd = Cmds[HandleIterator.CmdIndex];
			const FAussParentCmd& ParentCmd = Parents[Cmd.ParentIndex];

			UE_LOG(LogAussPlugins, VeryVerbose, TEXT("SendProperties_r: Parent=%d, Cmd=%d, ArrayIndex=%d"), Cmd.ParentIndex, HandleIterator.CmdIndex, HandleIterator.ArrayIndex);

			FConstAussObjectDataBuffer Data = (SourceData + Cmd) + HandleIterator.ArrayOffset;

			if (Cmd.Type == EAussLayoutCmdType::DynamicArray)
			{
				// TODO(nkaptx): Add array later
			}

			if (Cmd.Property != nullptr && ParentCmd.Property != nullptr)
			{
				// Auss no need to replicate these internal properties
				const FString propertyName = ParentCmd.Property->GetName();
				if (propertyName == "bReplicateMovement" ||
					propertyName == "bHidden" ||
					propertyName == "bTearOff" ||
					propertyName == "bCanBeDamaged" ||
					propertyName == "RemoteRole" ||
					propertyName == "ReplicatedMovement" ||
					propertyName == "AttachmentReplication" ||
					propertyName == "Owner" ||
					propertyName == "Role" ||
					propertyName == "Instigator" ||
					propertyName == "bIsSpectator" ||
					propertyName == "bOnlySpectator" ||
					propertyName == "bIsABot" ||
					propertyName == "bIsInactive" ||
					propertyName == "bFromPreviousLevel" ||
					propertyName == "StartTime" ||
					propertyName == "UniqueId")
				{
					UE_LOG(LogAussPlugins, Log, TEXT("SendProperties scape internal property name: %s"), *propertyName);
					continue;
				}

				if (Cmd.Type == EAussLayoutCmdType::PropertyString)
				{
					Ar.Serialize((const FString*)Data.Data, HandleIterator.CmdIndex);
				}
				else if (Cmd.Type == EAussLayoutCmdType::PropertyInt)
				{
					const FString tmp = FString::FromInt(*(const int32*)Data.Data);
					Ar.Serialize(&tmp, HandleIterator.CmdIndex);
				}
				else if (Cmd.Type == EAussLayoutCmdType::PropertyFloat)
				{
					// TODO(nkpatx): jindu optimize
					const FString tmp = FString::SanitizeFloat(*(const float*)Data.Data);
					Ar.Serialize(&tmp, HandleIterator.CmdIndex);
				}
				else
				{
					UE_LOG(LogAussPlugins, Warning, TEXT("SendProperties with not supported property name: %s, type: %d"), *propertyName, Cmd.Type);
				}
			}
		}
	}
}

void FAussLayout::SendProperties(FAussSendingState* RESTRICT RepState, 
	FAussChangelistState* RESTRICT RepChangelistState, 
	FDataStoreWriter& Ar, 
	const FConstAussObjectDataBuffer SourceData) const
{
	TArray<uint16> Changed;

	// Gather all change lists that are new since we last looked, and merge them all together into a single CL
	for (int32 i = 0; i < RepChangelistState->HistoryEnd; ++i)
	{
		const int32 HistoryIndex = i % FAussChangelistState::MAX_CHANGE_HISTORY;

		FAussChangedHistory& HistoryItem = RepChangelistState->ChangeHistory[HistoryIndex];

		TArray<uint16> Temp = MoveTemp(Changed);
		MergeChangeList(SourceData, HistoryItem.Changed, Temp, Changed);
	}

	FAussChangelistIterator ChangelistIterator(Changed, 0);
	FAussHandleIterator HandleIterator(Owner, ChangelistIterator, Cmds, BaseHandleToCmdIndex, 0, 1, 0, Cmds.Num() - 1);

	SendProperties_r(RepState, Ar, HandleIterator, SourceData, 0, false);
}

void FAussLayout::ReceiveProperties(FDataStoreReader& Ar, const FConstAussObjectDataBuffer SourceData) const
{
	for (TPair<int32, FString> elem : *Ar.GetProperties())
	{
		const FAussLayoutCmd& Cmd = Cmds[elem.Key];
		const FAussParentCmd& ParentCmd = Parents[Cmd.ParentIndex];
		if (Cmd.Property != nullptr && ParentCmd.Property != nullptr)
		{
			const FConstAussObjectDataBuffer Data = (SourceData + Cmd);
			if (Cmd.Type == EAussLayoutCmdType::PropertyString)
			{
				FString* dataAddress = (FString*)Data.Data;
				*dataAddress = (FString)elem.Value;
			}
			else if (Cmd.Type == EAussLayoutCmdType::PropertyInt)
			{
				int32* dataAddress = (int32*)Data.Data;
				*dataAddress = FCString::Atoi(*elem.Value);
			}
			else if (Cmd.Type == EAussLayoutCmdType::PropertyFloat)
			{
				float* dataAddress = (float*)Data.Data;
				*dataAddress = FCString::Atof(*elem.Value);
			}
			else
			{
				UE_LOG(LogAussPlugins, Warning, TEXT("UpdatePawnRepData not support parent name: %s, name: %s, type: %d, value: %s"),
					*ParentCmd.Property->GetName(), *Cmd.Property->GetName(), Cmd.Type, *elem.Value);
			}
		}
	}
}

void FAussLayout::AddReferencedObjects(FReferenceCollector& Collector)
{
	FProperty* Current = nullptr;
	for (FAussParentCmd& Parent : Parents)
	{
		Current = Parent.Property;
		if (Current != nullptr)
		{
			Current->AddReferencedObjects(Collector);
			if (Current == nullptr)
			{
				Parent.Property = nullptr;
			}
		}
	}
}

void FAussLayout::InitRepStateStaticBuffer(FAussStateStaticBuffer& ShadowData, const FConstAussObjectDataBuffer Source) const
{
	check(ShadowData.Buffer.Num() == 0);
	ShadowData.Buffer.SetNumZeroed(ShadowDataBufferSize);
	ConstructProperties(ShadowData);
	CopyProperties(ShadowData, Source);
}

void FAussLayout::ConstructProperties(FAussStateStaticBuffer& InShadowData) const
{
	FAussShadowDataBuffer ShadowData = InShadowData.GetData();
	for (const FAussParentCmd& Parent : Parents)
	{
		if (Parent.ArrayIndex == 0)
		{
			check((Parent.ShadowOffset + Parent.Property->GetSize()) <= InShadowData.Num());
			Parent.Property->InitializeValue(ShadowData + Parent);
		}
	}
}

void FAussLayout::DestructProperties(FAussStateStaticBuffer& InShadowData) const
{
	FAussShadowDataBuffer ShadowData = InShadowData.GetData();
	for (const FAussParentCmd& Parent : Parents)
	{
		if (Parent.ArrayIndex == 0)
		{
			check((Parent.ShadowOffset + Parent.Property->GetSize()) <= InShadowData.Num());
			Parent.Property->DestroyValue(ShadowData + Parent);
		}
	}

	InShadowData.Buffer.Empty();
}

void FAussLayout::CopyProperties(FAussStateStaticBuffer& InShadowData, const FConstAussObjectDataBuffer Source) const
{
	FAussShadowDataBuffer ShadowData = InShadowData.GetData();
	for (const FAussParentCmd& Parent : Parents)
	{
		if (Parent.ArrayIndex == 0)
		{
			check((Parent.ShadowOffset + Parent.Property->GetSize()) <= InShadowData.Num());
			Parent.Property->CopyCompleteValue(ShadowData + Parent, Source + Parent);
		}
	}
}

TUniquePtr<FAussState> FAussLayout::CreateRepState(const FConstAussObjectDataBuffer Source) const
{
	TUniquePtr<FAussState> RepState(new FAussState());
	FAussStateStaticBuffer StaticBuffer(AsShared());

	InitRepStateStaticBuffer(StaticBuffer, Source);
	RepState->ReceivingState.Reset(new FAussReceivingState(MoveTemp(StaticBuffer)));

	return RepState;
}


void FAussLayout::UpdateChangelistMgr(FAussSendingState* RESTRICT RepState,
	FAussChangelistMgr& InChangelistMgr,
	const UObject* InObject,
	const uint32 ReplicationFrame,
	const bool bForceCompare) const
{
	CompareProperties(RepState, &InChangelistMgr.RepChangelistState, (const uint8*)InObject);
}

TSharedPtr<FAussChangelistMgr> FAussLayout::CreateReplicationChangelistMgr(const UObject* InObject) const
{
	const uint8* ShadowStateSource = (const uint8*)InObject->GetArchetype();
	if (ShadowStateSource == nullptr)
	{
		UE_LOG(LogAussPlugins, Error, TEXT("FAussLayout::CreateReplicationChangelistMgr: Invalid object archetype, initializing shadow state to current object state: %s"), *GetFullNameSafe(InObject));
		ShadowStateSource = (const uint8*)InObject;
	}

	return MakeShareable(new FAussChangelistMgr(AsShared(), ShadowStateSource, InObject));
}

void FAussLayout::CompareProperties(
	FAussSendingState* RESTRICT RepState,
	FAussChangelistState* RESTRICT RepChangelistState,
	const FConstAussObjectDataBuffer Data) const
{
	if (IsEmpty())
	{
		return;
	}

	RepChangelistState->CompareIndex++;

	check((RepChangelistState->HistoryEnd - RepChangelistState->HistoryStart) < FAussChangelistState::MAX_CHANGE_HISTORY);
	const int32 HistoryIndex = RepChangelistState->HistoryEnd % FAussChangelistState::MAX_CHANGE_HISTORY;
	FAussChangedHistory& NewHistoryItem = RepChangelistState->ChangeHistory[HistoryIndex];

	TArray<uint16>& Changed = NewHistoryItem.Changed;
	Changed.Empty(1);

	// TODO(nkpatx): add push model optimize here
	const TBitArray<>* const LocalPushModelProperties = nullptr;

	FAussComparePropertiesSharedParams SharedParams{
		/*bForceFail=*/ false,
		Parents,
		Cmds,
		RepState,
		RepChangelistState,
		NetSerializeLayouts,
	};

	FAussComparePropertiesStackParams StackParams{
		Data,
		RepChangelistState->StaticBuffer.GetData(),
		Changed,
	};

	CompareParentProperties(SharedParams, StackParams);

	if (Changed.Num() == 0)
	{
		return;
	}

	Changed.Add(0);

	RepChangelistState->HistoryEnd++;
	RepChangelistState->SharedSerialization.Reset();

	// update change history
	return;
}

FAussStateStaticBuffer FAussLayout::CreateShadowBuffer(const FConstAussObjectDataBuffer Source) const
{
	FAussStateStaticBuffer ShadowData(AsShared());

	if (!IsEmpty())
	{
		if (ShadowDataBufferSize == 0)
		{
			UE_LOG(LogAussPlugins, Error, TEXT("FAussLayout::InitShadowData: Invalid RepLayout: %s"), *GetPathNameSafe(Owner));
		}
		else
		{
			InitRepStateStaticBuffer(ShadowData, Source);
		}
	}

	return ShadowData;
}

FAussStateStaticBuffer::~FAussStateStaticBuffer()
{
	//  TODO(nkpatx): release ptrs
}

FAussReceivingState::FAussReceivingState(FAussStateStaticBuffer&& InStaticBuffer)
	: StaticBuffer(MoveTemp(InStaticBuffer))
{}

TSharedPtr<FAussLayout>	FAussLayoutHelper::GetObjectClassRepLayout(UClass* Class)
{
	TSharedPtr<FAussLayout>* RepLayoutPtr = RepLayoutMap.Find(Class);
	if (!RepLayoutPtr)
	{
		RepLayoutPtr = &RepLayoutMap.Add(Class, FAussLayout::CreateFromClass(Class));
	}

	return *RepLayoutPtr;
}

TSharedPtr<FAussChangelistMgr> FAussLayoutHelper::GetReplicationChangeListMgr(UObject* Object)
{
	TSharedPtr<FAussChangelistMgr>* ReplicationChangeListMgrPtr = ReplicationChangeListMap.Find(Object);
	if (!ReplicationChangeListMgrPtr)
	{
		const TSharedPtr<const FAussLayout> RepLayout = GetObjectClassRepLayout(Object->GetClass());
		TSharedPtr<FAussChangelistMgr> ChangelistMgr = RepLayout->CreateReplicationChangelistMgr(Object);
		ReplicationChangeListMgrPtr = &ReplicationChangeListMap.Add(Object, ChangelistMgr);
	}

	return *ReplicationChangeListMgrPtr;
}

FAussChangelistMgr::FAussChangelistMgr(
	const TSharedRef<const FAussLayout>& InRepLayout,
	const uint8* InSource,
	const UObject* InRepresenting)
	: LastReplicationFrame(0)
	, LastInitialReplicationFrame(0)
	, RepChangelistState(InRepLayout, InSource, InRepresenting)
{}

FAussChangelistState::FAussChangelistState(
	const TSharedRef<const FAussLayout>& InRepLayout,
	const uint8* InSource,
	const UObject* InRepresenting)
	: HistoryStart(0)
	, HistoryEnd(0)
	, CompareIndex(0)
	, StaticBuffer(InRepLayout->CreateShadowBuffer(InSource))
{}

bool FAussHandleIterator::NextHandle()
{
	CmdIndex = INDEX_NONE;
	Handle = ChangelistIterator.Changed[ChangelistIterator.ChangedIndex];

	if (Handle == 0)
	{
		return false;		// Done
	}

	ChangelistIterator.ChangedIndex++;

	if (!ensureMsgf(ChangelistIterator.Changed.IsValidIndex(ChangelistIterator.ChangedIndex),
		TEXT("Attempted to access invalid iterator index: Handle=%d, ChangedIndex=%d, ChangedNum=%d, Owner=%s, LastSuccessfulCmd=%s"),
		Handle, ChangelistIterator.ChangedIndex, ChangelistIterator.Changed.Num(),
		*GetPathNameSafe(Owner),
		*((Cmds.IsValidIndex(LastSuccessfulCmdIndex) && Cmds[LastSuccessfulCmdIndex].Property) ? Cmds[LastSuccessfulCmdIndex].Property->GetPathName() : FString::FromInt(LastSuccessfulCmdIndex))))
	{
		return false;
	}

	const int32 HandleMinusOne = Handle - 1;

	ArrayIndex = (ArrayElementSize > 0 && NumHandlesPerElement > 0) ? HandleMinusOne / NumHandlesPerElement : 0;

	if (ArrayIndex >= MaxArrayIndex)
	{
		return false;
	}

	ArrayOffset = ArrayIndex * ArrayElementSize;

	const int32 RelativeHandle = HandleMinusOne - ArrayIndex * NumHandlesPerElement;

	if (!ensureMsgf(HandleToCmdIndex.IsValidIndex(RelativeHandle),
		TEXT("Attempted to access invalid RelativeHandle Index: Handle=%d, RelativeHandle=%d, NumHandlesPerElement=%d, ArrayIndex=%d, ArrayElementSize=%d, Owner=%s, LastSuccessfulCmd=%s"),
		Handle, RelativeHandle, NumHandlesPerElement, ArrayIndex, ArrayElementSize,
		*GetPathNameSafe(Owner),
		*((Cmds.IsValidIndex(LastSuccessfulCmdIndex) && Cmds[LastSuccessfulCmdIndex].Property) ? Cmds[LastSuccessfulCmdIndex].Property->GetPathName() : FString::FromInt(LastSuccessfulCmdIndex))))
	{
		return false;
	}

	CmdIndex = HandleToCmdIndex[RelativeHandle].CmdIndex;

	if (!ensureMsgf(MinCmdIndex <= CmdIndex && CmdIndex < MaxCmdIndex,
		TEXT("Attempted to access Command Index outside of iterator range: Handle=%d, RelativeHandle=%d, CmdIndex=%d, MinCmdIdx=%d, MaxCmdIdx=%d, ArrayIndex=%d, Owner=%s, LastSuccessfulCmd=%s"),
		Handle, RelativeHandle, CmdIndex, MinCmdIndex, MaxCmdIndex, ArrayIndex,
		*GetPathNameSafe(Owner),
		*((Cmds.IsValidIndex(LastSuccessfulCmdIndex) && Cmds[LastSuccessfulCmdIndex].Property) ? Cmds[LastSuccessfulCmdIndex].Property->GetPathName() : FString::FromInt(LastSuccessfulCmdIndex))))
	{
		return false;
	}

	const FAussLayoutCmd& Cmd = Cmds[CmdIndex];

	if (!ensureMsgf(Cmd.RelativeHandle - 1 == RelativeHandle,
		TEXT("Command Relative Handle does not match found Relative Handle: Handle=%d, RelativeHandle=%d, CmdIdx=%d, CmdRelativeHandle=%d, ArrayIndex=%d, Owner=%s, LastSuccessfulCmd=%s"),
		Handle, RelativeHandle, CmdIndex, Cmd.RelativeHandle, ArrayIndex,
		*GetPathNameSafe(Owner),
		*((Cmds.IsValidIndex(LastSuccessfulCmdIndex) && Cmds[LastSuccessfulCmdIndex].Property) ? Cmds[LastSuccessfulCmdIndex].Property->GetPathName() : FString::FromInt(LastSuccessfulCmdIndex))))
	{
		return false;
	}

	if (!ensureMsgf(Cmd.Type != EAussLayoutCmdType::Return,
		TEXT("Hit unexpected return handle: Handle=%d, RelativeHandle=%d, CmdIdx=%d, ArrayIndex=%d, Owner=%s, LastSuccessfulCmd=%s"),
		Handle, RelativeHandle, CmdIndex, ArrayIndex,
		*GetPathNameSafe(Owner),
		*((Cmds.IsValidIndex(LastSuccessfulCmdIndex) && Cmds[LastSuccessfulCmdIndex].Property) ? Cmds[LastSuccessfulCmdIndex].Property->GetPathName() : FString::FromInt(LastSuccessfulCmdIndex))))
	{
		return false;
	}

	LastSuccessfulCmdIndex = CmdIndex;

	return true;
}

bool FAussHandleIterator::JumpOverArray()
{
	const int32 ArrayChangedCount = ChangelistIterator.Changed[ChangelistIterator.ChangedIndex++];
	ChangelistIterator.ChangedIndex += ArrayChangedCount;

	if (!ensure(ChangelistIterator.Changed[ChangelistIterator.ChangedIndex] == 0))
	{
		return false;
	}

	ChangelistIterator.ChangedIndex++;

	return true;
}

int32 FAussHandleIterator::PeekNextHandle() const
{
	return ChangelistIterator.Changed[ChangelistIterator.ChangedIndex];
}
