#include "VirtualSpaceReplicationGraph.h"

#include "Net/UnrealNetwork.h"
#include "Engine/LevelStreaming.h"
#include "EngineUtils.h"
#include "CoreGlobals.h"

#if WITH_GAMEPLAY_DEBUGGER
#include "GameplayDebuggerCategoryReplicator.h"
#endif

#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameState.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Pawn.h"
#include "Engine/LevelScriptActor.h"
#include "MyPlayerController.h"
#include "MyCharacterBase.h"

DEFINE_LOG_CATEGORY(LogVirtualSpaceReplicationGraph);

float CVar_VirtualSpaceRepGraph_DestructionInfoMaxDist = 30000.f;
static FAutoConsoleVariableRef CVarVirtualSpaceRepGraphDestructMaxDist(TEXT("VirtualSpaceRepGraph.DestructInfo.MaxDist"), CVar_VirtualSpaceRepGraph_DestructionInfoMaxDist, TEXT("Max distance (not squared) to rep destruct infos at"), ECVF_Default);

int32 CVar_VirtualSpaceRepGraph_DisplayClientLevelStreaming = 0;
static FAutoConsoleVariableRef CVarVirtualSpaceRepGraphDisplayClientLevelStreaming(TEXT("VirtualSpaceRepGraph.DisplayClientLevelStreaming"), CVar_VirtualSpaceRepGraph_DisplayClientLevelStreaming, TEXT(""), ECVF_Default);

float CVar_VirtualSpaceRepGraph_CellSize = 1000.f;
static FAutoConsoleVariableRef CVarVirtualSpaceRepGraphCellSize(TEXT("VirtualSpaceRepGraph.CellSize"), CVar_VirtualSpaceRepGraph_CellSize, TEXT(""), ECVF_Default);

float CVar_VirtualSpaceRepGraph_SpatialBiasX = -150000.f;
static FAutoConsoleVariableRef CVarVirtualSpaceRepGraphSpatialBiasX(TEXT("VirtualSpaceRepGraph.SpatialBiasX"), CVar_VirtualSpaceRepGraph_SpatialBiasX, TEXT(""), ECVF_Default);

float CVar_VirtualSpaceRepGraph_SpatialBiasY = -200000.f;
static FAutoConsoleVariableRef CVarVirtualSpaceRepSpatialBiasY(TEXT("VirtualSpaceRepGraph.SpatialBiasY"), CVar_VirtualSpaceRepGraph_SpatialBiasY, TEXT(""), ECVF_Default);

int32 CVar_VirtualSpaceRepGraph_DynamicActorFrequencyBuckets = 3;
static FAutoConsoleVariableRef CVarVirtualSpaceRepDynamicActorFrequencyBuckets(TEXT("VirtualSpaceRepGraph.DynamicActorFrequencyBuckets"), CVar_VirtualSpaceRepGraph_DynamicActorFrequencyBuckets, TEXT(""), ECVF_Default);

int32 CVar_VirtualSpaceRepGraph_DisableSpatialRebuilds = 1;
static FAutoConsoleVariableRef CVarVirtualSpaceRepDisableSpatialRebuilds(TEXT("VirtualSpaceRepGraph.DisableSpatialRebuilds"), CVar_VirtualSpaceRepGraph_DisableSpatialRebuilds, TEXT(""), ECVF_Default);

UVirtualSpaceReplicationGraph::UVirtualSpaceReplicationGraph()
{
}

void InitClassReplicationInfo(FClassReplicationInfo& Info, UClass* Class, bool bSpatialize, float ServerMaxTickRate)
{
	AActor* CDO = Class->GetDefaultObject<AActor>();
	if (bSpatialize)
	{
		Info.SetCullDistanceSquared(CDO->NetCullDistanceSquared);
		UE_LOG(LogVirtualSpaceReplicationGraph, Log, TEXT("Setting cull distance for %s to %f (%f)"), *Class->GetName(), Info.GetCullDistanceSquared(), Info.GetCullDistance());
	}

	Info.ReplicationPeriodFrame = FMath::Max<uint32>((uint32)FMath::RoundToFloat(ServerMaxTickRate / CDO->NetUpdateFrequency), 1);

	UClass* NativeClass = Class;
	while (!NativeClass->IsNative() && NativeClass->GetSuperClass() && NativeClass->GetSuperClass() != AActor::StaticClass())
	{
		NativeClass = NativeClass->GetSuperClass();
	}

	UE_LOG(LogVirtualSpaceReplicationGraph, Log, TEXT("Setting replication period for %s (%s) to %d frames (%.2f)"), *Class->GetName(), *NativeClass->GetName(), Info.ReplicationPeriodFrame, CDO->NetUpdateFrequency);
}

