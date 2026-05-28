#pragma once

#include "CoreMinimal.h"
#include "CommandForgeTypes.h"

namespace UECommandForge
{
    class FRollbackPlanWriter
    {
    public:
        static bool Write(const FString& OutPath, const FCommandForgeRollbackPlan& Plan);
    };
}
