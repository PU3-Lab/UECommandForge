using UnrealBuildTool;

public class UECommandForgeEditor : ModuleRules
{
    public UECommandForgeEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core", "CoreUObject", "Engine",
            "UECommandForgeRuntime"
        });
        PrivateDependencyModuleNames.AddRange(new[]
        {
            "UnrealEd", "Json", "JsonUtilities",
            "AssetTools", "AssetRegistry",
            "BlueprintGraph", "KismetCompiler", "Kismet",
            "EditorScriptingUtilities",
            "AIModule"
            // StateTree 모듈은 Phase 4에서 추가
        });
    }
}