void UVirtualSpaceReplicationGraph::ResetGameWorldState()
{
	Super::ResetGameWorldState();

	AlwaysRelevantStreamingLevelActors.Empty();

	for (UNetReplicationGraphConnection* ConnManager : Connections)
	{
		for (UReplicationGraphNode* ConnectionNode : ConnManager->GetConnectionGraphNodes())
		{
			if (UVirtualSpaceReplicationGraphNode_AlwaysRelevant_ForConnection* AlwaysRelevantConnectionNode = Cast<UVirtualSpaceReplicationGraphNode_AlwaysRelevant_ForConnection>(ConnectionNode))
			{
				AlwaysRelevantConnectionNode->ResetGameWorldState();
			}
		}
	}

	for (UNetReplicationGraphConnection* ConnManager : PendingConnections)
	{
		for (UReplicationGraphNode* ConnectionNode : ConnManager->GetConnectionGraphNodes())
		{
			if (UVirtualSpaceReplicationGraphNode_AlwaysRelevant_ForConnection* AlwaysRelevantConnectionNode = Cast<UVirtualSpaceReplicationGraphNode_AlwaysRelevant_ForConnection>(ConnectionNode))
			{
				AlwaysRelevantConnectionNode->ResetGameWorldState();
			}
		}
	}
}

