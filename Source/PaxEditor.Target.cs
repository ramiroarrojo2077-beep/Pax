// Copyright Pax. All Rights Reserved.

using UnrealBuildTool;

public class PaxEditorTarget : TargetRules
{
	public PaxEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("Pax");
	}
}
