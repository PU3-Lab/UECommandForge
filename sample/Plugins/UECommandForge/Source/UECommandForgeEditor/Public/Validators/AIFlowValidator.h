#pragma once
#include "CoreMinimal.h"
#include "Specs/AIFlowSpec.h"
#include "CommandForgeTypes.h"

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
