#pragma once

#include "CoreMinimal.h"
#include "CommandForgeTypes.h"

class UBlueprint;

namespace UECommandForge
{
    struct FBlueprintComponentSpec
    {
        FString Name;
        FString Class;
    };

    class FBlueprintComponentApplier
    {
    public:
        static bool Apply(UBlueprint* Blueprint,
                          const TArray<FBlueprintComponentSpec>& Components,
                          const FString& FieldPrefix,
                          TArray<FCommandForgeError>& OutErrors,
                          TMap<FString, FString>& OutValidation);
    };
}
