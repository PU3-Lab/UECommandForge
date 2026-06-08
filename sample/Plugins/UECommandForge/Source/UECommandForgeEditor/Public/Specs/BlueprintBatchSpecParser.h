#pragma once

#include "CoreMinimal.h"
#include "CommandForgeTypes.h"
#include "Specs/BlueprintBatchSpec.h"

namespace UECommandForge
{
    class UECOMMANDFORGEEDITOR_API FBlueprintBatchSpecParser
    {
    public:
        static bool Parse(const FString& Json, FBlueprintBatchSpec& OutSpec,
                          TArray<FCommandForgeError>& OutErrors);
    };
}
