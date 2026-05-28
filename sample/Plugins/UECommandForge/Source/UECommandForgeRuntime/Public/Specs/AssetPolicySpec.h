#pragma once

#include "CoreMinimal.h"
#include "AssetPolicySpec.generated.h"

USTRUCT()
struct UECOMMANDFORGERUNTIME_API FCommandForgeAssetPolicyRule
{
    GENERATED_BODY()

    UPROPERTY() FString AssetClass;
    UPROPERTY() FString Prefix;
    UPROPERTY() FString Suffix;
    UPROPERTY() TArray<FString> AllowedPaths;
};

USTRUCT()
struct UECOMMANDFORGERUNTIME_API FCommandForgeAssetPolicyException
{
    GENERATED_BODY()

    UPROPERTY() FString PackageName;
    UPROPERTY() FString ObjectPath;
    UPROPERTY() FString Reason;
};

USTRUCT()
struct UECOMMANDFORGERUNTIME_API FCommandForgeUnusedAssetPolicy
{
    GENERATED_BODY()

    UPROPERTY() bool bEnabled = false;
    UPROPERTY() FString Severity = TEXT("warning");
};

USTRUCT()
struct UECOMMANDFORGERUNTIME_API FCommandForgeRedirectorPolicy
{
    GENERATED_BODY()

    UPROPERTY() bool bAllowed = false;
    UPROPERTY() FString Severity = TEXT("error");
};

USTRUCT()
struct UECOMMANDFORGERUNTIME_API FCommandForgeAssetPolicySpec
{
    GENERATED_BODY()

    UPROPERTY() FString Version;
    UPROPERTY() FString Kind;
    UPROPERTY() TArray<FCommandForgeAssetPolicyRule> Rules;
    UPROPERTY() TArray<FCommandForgeAssetPolicyException> Exceptions;
    UPROPERTY() FCommandForgeUnusedAssetPolicy UnusedAssetCandidates;
    UPROPERTY() FCommandForgeRedirectorPolicy Redirectors;
};
