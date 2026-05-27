#pragma once

#include "CoreMinimal.h"
#include "CommandForgeTypes.h"
#include "Specs/AIFlowSpec.h"

namespace UECommandForge
{
    class FAIFlowValidator
    {
    public:
        static bool Validate(const FAIFlowSpec& Spec,
                             TMap<FString, FString>& OutValidation,
                             TArray<FCommandForgeError>& OutErrors);
    };
}
