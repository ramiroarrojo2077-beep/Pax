// Copyright Pax. All Rights Reserved.

using UnrealBuildTool;

public class PaxTarget : TargetRules
{
	public PaxTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("Pax");
	}
}
