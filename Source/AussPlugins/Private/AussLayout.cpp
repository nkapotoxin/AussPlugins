#pragma once

#include "Log.h"
#include "AussLayout.h"

FAussLayout::FAussLayout()
	: Owner(nullptr)
{}

FAussLayout::~FAussLayout()
{
}

struct FInitFromPropertySharedParams
{
	TArray<FRepLayoutCmd>& Cmds;
	const UNetConnection* ServerConnection;
	const int32 ParentIndex;
	FRepParentCmd& Parent;
	bool bHasObjectProperties = false;
	bool bHasNetSerializeProperties = false;
	TMap<int32, TArray<FRepLayoutCmd>>* NetSerializeLayouts = nullptr;
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

static FName NAME_Vector_NetQuantize100(TEXT("Vector_NetQuantize100"));
static FName NAME_Vector_NetQuantize10(TEXT("Vector_NetQuantize10"));
static FName NAME_Vector_NetQuantizeNormal(TEXT("Vector_NetQuantizeNormal"));
static FName NAME_Vector_NetQuantize(TEXT("Vector_NetQuantize"));
static FName NAME_UniqueNetIdRepl(TEXT("UniqueNetIdRepl"));
static FName NAME_RepMovement(TEXT("RepMovement"));

bool GbTrackNetSerializeObjectReferences = false;

enum class ERepBuildType
{
	Class,
	Function,
	Struct
};

template<ERepBuildType BuildType>
static int32 InitFromProperty_r(
	FInitFromPropertySharedParams& SharedParams,
	FInitFromPropertyStackParams StackParams);

TSharedPtr<FAussLayout> FAussLayout::CreateFromClass(UClass* InClass)
{
	TSharedPtr<FAussLayout> aussLayout = MakeShareable<FAussLayout>(new FAussLayout());
	aussLayout->InitFromClass(InClass);
	return aussLayout;
}

static FORCEINLINE uint16 AddParentProperty(
	TArray<FRepParentCmd>& Parents,
	FProperty* Property,
	int32 ArrayIndex)
{
	return Parents.Emplace(Property, ArrayIndex);
}

template<ERepBuildType BuildType>
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

