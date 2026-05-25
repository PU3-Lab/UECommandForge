#pragma once
#include "CoreMinimal.h"
#include "Specs/AIFlowSpec.h"
#include "CommandForgeTypes.h"

namespace UECommandForge
{
    class FAIFlowSpecParser
    {
    public:
        static bool Parse(const FString& Json, FAIFlowSpec& OutSpec,
                          TArray<FCommandForgeError>& OutErrors);
    };
}
