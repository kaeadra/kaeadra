using UnrealBuildTool;

public class IronSiegeEditorTarget : TargetRules
{
	public IronSiegeEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("IronSiege");
	}
}