void UVirtualSpaceReplicationGraph::InitGlobalActorClassSettings()
{
	Super::InitGlobalActorClassSettings();

	auto AddInfo = [&](UClass* Class, EClassRepNodeMapping Mapping) { ClassRepNodePolicies.Set(Class, Mapping); };

	AddInfo(ALevelScriptActor::StaticClass(), EClassRepNodeMapping::NotRouted);				
	AddInfo(APlayerState::StaticClass(), EClassRepNodeMapping::NotRouted);				
	AddInfo(AReplicationGraphDebugActor::StaticClass(), EClassRepNodeMapping::NotRouted);				
	AddInfo(AInfo::StaticClass(), EClassRepNodeMapping::RelevantAllConnections);	

#if WITH_GAMEPLAY_DEBUGGER
	AddInfo(AGameplayDebuggerCategoryReplicator::StaticClass(), EClassRepNodeMapping::NotRouted);
#endif

	TArray<UClass*> AllReplicatedClasses;

	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		AActor* ActorCDO = Cast<AActor>(Class->GetDefaultObject());
		if (!ActorCDO || !ActorCDO->GetIsReplicated())
		{
			continue;
		}

		if (Class->GetName().StartsWith(TEXT("SKEL_")) || Class->GetName().StartsWith(TEXT("REINST_")))
		{
			continue;
		}

		AllReplicatedClasses.Add(Class);

		if (ClassRepNodePolicies.Contains(Class, false))
		{
			continue;
		}

		auto ShouldSpatialize = [](const AActor* CDO)
		{
			return CDO->GetIsReplicated() && (!(CDO->bAlwaysRelevant || CDO->bOnlyRelevantToOwner || CDO->bNetUseOwnerRelevancy));
		};

		auto GetLegacyDebugStr = [](const AActor* CDO)
		{
			return FString::Printf(TEXT("%s [%d/%d/%d]"), *CDO->GetClass()->GetName(), CDO->bAlwaysRelevant, CDO->bOnlyRelevantToOwner, CDO->bNetUseOwnerRelevancy);
		};

		UClass* SuperClass = Class->GetSuperClass();
		if (AActor* SuperCDO = Cast<AActor>(SuperClass->GetDefaultObject()))
		{
			if (SuperCDO->GetIsReplicated() == ActorCDO->GetIsReplicated()
				&& SuperCDO->bAlwaysRelevant == ActorCDO->bAlwaysRelevant
				&& SuperCDO->bOnlyRelevantToOwner == ActorCDO->bOnlyRelevantToOwner
				&& SuperCDO->bNetUseOwnerRelevancy == ActorCDO->bNetUseOwnerRelevancy
				)
			{
				continue;
			}

			if (ShouldSpatialize(ActorCDO) == false && ShouldSpatialize(SuperCDO) == true)
			{
				UE_LOG(LogVirtualSpaceReplicationGraph, Log, TEXT("Adding %s to NonSpatializedChildClasses. (Parent: %s)"), *GetLegacyDebugStr(ActorCDO), *GetLegacyDebugStr(SuperCDO));
				NonSpatializedChildClasses.Add(Class);
			}
		}

		if (ShouldSpatialize(ActorCDO))
		{
			AddInfo(Class, EClassRepNodeMapping::Spatialize_Dynamic);
		}
		else if (ActorCDO->bAlwaysRelevant && !ActorCDO->bOnlyRelevantToOwner)
		{
			AddInfo(Class, EClassRepNodeMapping::RelevantAllConnections);
		}
	}

	TArray<UClass*> ExplicitlySetClasses;
	auto SetClassInfo = [&](UClass* Class, const FClassReplicationInfo& Info) { GlobalActorReplicationInfoMap.SetClassInfo(Class, Info); ExplicitlySetClasses.Add(Class); };

	FClassReplicationInfo PawnClassRepInfo;
	PawnClassRepInfo.DistancePriorityScale = 1.f;
	PawnClassRepInfo.StarvationPriorityScale = 1.f;
	PawnClassRepInfo.ActorChannelFrameTimeout = 4;
	PawnClassRepInfo.SetCullDistanceSquared(1000.f * 1000.f); // TODO(nkaptx): Change me please.
	SetClassInfo(APawn::StaticClass(), PawnClassRepInfo);

	FClassReplicationInfo PlayerStateRepInfo;
	PlayerStateRepInfo.DistancePriorityScale = 0.f;
	PlayerStateRepInfo.ActorChannelFrameTimeout = 0;
	SetClassInfo(APlayerState::StaticClass(), PlayerStateRepInfo);

	UReplicationGraphNode_ActorListFrequencyBuckets::DefaultSettings.ListSize = 12;

	for (UClass* ReplicatedClass : AllReplicatedClasses)
	{
		if (ExplicitlySetClasses.FindByPredicate([&](const UClass* SetClass) { return ReplicatedClass->IsChildOf(SetClass); }) != nullptr)
		{
			continue;
		}

		const bool bClassIsSpatialized = IsSpatialized(ClassRepNodePolicies.GetChecked(ReplicatedClass));

		FClassReplicationInfo ClassInfo;
		InitClassReplicationInfo(ClassInfo, ReplicatedClass, bClassIsSpatialized, NetDriver->NetServerMaxTickRate);
		GlobalActorReplicationInfoMap.SetClassInfo(ReplicatedClass, ClassInfo);
	}


	UE_LOG(LogVirtualSpaceReplicationGraph, Log, TEXT(""));
	UE_LOG(LogVirtualSpaceReplicationGraph, Log, TEXT("Class Routing Map: "));
	UEnum* Enum = StaticEnum<EClassRepNodeMapping>();
	for (auto ClassMapIt = ClassRepNodePolicies.CreateIterator(); ClassMapIt; ++ClassMapIt)
	{
		UClass* Class = CastChecked<UClass>(ClassMapIt.Key().ResolveObjectPtr());
		const EClassRepNodeMapping Mapping = ClassMapIt.Value();

		UClass* ParentNativeClass = GetParentNativeClass(Class);
		const EClassRepNodeMapping* ParentMapping = ClassRepNodePolicies.Get(ParentNativeClass);
		if (ParentMapping && Class != ParentNativeClass && Mapping == *ParentMapping)
		{
			continue;
		}

		UE_LOG(LogVirtualSpaceReplicationGraph, Log, TEXT("  %s (%s) -> %s"), *Class->GetName(), *GetNameSafe(ParentNativeClass), *Enum->GetNameStringByValue(static_cast<uint32>(Mapping)));
	}

	UE_LOG(LogVirtualSpaceReplicationGraph, Log, TEXT(""));
	UE_LOG(LogVirtualSpaceReplicationGraph, Log, TEXT("Class Settings Map: "));
	FClassReplicationInfo DefaultValues;
	for (auto ClassRepInfoIt = GlobalActorReplicationInfoMap.CreateClassMapIterator(); ClassRepInfoIt; ++ClassRepInfoIt)
	{
		UClass* Class = CastChecked<UClass>(ClassRepInfoIt.Key().ResolveObjectPtr());
		const FClassReplicationInfo& ClassInfo = ClassRepInfoIt.Value();
		UE_LOG(LogVirtualSpaceReplicationGraph, Log, TEXT("  %s (%s) -> %s"), *Class->GetName(), *GetNameSafe(GetParentNativeClass(Class)), *ClassInfo.BuildDebugStringDelta());
	}


	DestructInfoMaxDistanceSquared = CVar_VirtualSpaceRepGraph_DestructionInfoMaxDist * CVar_VirtualSpaceRepGraph_DestructionInfoMaxDist;

	//	TODO(nkaptx): Register for game code callbacks.

