using UnrealBuildTool;

public class UmayyadStrike : ModuleRules
{
	public UmayyadStrike(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput",
			"UMG", "Slate", "SlateCore", "Niagara", "PhysicsCore", "NetCore"
		});
	}
}
