// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SiteSyncAR : ModuleRules
{
	public SiteSyncAR(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput",
			"AugmentedReality",
			"ProceduralMeshComponent",
			"GeometryCore",
			"GeometryScriptingCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate", "SlateCore",
			"HeadMountedDisplay"
		});

		if (Target.Platform == UnrealTargetPlatform.IOS)
		{
			PrivateDependencyModuleNames.Add("AppleARKit");
			PrivateDependencyModuleNames.Add("AppleImageUtils");
		}
	}
}
