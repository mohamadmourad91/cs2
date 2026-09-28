using UnrealBuildTool;

public class UmayyadStrikeEditorTarget : TargetRules
{
	public UmayyadStrikeEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("UmayyadStrike");
	}
}
