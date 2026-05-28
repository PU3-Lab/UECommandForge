#pragma once

#include "CoreMinimal.h"
#include "CommandForgeTypes.h"

struct FCommandForgeReport
{
    bool bOk = false;
    FString Commandlet;
    FString TransactionId;
    bool bDryRun = false;
    bool bApplied = false;
    TArray<FString> CreatedAssets;
    TArray<FString> ModifiedAssets;
    TArray<FString> ChangedAssets;
    TArray<FString> ChangedFiles;
    TArray<FCommandForgeAssetSnapshotRecord> Assets;
    TMap<FString, FString> Validation;
    TMap<FString, FString> PostValidation;
    TArray<FCommandForgeValidationIssue> ValidationIssues;
    TArray<FCommandForgeError> Errors;
    TArray<FString> NextSuggestions;
    TArray<FCommandForgeStepResult> Steps;
    bool bRollbackAvailable = false;
    FString RollbackPlanPath;
};

namespace UECommandForge
{
    class FJsonReportWriter
    {
    public:
        static bool Write(const FString& OutPath, const FCommandForgeReport& Report);
    };
}
