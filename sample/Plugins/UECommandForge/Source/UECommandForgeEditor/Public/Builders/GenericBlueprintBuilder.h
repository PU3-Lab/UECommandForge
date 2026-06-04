#pragma once

#include "CoreMinimal.h"
#include "Specs/BlueprintSpec.h"
#include "CommandForgeTypes.h"

class UBlueprint;
class UClass;

namespace UECommandForge
{
    struct FGenericBlueprintBuildOptions
    {
        FString FieldPrefix = TEXT("Blueprint");
        TArray<FString> ParentClassModules;
        bool bAllowReplace = false;
        bool bRequireActorParent = true;
        bool bRequireBlueprintable = true;
        bool bCompile = true;
        bool bSave = true;
    };

    class FGenericBlueprintBuilder
    {
    public:
        static bool Build(const FBlueprintSpec& Spec,
                          const FGenericBlueprintBuildOptions& Options,
                          TArray<FCommandForgeError>& OutErrors,
                          TMap<FString, FString>& OutValidation);

        static UClass* ResolveParentClass(const FString& ParentClass,
                                          const TArray<FString>& ParentClassModules);

        static bool DeleteExistingBlueprint(const FString& AssetPath,
                                            const FString& Field,
                                            TArray<FCommandForgeError>& OutErrors);
    };
}