#if WITH_GAMEPLAY_DEBUGGER
	AGameplayDebuggerCategoryReplicator::NotifyDebuggerOwnerChange.AddUObject(this, &UVirtualSpaceReplicationGraph::OnGameplayDebuggerOwnerChange);
#endif
}

void UVirtualSpaceReplicationGraph::InitGlobalGraphNodes()
{
	GridNode = CreateNewNode<UReplicationGraphNode_GridSpatialization2D>();
	GridNode->CellSize = CVar_VirtualSpaceRepGraph_CellSize;
	GridNode->SpatialBias = FVector2D(CVar_VirtualSpaceRepGraph_SpatialBiasX, CVar_VirtualSpaceRepGraph_SpatialBiasY);

	if (CVar_VirtualSpaceRepGraph_DisableSpatialRebuilds)
	{
		GridNode->AddSpatialRebuildBlacklistClass(AActor::StaticClass()); 
	}

	AddGlobalGraphNode(GridNode);

	AlwaysRelevantNode = CreateNewNode<UReplicationGraphNode_ActorList>();
	AddGlobalGraphNode(AlwaysRelevantNode);

	UVirtualSpaceReplicationGraphNode_PlayerStateFrequencyLimiter* PlayerStateNode = CreateNewNode<UVirtualSpaceReplicationGraphNode_PlayerStateFrequencyLimiter>();
	AddGlobalGraphNode(PlayerStateNode);
}

void UVirtualSpaceReplicationGraph::InitConnectionGraphNodes(UNetReplicationGraphConnection* RepGraphConnection)
{
	Super::InitConnectionGraphNodes(RepGraphConnection);

	UVirtualSpaceReplicationGraphNode_AlwaysRelevant_ForConnection* AlwaysRelevantConnectionNode = CreateNewNode<UVirtualSpaceReplicationGraphNode_AlwaysRelevant_ForConnection>();

	RepGraphConnection->OnClientVisibleLevelNameAdd.AddUObject(AlwaysRelevantConnectionNode, &UVirtualSpaceReplicationGraphNode_AlwaysRelevant_ForConnection::OnClientLevelVisibilityAdd);
	RepGraphConnection->OnClientVisibleLevelNameRemove.AddUObject(AlwaysRelevantConnectionNode, &UVirtualSpaceReplicationGraphNode_AlwaysRelevant_ForConnection::OnClientLevelVisibilityRemove);

	AddConnectionGraphNode(AlwaysRelevantConnectionNode, RepGraphConnection);
}

EClassRepNodeMapping UVirtualSpaceReplicationGraph::GetMappingPolicy(UClass* Class)
{
	EClassRepNodeMapping* PolicyPtr = ClassRepNodePolicies.Get(Class);
	EClassRepNodeMapping Policy = PolicyPtr ? *PolicyPtr : EClassRepNodeMapping::NotRouted;
	return Policy;
}

void UVirtualSpaceReplicationGraph::RouteAddNetworkActorToNodes(const FNewReplicatedActorInfo& ActorInfo, FGlobalActorReplicationInfo& GlobalInfo)
{
	EClassRepNodeMapping Policy = GetMappingPolicy(ActorInfo.Class);
	switch (Policy)
	{
	case EClassRepNodeMapping::NotRouted:
	{
		break;
	}

	case EClassRepNodeMapping::RelevantAllConnections:
	{
		if (ActorInfo.StreamingLevelName == NAME_None)
		{
			AlwaysRelevantNode->NotifyAddNetworkActor(ActorInfo);
		}
		else
		{
			FActorRepListRefView& RepList = AlwaysRelevantStreamingLevelActors.FindOrAdd(ActorInfo.StreamingLevelName);
			RepList.ConditionalAdd(ActorInfo.Actor);
		}
		break;
	}

	case EClassRepNodeMapping::Spatialize_Static:
	{
		GridNode->AddActor_Static(ActorInfo, GlobalInfo);
		break;
	}

	case EClassRepNodeMapping::Spatialize_Dynamic:
	{
		GridNode->AddActor_Dynamic(ActorInfo, GlobalInfo);
		break;
	}

	case EClassRepNodeMapping::Spatialize_Dormancy:
	{
		GridNode->AddActor_Dormancy(ActorInfo, GlobalInfo);
		break;
	}
	};
}

