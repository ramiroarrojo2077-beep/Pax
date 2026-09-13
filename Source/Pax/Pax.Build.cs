// Copyright Pax. All Rights Reserved.

using UnrealBuildTool;

public class Pax : ModuleRules
{
	public Pax(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"PhysicsCore",
			"ChaosVehicles",
			"ChaosVehiclesCore",
			"ProceduralMeshComponent",
			"AIModule",
			"NavigationSystem",
			"GameplayTasks",
			"UMG",
			"Slate",
			"SlateCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"RenderCore",
			"Projects"
		});

		// Las funciones de runtime del wheeled vehicle movement component que
		// modulan el agarre rueda a rueda (SetWheelFrictionMultiplier y
		// compañía) aparecieron en 5.3. El modelo de neumático las usa cuando
		// están disponibles y cae a fuerzas aplicadas al chasis si no lo están,
		// de modo que el módulo compila igual en 5.1 y 5.2.
		bool bHasWheelRuntimeApi = Target.Version.MajorVersion > 5
			|| (Target.Version.MajorVersion == 5 && Target.Version.MinorVersion >= 3);
		PublicDefinitions.Add("PAX_HAS_WHEEL_RUNTIME_API=" + (bHasWheelRuntimeApi ? "1" : "0"));
	}
}
