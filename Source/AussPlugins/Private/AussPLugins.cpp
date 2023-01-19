#include "AussPlugins.h"
#include "Log.h"

#define LOCTEXT_NAMESPACE "FAussPluginsModule"

void FAussPluginsModule::StartupModule()
{
#if WITH_AUSS
	UE_LOG(LogAussPlugins, Log, TEXT("FAussPluginsModule startup"));
#else
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

IMPLEMENT_MODUEL(FAussPluginsModule, AussPlugins)