void UVirtualSpaceReplicationGraph::RouteRemoveNetworkActorToNodes(const FNewReplicatedActorInfo& ActorInfo)
{
	EClassRepNodeMapping Policy = GetMappingPolicy(ActorInfo.Class);
	switch (Policy)
	{
	case EClassRepNodeMapping::NotRouted:
	{
		break;
	}

	case EClassRepNodeMapping::RelevantAllConnections:
	{
		if (ActorInfo.StreamingLevelName == NAME_None)
		{
			AlwaysRelevantNode->NotifyRemoveNetworkActor(ActorInfo);
		}
		else
		{
			FActorRepListRefView& RepList = AlwaysRelevantStreamingLevelActors.FindChecked(ActorInfo.StreamingLevelName);
			if (RepList.RemoveFast(ActorInfo.Actor) == false)
			{
				UE_LOG(LogVirtualSpaceReplicationGraph, Warning, TEXT("Actor %s was not found in AlwaysRelevantStreamingLevelActors list. LevelName: %s"), *GetActorRepListTypeDebugString(ActorInfo.Actor), *ActorInfo.StreamingLevelName.ToString());
			}
		}
		break;
	}

	case EClassRepNodeMapping::Spatialize_Static:
	{
		GridNode->RemoveActor_Static(ActorInfo);
		break;
	}

	case EClassRepNodeMapping::Spatialize_Dynamic:
	{
		GridNode->RemoveActor_Dynamic(ActorInfo);
		break;
	}

	case EClassRepNodeMapping::Spatialize_Dormancy:
	{
		GridNode->RemoveActor_Dormancy(ActorInfo);
		break;
	}
	};
}

#if WITH_EDITOR
#define CHECK_WORLDS(X) if(X->GetWorld() != GetWorld()) return;
#else
#define CHECK_WORLDS(X)
#endif

#if WITH_GAMEPLAY_DEBUGGER
void UVirtualSpaceReplicationGraph::OnGameplayDebuggerOwnerChange(AGameplayDebuggerCategoryReplicator* Debugger, APlayerController* OldOwner)
{
	auto GetAlwaysRelevantForConnectionNode = [&](APlayerController* Controller) -> UVirtualSpaceReplicationGraphNode_AlwaysRelevant_ForConnection*
	{
		if (OldOwner)
		{
			if (UNetConnection* NetConnection = OldOwner->GetNetConnection())
			{
				if (UNetReplicationGraphConnection* GraphConnection = FindOrAddConnectionManager(NetConnection))
				{
					for (UReplicationGraphNode* ConnectionNode : GraphConnection->GetConnectionGraphNodes())
					{
						if (UVirtualSpaceReplicationGraphNode_AlwaysRelevant_ForConnection* AlwaysRelevantConnectionNode = Cast<UVirtualSpaceReplicationGraphNode_AlwaysRelevant_ForConnection>(ConnectionNode))
						{
							return AlwaysRelevantConnectionNode;
						}
					}

				}
			}
		}

		return nullptr;
	};

	if (UVirtualSpaceReplicationGraphNode_AlwaysRelevant_ForConnection* AlwaysRelevantConnectionNode = GetAlwaysRelevantForConnectionNode(OldOwner))
	{
		AlwaysRelevantConnectionNode->GameplayDebugger = nullptr;
	}

	if (UVirtualSpaceReplicationGraphNode_AlwaysRelevant_ForConnection* AlwaysRelevantConnectionNode = GetAlwaysRelevantForConnectionNode(Debugger->GetReplicationOwner()))
	{
		AlwaysRelevantConnectionNode->GameplayDebugger = Debugger;
	}
}
#endif

void UVirtualSpaceReplicationGraphNode_AlwaysRelevant_ForConnection::ResetGameWorldState()
{
	AlwaysRelevantStreamingLevelsNeedingReplication.Empty();
}

