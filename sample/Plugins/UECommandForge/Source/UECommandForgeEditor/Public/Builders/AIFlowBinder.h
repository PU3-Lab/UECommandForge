#pragma once

#include "CoreMinimal.h"
#include "CommandForgeTypes.h"
#include "Specs/AIFlowSpec.h"

namespace UECommandForge
{
    class FAIFlowBinder
    {
    public:
        static bool Bind(const FAIFlowSpec& Spec,
                         TArray<FCommandForgeError>& OutErrors,
                         TMap<FString, FString>& OutValidation);
    };
}
