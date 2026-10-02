using UnrealBuildTool;

public class GADE7322_POETD : ModuleRules
{
    public GADE7322_POETD(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicIncludePaths.Add(ModuleDirectory);

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "ProceduralMeshComponent",
            "UMG",
            "Slate",
            "SlateCore"
        });
    }
}