void UVirtualSpaceReplicationGraphNode_AlwaysRelevant_ForConnection::GatherActorListsForConnection(const FConnectionGatherActorListParameters& Params)
{
	QUICK_SCOPE_CYCLE_COUNTER(UVirtualSpaceReplicationGraphNode_AlwaysRelevant_ForConnection_GatherActorListsForConnection);

	UVirtualSpaceReplicationGraph* VirtualSpaceGraph = CastChecked<UVirtualSpaceReplicationGraph>(GetOuter());

	ReplicationActorList.Reset();

	auto ResetActorCullDistance = [&](AActor* ActorToSet, AActor*& LastActor) {

		if (ActorToSet != LastActor)
		{
			LastActor = ActorToSet;

			UE_LOG(LogVirtualSpaceReplicationGraph, Verbose, TEXT("Setting pawn cull distance to 0. %s"), *ActorToSet->GetName());
			FConnectionReplicationActorInfo& ConnectionActorInfo = Params.ConnectionManager.ActorInfoMap.FindOrAdd(ActorToSet);
			ConnectionActorInfo.SetCullDistanceSquared(0.f);
		}
	};

	for (const FNetViewer& CurViewer : Params.Viewers)
	{
		ReplicationActorList.ConditionalAdd(CurViewer.InViewer);
		ReplicationActorList.ConditionalAdd(CurViewer.ViewTarget);

		if (AMyPlayerController* PC = Cast<AMyPlayerController>(CurViewer.InViewer))
		{
			const bool bReplicatePS = (Params.ConnectionManager.ConnectionOrderNum % 2) == (Params.ReplicationFrameNum % 2);
			if (bReplicatePS)
			{
				if (APlayerState* PS = PC->PlayerState)
				{
					if (!bInitializedPlayerState)
					{
						bInitializedPlayerState = true;
						FConnectionReplicationActorInfo& ConnectionActorInfo = Params.ConnectionManager.ActorInfoMap.FindOrAdd(PS);
						ConnectionActorInfo.ReplicationPeriodFrame = 1;
					}

					ReplicationActorList.ConditionalAdd(PS);
				}
			}

			FAlwaysRelevantActorInfo* LastData = PastRelevantActors.FindByKey<UNetConnection*>(CurViewer.Connection);

			if (LastData == nullptr)
			{
				FAlwaysRelevantActorInfo NewActorInfo;
				NewActorInfo.Connection = CurViewer.Connection;
				LastData = &(PastRelevantActors[PastRelevantActors.Add(NewActorInfo)]);
			}

			check(LastData != nullptr);

			if (AMyCharacterBase* Pawn = Cast<AMyCharacterBase>(PC->GetPawn()))
			{
				ResetActorCullDistance(Pawn, static_cast<AActor*&>(LastData->LastViewer));

				if (Pawn != CurViewer.ViewTarget)
				{
					ReplicationActorList.ConditionalAdd(Pawn);
				}

				//int32 InventoryCount = Pawn->GetInventoryCount();
				//for (int32 i = 0; i < InventoryCount; ++i)
				//{
				//	AVirtualSpaceWeapon* Weapon = Pawn->GetInventoryWeapon(i);
				//	if (Weapon)
				//	{
				//		ReplicationActorList.ConditionalAdd(Weapon);
				//	}
				//}
			}

			if (AMyCharacterBase* ViewTargetPawn = Cast<AMyCharacterBase>(CurViewer.ViewTarget))
			{
				ResetActorCullDistance(ViewTargetPawn, static_cast<AActor*&>(LastData->LastViewTarget));
			}
		}
	}

	PastRelevantActors.RemoveAll([&](FAlwaysRelevantActorInfo& RelActorInfo) {
		return RelActorInfo.Connection == nullptr;
		});

	Params.OutGatheredReplicationLists.AddReplicationActorList(ReplicationActorList);

	FPerConnectionActorInfoMap& ConnectionActorInfoMap = Params.ConnectionManager.ActorInfoMap;

	TMap<FName, FActorRepListRefView>& AlwaysRelevantStreamingLevelActors = VirtualSpaceGraph->AlwaysRelevantStreamingLevelActors;

	for (int32 Idx = AlwaysRelevantStreamingLevelsNeedingReplication.Num() - 1; Idx >= 0; --Idx)
	{
		const FName& StreamingLevel = AlwaysRelevantStreamingLevelsNeedingReplication[Idx];

		FActorRepListRefView* Ptr = AlwaysRelevantStreamingLevelActors.Find(StreamingLevel);
		if (Ptr == nullptr)
		{
			UE_CLOG(CVar_VirtualSpaceRepGraph_DisplayClientLevelStreaming > 0, LogVirtualSpaceReplicationGraph, Display, TEXT("CLIENTSTREAMING Removing %s from AlwaysRelevantStreamingLevelActors because FActorRepListRefView is null. %s "), *StreamingLevel.ToString(), *Params.ConnectionManager.GetName());
			AlwaysRelevantStreamingLevelsNeedingReplication.RemoveAtSwap(Idx, 1, false);
			continue;
		}

		FActorRepListRefView& RepList = *Ptr;

		if (RepList.Num() > 0)
		{
			bool bAllDormant = true;
			for (FActorRepListType Actor : RepList)
			{
				FConnectionReplicationActorInfo& ConnectionActorInfo = ConnectionActorInfoMap.FindOrAdd(Actor);
				if (ConnectionActorInfo.bDormantOnConnection == false)
				{
					bAllDormant = false;
					break;
				}
			}

			if (bAllDormant)
			{
				UE_CLOG(CVar_VirtualSpaceRepGraph_DisplayClientLevelStreaming > 0, LogVirtualSpaceReplicationGraph, Display, TEXT("CLIENTSTREAMING All AlwaysRelevant Actors Dormant on StreamingLevel %s for %s. Removing list."), *StreamingLevel.ToString(), *Params.ConnectionManager.GetName());
				AlwaysRelevantStreamingLevelsNeedingReplication.RemoveAtSwap(Idx, 1, false);
			}
			else
			{
				UE_CLOG(CVar_VirtualSpaceRepGraph_DisplayClientLevelStreaming > 0, LogVirtualSpaceReplicationGraph, Display, TEXT("CLIENTSTREAMING Adding always Actors on StreamingLevel %s for %s because it has at least one non dormant actor"), *StreamingLevel.ToString(), *Params.ConnectionManager.GetName());
				Params.OutGatheredReplicationLists.AddReplicationActorList(RepList);
			}
		}
		else
		{
			UE_LOG(LogVirtualSpaceReplicationGraph, Warning, TEXT("UVirtualSpaceReplicationGraphNode_AlwaysRelevant_ForConnection::GatherActorListsForConnection - empty RepList %s"), *Params.ConnectionManager.GetName());
		}

	}

#if WITH_GAMEPLAY_DEBUGGER
	if (GameplayDebugger)
	{
		ReplicationActorList.ConditionalAdd(GameplayDebugger);
	}
#endif
}

