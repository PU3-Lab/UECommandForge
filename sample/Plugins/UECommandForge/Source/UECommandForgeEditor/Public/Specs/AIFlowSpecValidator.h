#pragma once
#include "CoreMinimal.h"
#include "Specs/AIFlowSpec.h"
#include "CommandForgeTypes.h"

namespace UECommandForge
{
    class UECOMMANDFORGEEDITOR_API FAIFlowSpecValidator
    {
    public:
        static bool Validate(const FAIFlowSpec& Spec, TArray<FCommandForgeError>& OutErrors);

    private:
        static void CheckRequiredPath(const FString& Value, const FString& Field,
                                      TArray<FCommandForgeError>& OutErrors);
        static void CheckGamePrefix(const FString& Value, const FString& Field,
                                    TArray<FCommandForgeError>& OutErrors);
    };
}
