using UnrealBuildTool;

public class IronSiege : ModuleRules
{
	public IronSiege(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] {
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "ChaosVehicles", "PhysicsCore", "UMG", "Slate", "SlateCore", "AIModule", "AudioCaptureCore", "AudioMixer", "RenderCore", "ImageCore"
		});
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}
	}
}
