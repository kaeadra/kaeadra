using UnrealBuildTool;

public class IronSiegeTarget : TargetRules
{
	public IronSiegeTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("IronSiege");
	}
}
