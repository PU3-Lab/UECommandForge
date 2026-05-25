#pragma once

#include "CoreMinimal.h"
#include "CommandForgeTypes.h"

struct FCommandForgeReport
{
    bool bOk = false;
    FString Commandlet;
    TArray<FString> CreatedAssets;
    TArray<FString> ModifiedAssets;
    TMap<FString, FString> Validation;
    TArray<FCommandForgeError> Errors;
    TArray<FString> NextSuggestions;
};

namespace UECommandForge
{
    class FJsonReportWriter
    {
    public:
        static bool Write(const FString& OutPath, const FCommandForgeReport& Report);
    };
}
