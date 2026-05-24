// Copyright (c) 2026 James Russo and Cole. All rights reserved. SiteSync AR is proprietary software — see LICENSE.

using System.IO;
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

			// CoreLocation for our GetDeviceGeoLocation shim (bypasses UE's
			// LocationServicesIOSImpl which calls requestAlwaysAuthorization —
			// iOS now denies that as "legacy on-demand" for new apps).
			PublicFrameworks.Add("CoreLocation");

			// UPL injects NSLocation*UsageDescription into Info.plist.
			// IOSRuntimeSettings.AdditionalPlistData (ini) is overwritten by
			// AppleARKit_IOS_UPL.xml in UE 5.6, so we use the same mechanism.
			AdditionalPropertiesForReceipt.Add("IOSPlugin",
				Path.Combine(ModuleDirectory, "../../Build/IOS/SiteSyncAR_IOS_UPL.xml"));
		}
	}
}