void UVirtualSpaceReplicationGraphNode_AlwaysRelevant_ForConnection::OnClientLevelVisibilityAdd(FName LevelName, UWorld* StreamingWorld)
{
	UE_CLOG(CVar_VirtualSpaceRepGraph_DisplayClientLevelStreaming > 0, LogVirtualSpaceReplicationGraph, Display, TEXT("CLIENTSTREAMING ::OnClientLevelVisibilityAdd - %s"), *LevelName.ToString());
	AlwaysRelevantStreamingLevelsNeedingReplication.Add(LevelName);
}

void UVirtualSpaceReplicationGraphNode_AlwaysRelevant_ForConnection::OnClientLevelVisibilityRemove(FName LevelName)
{
	UE_CLOG(CVar_VirtualSpaceRepGraph_DisplayClientLevelStreaming > 0, LogVirtualSpaceReplicationGraph, Display, TEXT("CLIENTSTREAMING ::OnClientLevelVisibilityRemove - %s"), *LevelName.ToString());
	AlwaysRelevantStreamingLevelsNeedingReplication.Remove(LevelName);
}

void UVirtualSpaceReplicationGraphNode_AlwaysRelevant_ForConnection::LogNode(FReplicationGraphDebugInfo& DebugInfo, const FString& NodeName) const
{
	DebugInfo.Log(NodeName);
	DebugInfo.PushIndent();
	LogActorRepList(DebugInfo, NodeName, ReplicationActorList);

	for (const FName& LevelName : AlwaysRelevantStreamingLevelsNeedingReplication)
	{
		UVirtualSpaceReplicationGraph* VirtualSpaceGraph = CastChecked<UVirtualSpaceReplicationGraph>(GetOuter());
		if (FActorRepListRefView* RepList = VirtualSpaceGraph->AlwaysRelevantStreamingLevelActors.Find(LevelName))
		{
			LogActorRepList(DebugInfo, FString::Printf(TEXT("AlwaysRelevant StreamingLevel List: %s"), *LevelName.ToString()), *RepList);
		}
	}

	DebugInfo.PopIndent();
}

