using UnrealBuildTool;

public class GADE7322_POETDEditor : ModuleRules
{
    public GADE7322_POETDEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "UnrealEd",
            "UMG",
            "UMGEditor",
            "Slate",
            "SlateCore",
            "GADE7322_POETD"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "AssetRegistry",
            "Kismet"
        });

        PrivateIncludePaths.Add(System.IO.Path.Combine(ModuleDirectory, "..", "GADE7322_POETD"));
    }
}
