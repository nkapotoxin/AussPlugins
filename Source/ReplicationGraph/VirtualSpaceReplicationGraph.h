#pragma once

#include "CoreMinimal.h"
#include "ReplicationGraph.h"
#include "VirtualSpaceReplicationGraph.generated.h"

class AMyCharacterBase;
class UReplicationGraphNode_GridSpatialization2D;
class AGameplayDebuggerCategoryReplicator;

DECLARE_LOG_CATEGORY_EXTERN( LogVirtualSpaceReplicationGraph, Display, All );

UENUM()
enum class EClassRepNodeMapping : uint32
{
	NotRouted,						
	RelevantAllConnections,			

	Spatialize_Static,				
	Spatialize_Dynamic,				
	Spatialize_Dormancy,			
};

UCLASS(transient, config=Engine)
class UVirtualSpaceReplicationGraph :public UReplicationGraph
{
	GENERATED_BODY()

public:

	UVirtualSpaceReplicationGraph();

	virtual void ResetGameWorldState() override;

	virtual void InitGlobalActorClassSettings() override;
	virtual void InitGlobalGraphNodes() override;
	virtual void InitConnectionGraphNodes(UNetReplicationGraphConnection* RepGraphConnection) override;
	virtual void RouteAddNetworkActorToNodes(const FNewReplicatedActorInfo& ActorInfo, FGlobalActorReplicationInfo& GlobalInfo) override;
	virtual void RouteRemoveNetworkActorToNodes(const FNewReplicatedActorInfo& ActorInfo) override;
	
	UPROPERTY()
	TArray<UClass*>	SpatializedClasses;

	UPROPERTY()
	TArray<UClass*> NonSpatializedChildClasses;

	UPROPERTY()
	TArray<UClass*>	AlwaysRelevantClasses;
	
	UPROPERTY()
	UReplicationGraphNode_GridSpatialization2D* GridNode;

	UPROPERTY()
	UReplicationGraphNode_ActorList* AlwaysRelevantNode;

	TMap<FName, FActorRepListRefView> AlwaysRelevantStreamingLevelActors;

#if WITH_GAMEPLAY_DEBUGGER
	void OnGameplayDebuggerOwnerChange(AGameplayDebuggerCategoryReplicator* Debugger, APlayerController* OldOwner);
#endif

	void PrintRepNodePolicies();

private:

	EClassRepNodeMapping GetMappingPolicy(UClass* Class);

	bool IsSpatialized(EClassRepNodeMapping Mapping) const { return Mapping >= EClassRepNodeMapping::Spatialize_Static; }

	TClassMap<EClassRepNodeMapping> ClassRepNodePolicies;
};

UCLASS()
class UVirtualSpaceReplicationGraphNode_AlwaysRelevant_ForConnection : public UReplicationGraphNode
{
	GENERATED_BODY()

public:

	virtual void NotifyAddNetworkActor(const FNewReplicatedActorInfo& Actor) override { }
	virtual bool NotifyRemoveNetworkActor(const FNewReplicatedActorInfo& ActorInfo, bool bWarnIfNotFound=true) override { return false; }
	virtual void NotifyResetAllNetworkActors() override { }

	virtual void GatherActorListsForConnection(const FConnectionGatherActorListParameters& Params) override;

	virtual void LogNode(FReplicationGraphDebugInfo& DebugInfo, const FString& NodeName) const override;

	void OnClientLevelVisibilityAdd(FName LevelName, UWorld* StreamingWorld);
	void OnClientLevelVisibilityRemove(FName LevelName);

	void ResetGameWorldState();

#if WITH_GAMEPLAY_DEBUGGER
	AGameplayDebuggerCategoryReplicator* GameplayDebugger = nullptr;
#endif

private:

	TArray<FName, TInlineAllocator<64> > AlwaysRelevantStreamingLevelsNeedingReplication;

	FActorRepListRefView ReplicationActorList;

	UPROPERTY()
	AActor* LastPawn = nullptr;

	UPROPERTY()
	TArray<FAlwaysRelevantActorInfo> PastRelevantActors;

	bool bInitializedPlayerState = false;
};

UCLASS()
class UVirtualSpaceReplicationGraphNode_PlayerStateFrequencyLimiter : public UReplicationGraphNode
{
	GENERATED_BODY()

	UVirtualSpaceReplicationGraphNode_PlayerStateFrequencyLimiter();

	virtual void NotifyAddNetworkActor(const FNewReplicatedActorInfo& Actor) override { }
	virtual bool NotifyRemoveNetworkActor(const FNewReplicatedActorInfo& ActorInfo, bool bWarnIfNotFound=true) override { return false; }

	virtual void GatherActorListsForConnection(const FConnectionGatherActorListParameters& Params) override;

	virtual void PrepareForReplication() override;

	virtual void LogNode(FReplicationGraphDebugInfo& DebugInfo, const FString& NodeName) const override;

	int32 TargetActorsPerFrame = 2;

private:
	
	TArray<FActorRepListRefView> ReplicationActorLists;
	FActorRepListRefView ForceNetUpdateReplicationActorList;
};