UVirtualSpaceReplicationGraphNode_PlayerStateFrequencyLimiter::UVirtualSpaceReplicationGraphNode_PlayerStateFrequencyLimiter()
{
	bRequiresPrepareForReplicationCall = true;
}

void UVirtualSpaceReplicationGraphNode_PlayerStateFrequencyLimiter::PrepareForReplication()
{
	QUICK_SCOPE_CYCLE_COUNTER(UVirtualSpaceReplicationGraphNode_PlayerStateFrequencyLimiter_GlobalPrepareForReplication);

	ReplicationActorLists.Reset();
	ForceNetUpdateReplicationActorList.Reset();

	ReplicationActorLists.AddDefaulted();
	FActorRepListRefView* CurrentList = &ReplicationActorLists[0];

	for (TActorIterator<APlayerState> It(GetWorld()); It; ++It)
	{
		APlayerState* PS = *It;
		if (IsActorValidForReplicationGather(PS) == false)
		{
			continue;
		}

		if (CurrentList->Num() >= TargetActorsPerFrame)
		{
			ReplicationActorLists.AddDefaulted();
			CurrentList = &ReplicationActorLists.Last();
		}

		CurrentList->Add(PS);
	}
}

void UVirtualSpaceReplicationGraphNode_PlayerStateFrequencyLimiter::GatherActorListsForConnection(const FConnectionGatherActorListParameters& Params)
{
	const int32 ListIdx = Params.ReplicationFrameNum % ReplicationActorLists.Num();
	Params.OutGatheredReplicationLists.AddReplicationActorList(ReplicationActorLists[ListIdx]);

	if (ForceNetUpdateReplicationActorList.Num() > 0)
	{
		Params.OutGatheredReplicationLists.AddReplicationActorList(ForceNetUpdateReplicationActorList);
	}
}

void UVirtualSpaceReplicationGraphNode_PlayerStateFrequencyLimiter::LogNode(FReplicationGraphDebugInfo& DebugInfo, const FString& NodeName) const
{
	DebugInfo.Log(NodeName);
	DebugInfo.PushIndent();

	int32 i = 0;
	for (const FActorRepListRefView& List : ReplicationActorLists)
	{
		LogActorRepList(DebugInfo, FString::Printf(TEXT("Bucket[%d]"), i++), List);
	}

	DebugInfo.PopIndent();
}

void UVirtualSpaceReplicationGraph::PrintRepNodePolicies()
{
	UEnum* Enum = StaticEnum<EClassRepNodeMapping>();
	if (!Enum)
	{
		return;
	}

	GLog->Logf(TEXT("===================================="));
	GLog->Logf(TEXT("VirtualSpace Replication Routing Policies"));
	GLog->Logf(TEXT("===================================="));

	for (auto It = ClassRepNodePolicies.CreateIterator(); It; ++It)
	{
		FObjectKey ObjKey = It.Key();

		EClassRepNodeMapping Mapping = It.Value();

		GLog->Logf(TEXT("%-40s --> %s"), *GetNameSafe(ObjKey.ResolveObjectPtr()), *Enum->GetNameStringByValue(static_cast<uint32>(Mapping)));
	}
}

FAutoConsoleCommandWithWorldAndArgs VirtualSpacePrintRepNodePoliciesCmd(TEXT("VirtualSpaceRepGraph.PrintRouting"), TEXT("Prints how actor classes are routed to RepGraph nodes"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			for (TObjectIterator<UVirtualSpaceReplicationGraph> It; It; ++It)
			{
				It->PrintRepNodePolicies();
			}
		})
);

FAutoConsoleCommandWithWorldAndArgs ChangeFrequencyBucketsCmd(TEXT("VirtualSpaceRepGraph.FrequencyBuckets"), TEXT("Resets frequency bucket count."), FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray< FString >& Args, UWorld* World)
	{
		int32 Buckets = 1;
		if (Args.Num() > 0)
		{
			LexTryParseString<int32>(Buckets, *Args[0]);
		}

		UE_LOG(LogVirtualSpaceReplicationGraph, Display, TEXT("Setting Frequency Buckets to %d"), Buckets);
		for (TObjectIterator<UReplicationGraphNode_ActorListFrequencyBuckets> It; It; ++It)
		{
			UReplicationGraphNode_ActorListFrequencyBuckets* Node = *It;
			Node->SetNonStreamingCollectionSize(Buckets);
		}
	}));
