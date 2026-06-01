#pragma once

#include "CoreMinimal.h"
#include "BlueprintDefaultsSpec.generated.h"

USTRUCT()
struct UECOMMANDFORGEEDITOR_API FBlueprintDefaultPropertySpec
{
    GENERATED_BODY()

    UPROPERTY() FString Property;
    UPROPERTY() FString Value;
};

USTRUCT()
struct UECOMMANDFORGEEDITOR_API FBlueprintDefaultComponentSpec
{
    GENERATED_BODY()

    UPROPERTY() FString Name;
    UPROPERTY() FString Class;
};

USTRUCT()
struct UECOMMANDFORGEEDITOR_API FBlueprintDefaultsSpec
{
    GENERATED_BODY()

    UPROPERTY() FString Version;
    UPROPERTY() FString Kind;
    UPROPERTY() FString TransactionId;
    UPROPERTY() FString AssetPath;
    UPROPERTY() bool bCompile = true;
    UPROPERTY() bool bSave = true;
    UPROPERTY() TArray<FBlueprintDefaultPropertySpec> Defaults;
    UPROPERTY() TArray<FBlueprintDefaultComponentSpec> Components;
};
