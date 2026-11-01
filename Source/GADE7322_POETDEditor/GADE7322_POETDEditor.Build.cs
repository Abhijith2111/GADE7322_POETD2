using UnrealBuildTool;

public class GADE7322_POETDEditor : ModuleRules
{
    public GADE7322_POETDEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "UnrealEd",
            "UMG",
            "UMGEditor",
            "BlueprintGraph",
            "KismetCompiler",
            "Slate",
            "SlateCore",
            "AssetRegistry",
            "GADE7322_POETD"
        });
    }
}
