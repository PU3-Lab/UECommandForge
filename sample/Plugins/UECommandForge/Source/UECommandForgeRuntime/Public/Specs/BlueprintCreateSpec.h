#pragma once

#include "CoreMinimal.h"
#include "BlueprintCreateSpec.generated.h"

USTRUCT()
struct UECOMMANDFORGERUNTIME_API FCommandForgeBlueprintCreateSpec
{
    GENERATED_BODY()

    UPROPERTY()
    FString Version;

    UPROPERTY()
    FString Kind;

    UPROPERTY()
    FString TransactionId;

    UPROPERTY()
    FString AssetPath;

    UPROPERTY()
    FString ParentClass = TEXT("/Script/Engine.Actor");

    UPROPERTY()
    bool bCompile = true;

    UPROPERTY()
    bool bSave = true;

    UPROPERTY()
    bool bAllowReplace = false;
};