	const uint32 StructChecksum = GetRepLayoutCmdCompatibleChecksum(SharedParams, StackParams);

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

template<ERepBuildType BuildType>
static FORCEINLINE const int32 GetOffsetForProperty(const FProperty& Property)
{
	return Property.GetOffset_ForGC();
}

template<>
const FORCEINLINE int32 GetOffsetForProperty<ERepBuildType::Function>(const FProperty& Property)
{
	return Property.GetOffset_ForUFunction();
}

static uint32 GetRepLayoutCmdCompatibleChecksum(
	const FProperty* Property,
	const UNetConnection* ServerConnection,
	const uint32			StaticArrayIndex,
	const uint32			InChecksum)
{
	// TODO
	return 0;
}

static uint32 GetRepLayoutCmdCompatibleChecksum(
	FInitFromPropertySharedParams& SharedParams,
	const FInitFromPropertyStackParams& StackParams)
{
	return GetRepLayoutCmdCompatibleChecksum(StackParams.Property, SharedParams.ServerConnection, StackParams.StaticArrayIndex, StackParams.ParentChecksum);
}

static uint32 AddPropertyCmd(
	FInitFromPropertySharedParams& SharedParams,
	const FInitFromPropertyStackParams& StackParams)
{
	FRepLayoutCmd& Cmd = SharedParams.Cmds.AddZeroed_GetRef();

	Cmd.Property = StackParams.Property;
	Cmd.Type = ERepLayoutCmdType::Property;		// Initially set to generic type
	Cmd.Offset = StackParams.Offset;
	Cmd.ElementSize = Cmd.Property->ElementSize;
	Cmd.RelativeHandle = StackParams.RelativeHandle;
	Cmd.ParentIndex = SharedParams.ParentIndex;
	Cmd.CompatibleChecksum = GetRepLayoutCmdCompatibleChecksum(SharedParams, StackParams);

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
		Cmd.Flags |= ERepLayoutCmdFlags::IsStruct;

		if (Struct->GetFName() == NAME_Vector)
		{
			Cmd.Type = ERepLayoutCmdType::PropertyVector;
		}
		else if (Struct->GetFName() == NAME_Rotator)
		{
			Cmd.Type = ERepLayoutCmdType::PropertyRotator;
		}
		else if (Struct->GetFName() == NAME_Plane)
		{
			Cmd.Type = ERepLayoutCmdType::PropertyPlane;
		}
		else if (Struct->GetFName() == NAME_Vector_NetQuantize100)
		{
			Cmd.Type = ERepLayoutCmdType::PropertyVector100;
		}
		else if (Struct->GetFName() == NAME_Vector_NetQuantize10)
		{
			Cmd.Type = ERepLayoutCmdType::PropertyVector10;
		}
		else if (Struct->GetFName() == NAME_Vector_NetQuantizeNormal)
		{
			Cmd.Type = ERepLayoutCmdType::PropertyVectorNormal;
		}
		else if (Struct->GetFName() == NAME_Vector_NetQuantize)
		{
			Cmd.Type = ERepLayoutCmdType::PropertyVectorQ;
		}
		else if (Struct->GetFName() == NAME_UniqueNetIdRepl)
		{
			Cmd.Type = ERepLayoutCmdType::PropertyNetId;
		}
		else if (Struct->GetFName() == NAME_RepMovement)
		{
			Cmd.Type = ERepLayoutCmdType::RepMovement;
		}
		else if (StackParams.bNetSerializeStructWithObjects)
		{
			Cmd.Type = ERepLayoutCmdType::NetSerializeStructWithObjectReferences;
		}
		else
		{
			UE_LOG(LogRep, VeryVerbose, TEXT("AddPropertyCmd: Falling back to default type for property [%s]"), *Cmd.Property->GetFullName());
		}
	}
	else if (UnderlyingProperty->IsA(FBoolProperty::StaticClass()))
	{
		const FBoolProperty* BoolProperty = static_cast<FBoolProperty*>(UnderlyingProperty);
		Cmd.Type = BoolProperty->IsNativeBool() ? ERepLayoutCmdType::PropertyNativeBool : ERepLayoutCmdType::PropertyBool;
	}
	else if (UnderlyingProperty->IsA(FFloatProperty::StaticClass()))
	{
		Cmd.Type = ERepLayoutCmdType::PropertyFloat;
	}
	else if (UnderlyingProperty->IsA(FIntProperty::StaticClass()))
	{
		Cmd.Type = ERepLayoutCmdType::PropertyInt;
	}
	else if (UnderlyingProperty->IsA(FByteProperty::StaticClass()))
	{
		Cmd.Type = ERepLayoutCmdType::PropertyByte;
	}
	else if (UnderlyingProperty->IsA(FObjectPropertyBase::StaticClass()))
	{
		SharedParams.bHasObjectProperties = true;
		if (UnderlyingProperty->IsA(FSoftObjectProperty::StaticClass()))
		{
			Cmd.Type = ERepLayoutCmdType::PropertySoftObject;
		}
		else if (UnderlyingProperty->IsA(FWeakObjectProperty::StaticClass()))
		{
			Cmd.Type = ERepLayoutCmdType::PropertyWeakObject;
		}
		else
		{
			Cmd.Type = ERepLayoutCmdType::PropertyObject;
		}
	}
	else if (UnderlyingProperty->IsA(FNameProperty::StaticClass()))
	{
		Cmd.Type = ERepLayoutCmdType::PropertyName;
	}
	else if (UnderlyingProperty->IsA(FUInt32Property::StaticClass()))
	{
		Cmd.Type = ERepLayoutCmdType::PropertyUInt32;
	}
	else if (UnderlyingProperty->IsA(FUInt64Property::StaticClass()))
	{
		Cmd.Type = ERepLayoutCmdType::PropertyUInt64;
	}
	else if (UnderlyingProperty->IsA(FStrProperty::StaticClass()))
	{
		Cmd.Type = ERepLayoutCmdType::PropertyString;
	}
	else
	{
		UE_LOG(LogRep, VeryVerbose, TEXT("AddPropertyCmd: Falling back to default type for property [%s]"), *Cmd.Property->GetFullName());
	}

	if (Cmd.Property->SupportsNetSharedSerialization() && (Cmd.Property->GetFName() != NAME_RemoteRole))
	{
		Cmd.Flags |= ERepLayoutCmdFlags::IsSharedSerialization;
	}

	return Cmd.CompatibleChecksum;
}

