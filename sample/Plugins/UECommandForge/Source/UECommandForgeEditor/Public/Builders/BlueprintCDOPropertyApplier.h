#pragma once

#include "CoreMinimal.h"
#include "CommandForgeTypes.h"

class UBlueprint;

namespace UECommandForge
{
    struct FBlueprintCDOPropertyAssignment
    {
        FString PropertyName;
        FString Value;
    };

    class FBlueprintCDOPropertyApplier
    {
    public:
        static bool Apply(UBlueprint* Blueprint,
                          const TArray<FBlueprintCDOPropertyAssignment>& Assignments,
                          const FString& FieldPrefix,
                          bool bCompileBeforeApply,
                          TArray<FCommandForgeError>& OutErrors,
                          TMap<FString, FString>& OutValidation);
    };
}
