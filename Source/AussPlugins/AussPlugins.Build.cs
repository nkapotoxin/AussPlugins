// Replace me please, Auss.

/**
* Copyright here.
*/

using UnrealBuildTool;
using System.IO;

public class AussPlugins : ModuleRules
{
	private string ModulePath
	{
		get
		{
			return ModuleDirectory;
		}
	}

	private string ThirdPartyPath
	{
		get
		{
			return Path.GetFullPath(Path.Combine(ModulePath, "../../ThirdParty"));
		}
	}

	public AussPlugins(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		bEnableUndefinedIdentifierWarnings = false;
		bEnableExceptions = true;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject"});
		PrivateDependencyModuleNames.AddRange(new string[]{ "CoreUObject", "Engine", "Slate", "SlateCore"});

		PublicIncludePaths.Add(Path.Combine(ThirdPartyPath, "includes"));

		// TODO: Update cpp_redis tacopie to AussSDK later
		PublicAdditionalLibraries.Add(Path.Combine(ThirdPartyPath, "libs", "cpp_redis.lib"));
		PublicAdditionalLibraries.Add(Path.Combine(ThirdPartyPath, "libs", "tacopie.lib"));

		if (Target.Type == TargetRules.TargetType.Server)
		{
			PublicDefinitions.Add("WITH_AUSS=1");
			if (Target.Platform == UnrealTargetPlatform.Linux)
			{
				// TODO: Add linux os support
			}
			else if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				// TODO: Add windows os support
			}
		}
		else
		{
			PublicDefinitions.Add("WITH_AUSS=0");
		}
	}
}