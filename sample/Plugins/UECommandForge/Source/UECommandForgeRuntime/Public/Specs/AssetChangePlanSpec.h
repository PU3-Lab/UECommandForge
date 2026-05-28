#pragma once

#include "CoreMinimal.h"
#include "AssetChangePlanSpec.generated.h"

USTRUCT()
struct UECOMMANDFORGERUNTIME_API FCommandForgeAssetChangeOperation
{
    GENERATED_BODY()

    UPROPERTY() FString Operation;
    UPROPERTY() FString Path;
    UPROPERTY() FString Source;
    UPROPERTY() FString Target;
    UPROPERTY() FString Asset;
};

USTRUCT()
struct UECOMMANDFORGERUNTIME_API FCommandForgeAssetChangePlanSpec
{
    GENERATED_BODY()

    UPROPERTY() FString Version;
    UPROPERTY() FString Kind;
    UPROPERTY() FString TransactionId;
    UPROPERTY() FString RollbackPlanPath;
    UPROPERTY() bool bAllowDelete = false;
    UPROPERTY() TArray<FString> DeleteAssets;
    UPROPERTY() TArray<FCommandForgeAssetChangeOperation> Operations;
};
