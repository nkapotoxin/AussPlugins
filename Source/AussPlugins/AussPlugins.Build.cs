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

		// Support no source code
		PrecompileForTargets = PrecompileTargetsType.None;
		bPrecompile = true;
		// bUsePrecompiled = true;

		PublicDefinitions.AddRange(
			new[]
			{
				"USE_IMPORT_EXPORT",
				"AUSS_RUNTIME_EXPORTS"
			});

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "JsonUtilities", "Json"});
		PrivateDependencyModuleNames.AddRange(new string[]{ "CoreUObject", "Engine", "Slate", "SlateCore"});

		PublicIncludePaths.Add(Path.Combine(ThirdPartyPath, "includes"));

		// TODO: Update AussSdkLocal to AussSDK later
		PublicAdditionalLibraries.Add(Path.Combine(ThirdPartyPath, "libs", "AussSdkLocal.lib"));

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