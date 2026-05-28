#pragma once

#include "CoreMinimal.h"
#include "CommandForgeTypes.generated.h"

UENUM()
enum class ECommandForgeExitCode : uint8
{
    Ok = 0,
    SpecParseFailed = 2,
    BuildFailed = 3,
    ValidationFailed = 4,
    EngineError = 5
};

USTRUCT()
struct UECOMMANDFORGERUNTIME_API FCommandForgeError
{
    GENERATED_BODY()

    UPROPERTY() FString Code;
    UPROPERTY() FString Message;
    UPROPERTY() FString Field;
};

USTRUCT()
struct UECOMMANDFORGERUNTIME_API FCommandForgeValidationIssue
{
    GENERATED_BODY()

    UPROPERTY() FString Severity;
    UPROPERTY() FString Code;
    UPROPERTY() FString Message;
    UPROPERTY() FString Field;
    UPROPERTY() FString AssetPath;
    UPROPERTY() FString FilePath;
    UPROPERTY() FString SuggestedFix;
};

USTRUCT()
struct UECOMMANDFORGERUNTIME_API FCommandForgeRollbackOperation
{
    GENERATED_BODY()

    UPROPERTY() FString PlannedOperation;
    UPROPERTY() FString BeforeAssetPath;
    UPROPERTY() FString BeforeFilePath;
    UPROPERTY() FString AfterAssetPath;
    UPROPERTY() FString AfterFilePath;
    UPROPERTY() TArray<FString> DependencySnapshot;
    UPROPERTY() TMap<FString, FString> ValidationSummary;
};

USTRUCT()
struct UECOMMANDFORGERUNTIME_API FCommandForgeRollbackPlan
{
    GENERATED_BODY()

    UPROPERTY() FString TransactionId;
    UPROPERTY() FString Commandlet;
    UPROPERTY() FString Timestamp;
    UPROPERTY() TArray<FCommandForgeRollbackOperation> Operations;
};

USTRUCT()
struct UECOMMANDFORGERUNTIME_API FCommandForgeStepResult
{
    GENERATED_BODY()

    UPROPERTY() FString Step;
    UPROPERTY() bool bOk = false;
    UPROPERTY() TArray<FString> CreatedAssets;
    UPROPERTY() TMap<FString, FString> Validation;
    UPROPERTY() TArray<FCommandForgeError> Errors;
};
