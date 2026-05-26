#pragma once
#include "CoreMinimal.h"
#include "Specs/BlueprintSpec.h"
#include "CommandForgeTypes.h"

namespace UECommandForge
{
    class FCharacterBlueprintBuilder
    {
    public:
        static bool Build(const FBlueprintSpec& Spec,
                          TArray<FCommandForgeError>& OutErrors,
                          TMap<FString, FString>& OutValidation);
    };
}