static FORCEINLINE uint32 AddArrayCmd(
	FInitFromPropertySharedParams& SharedParams,
	const FInitFromPropertyStackParams StackParams)
{
	FRepLayoutCmd& Cmd = SharedParams.Cmds.AddZeroed_GetRef();

	Cmd.Type = ERepLayoutCmdType::DynamicArray;
	Cmd.Property = StackParams.Property;
	Cmd.Offset = StackParams.Offset;
	Cmd.ElementSize = static_cast<FArrayProperty*>(StackParams.Property)->Inner->ElementSize;
	Cmd.RelativeHandle = StackParams.RelativeHandle;
	Cmd.ParentIndex = SharedParams.ParentIndex;
	Cmd.CompatibleChecksum = GetRepLayoutCmdCompatibleChecksum(SharedParams, StackParams);

	return Cmd.CompatibleChecksum;
}

static FORCEINLINE void AddReturnCmd(TArray<FRepLayoutCmd>& Cmds)
{
	Cmds.AddZeroed_GetRef().Type = ERepLayoutCmdType::Return;
}

template<ERepBuildType BuildType>
static int32 InitFromProperty_r(
	FInitFromPropertySharedParams& SharedParams,
	FInitFromPropertyStackParams StackParams)
{
	if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(StackParams.Property))
	{
		const int32 CmdStart = SharedParams.Cmds.Num();
		SharedParams.Parent.Flags |= ERepParentFlags::HasDynamicArrayProperties;

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
			if (ERepBuildType::Class == BuildType && GbTrackNetSerializeObjectReferences && nullptr != SharedParams.NetSerializeLayouts && !EnumHasAnyFlags(Struct->StructFlags, STRUCT_IdenticalNative))
			{
				const int32 PrevCmdNum = SharedParams.Cmds.Num();

				TArray<FRepLayoutCmd> TempCmds;
				TArray<FRepLayoutCmd>* NewCmds = &TempCmds;

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
			ERepLayoutCmdType::Property == SharedParams.Cmds.Last().Type)
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
	TMap<int32, TArray<FRepLayoutCmd>> TempNetSerializeLayouts;

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
		RelativeHandle = InitFromProperty_r<ERepBuildType::Class>(SharedParams, StackParams);
		Parents[ParentHandle].CmdEnd = Cmds.Num();
		Parents[ParentHandle].Flags |= ERepParentFlags::IsConditional;
		Parents[ParentHandle].Offset = GetOffsetForProperty<ERepBuildType::Class>(*Property) + ParentOffset;

		if (Parents[i].CmdEnd > Parents[i].CmdStart)
		{
			check(Cmds[Parents[i].CmdStart].Offset >= LastOffset);		//>= since bool's can be combined
			LastOffset = Cmds[Parents[i].CmdStart].Offset;
		}
	}

	AddReturnCmd(Cmds);
	Owner = InObjectClass;
}

void FAussLayout::SendProperties(FDataStoreWriter& Ar, const FConstRepObjectDataBuffer SourceData) const
{
	for (int32 i = 0; i < Cmds.Num(); i++)
	{
		const FRepLayoutCmd& Cmd = Cmds[i];
		const FRepParentCmd& ParentCmd = Parents[Cmd.ParentIndex];
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

			const FConstRepObjectDataBuffer Data = (SourceData + Cmd);
			if (Cmd.Type == ERepLayoutCmdType::PropertyString)
			{
				Ar.Serialize((const FString*)Data.Data, i);
			}
			else if (Cmd.Type == ERepLayoutCmdType::PropertyInt)
			{
				const FString tmp = FString::FromInt(*(const int32*)Data.Data);
				Ar.Serialize(&tmp, i);
			}
			else if (Cmd.Type == ERepLayoutCmdType::PropertyFloat)
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

void FAussLayout::ReceiveProperties(FDataStoreReader& Ar, const FConstRepObjectDataBuffer SourceData) const
{
	for (TPair<int32, FString> elem : *Ar.GetProperties())
	{
		const FRepLayoutCmd& Cmd = Cmds[elem.Key];
		const FRepParentCmd& ParentCmd = Parents[Cmd.ParentIndex];
		if (Cmd.Property != nullptr && ParentCmd.Property != nullptr)
		{
			const FConstRepObjectDataBuffer Data = (SourceData + Cmd);
			if (Cmd.Type == ERepLayoutCmdType::PropertyString)
			{
				FString* dataAddress = (FString*)Data.Data;
				*dataAddress = (FString)elem.Value;
			}
			else if (Cmd.Type == ERepLayoutCmdType::PropertyInt)
			{
				int32* dataAddress = (int32*)Data.Data;
				*dataAddress = FCString::Atoi(*elem.Value);
			}
			else if (Cmd.Type == ERepLayoutCmdType::PropertyFloat)
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
	for (FRepParentCmd& Parent : Parents)
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
