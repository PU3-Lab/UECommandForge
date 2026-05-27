#pragma once

#include "CoreMinimal.h"
#include "Specs/StateTreeSpec.h"
#include "CommandForgeTypes.h"

namespace UECommandForge
{
    class FStateTreeBuilder
    {
    public:
        static bool Build(const FStateTreeSpec& Spec,
                          TArray<FCommandForgeError>& OutErrors,
                          TMap<FString, FString>& OutValidation);
    };
}
