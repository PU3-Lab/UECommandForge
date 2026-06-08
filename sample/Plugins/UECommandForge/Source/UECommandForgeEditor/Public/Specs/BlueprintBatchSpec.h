#pragma once

#include "CoreMinimal.h"
#include "Builders/BlueprintComponentApplier.h"
#include "Builders/BlueprintCDOPropertyApplier.h"
#include "BlueprintBatchSpec.generated.h"

USTRUCT()
struct UECOMMANDFORGEEDITOR_API FBlueprintBatchItemSpec
{
    GENERATED_BODY()

    UPROPERTY()
    FString AssetPath;

    UPROPERTY()
    FString ParentClass = TEXT("/Script/Engine.Actor");

    UPROPERTY()
    bool bAllowReplace = false;

    TArray<UECommandForge::FBlueprintComponentSpec> Components;
    TArray<UECommandForge::FBlueprintCDOPropertyAssignment> Defaults;
};

USTRUCT()
struct UECOMMANDFORGEEDITOR_API FBlueprintBatchSpec
{
    GENERATED_BODY()

    UPROPERTY()
    FString Version;

    UPROPERTY()
    FString Kind;

    UPROPERTY()
    FString TransactionId;

    UPROPERTY()
    bool bCompile = true;

    UPROPERTY()
    bool bSave = true;

    UPROPERTY()
    TArray<FBlueprintBatchItemSpec> Items;
};
