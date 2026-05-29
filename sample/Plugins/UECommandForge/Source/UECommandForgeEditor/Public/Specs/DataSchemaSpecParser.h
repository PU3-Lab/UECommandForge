#pragma once

#include "CoreMinimal.h"
#include "CommandForgeTypes.h"
#include "Specs/DataSchemaSpec.h"

namespace UECommandForge
{
    class FDataSchemaSpecParser
    {
    public:
        static bool ParseJson(const FString& Json, FCommandForgeDataSchemaSpec& OutSpec,
                              TArray<FCommandForgeError>& OutErrors);
    };
}
