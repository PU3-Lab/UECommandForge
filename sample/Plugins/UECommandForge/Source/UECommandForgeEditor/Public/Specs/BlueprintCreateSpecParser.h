#pragma once
#include "CoreMinimal.h"
#include "Specs/BlueprintCreateSpec.h"
#include "CommandForgeTypes.h"

namespace UECommandForge
{
    class FBlueprintCreateSpecParser
    {
    public:
        static bool Parse(const FString& Json, FCommandForgeBlueprintCreateSpec& OutSpec,
                          TArray<FCommandForgeError>& OutErrors);
    };
}
