#pragma once

#include "CoreMinimal.h"
#include "CommandForgeTypes.h"
#include "Specs/CppClassBatchSpec.h"

namespace UECommandForge
{
    class UECOMMANDFORGEEDITOR_API FCppClassBatchSpecParser
    {
    public:
        static bool Parse(const FString& Json, FCppClassBatchSpec& OutSpec,
                          TArray<FCommandForgeError>& OutErrors);
    };
}
