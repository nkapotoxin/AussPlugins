#include "AussPlugins.h"
#include "Log.h"
#include "AussStore.h"

#define LOCTEXT_NAMESPACE "FAussPluginsModule"

void FAussPluginsModule::StartupModule()
{
#if WITH_AUSS
	UE_LOG(LogAussPlugins, Log, TEXT("FAussPluginsModule startup"));
	AussStore::JsonTest();
#else
	AussStore::JsonTest();
#endif
}

void FAussPluginsModule::ShutdownModule()
{
#if WITH_AUSS
	UE_LOG(LogAussPlugins, Log, TEXT("FAussPluginsModule shutdown"));
#else
#endif		
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAussPluginsModule, AussPlugins)
