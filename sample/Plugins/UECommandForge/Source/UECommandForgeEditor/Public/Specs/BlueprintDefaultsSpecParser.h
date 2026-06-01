#pragma once

#include "CoreMinimal.h"
#include "CommandForgeTypes.h"
#include "Specs/BlueprintDefaultsSpec.h"

namespace UECommandForge
{
    class FBlueprintDefaultsSpecParser
    {
    public:
        static bool ParseJson(const FString& Json,
                              FBlueprintDefaultsSpec& OutSpec,
                              TArray<FCommandForgeError>& OutErrors);
    };
}
