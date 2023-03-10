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

		PublicDefinitions.AddRange(
			new[]
			{
				"USE_IMPORT_EXPORT",
				"AUSS_RUNTIME_EXPORTS"
			});

		PublicIncludePaths.AddRange(
			new[]
			{
				// ... add public include paths required here ...
				Path.Combine(ModuleDirectory, "AussSdk/include"),
				Path.Combine(ModuleDirectory, "AussSdk/source")
			});

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "JsonUtilities", "Json"});
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