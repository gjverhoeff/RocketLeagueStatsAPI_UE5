// Copyright (c) 2026 Sumatras Studios. All Rights Reserved.

using UnrealBuildTool;

public class RocketLeagueStatsAPI : ModuleRules
{
	public RocketLeagueStatsAPI(ReadOnlyTargetRules Target) : base(Target)
	{
		IWYUSupport = IWYUSupport.Full;
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "Sockets", "Networking", "Json", "JsonUtilities" });
		PrivateDependencyModuleNames.AddRange(new string[] { "CoreUObject", "Engine", "Slate", "SlateCore" });
	}
}

