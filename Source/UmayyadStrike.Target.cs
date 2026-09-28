using UnrealBuildTool;

public class UmayyadStrikeTarget : TargetRules
{
	public UmayyadStrikeTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("UmayyadStrike");
	}
}
