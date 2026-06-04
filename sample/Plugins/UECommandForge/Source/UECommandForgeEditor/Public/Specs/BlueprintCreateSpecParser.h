#pragma once
#include "CoreMinimal.h"
#include "Specs/BlueprintSpec.h"
#include "CommandForgeTypes.h"

namespace UECommandForge
{
    class FBlueprintCreateSpecParser
    {
    public:
        static bool Parse(const FString& Json, FBlueprintSpec& OutSpec,
                          TArray<FCommandForgeError>& OutErrors);
    };
}

