#pragma once

#include "CoreMinimal.h"
#include "DataSchemaSpec.generated.h"

USTRUCT()
struct UECOMMANDFORGERUNTIME_API FCommandForgeDataSchemaField
{
    GENERATED_BODY()

    UPROPERTY() FString Name;
    UPROPERTY() FString Type;
    UPROPERTY() bool bRequired = false;
    UPROPERTY() bool bUnique = false;
    UPROPERTY() bool bHasMin = false;
    UPROPERTY() double Min = 0.0;
    UPROPERTY() bool bHasMax = false;
    UPROPERTY() double Max = 0.0;
    UPROPERTY() FString Regex;
    UPROPERTY() TArray<FString> AllowedValues;
    UPROPERTY() FString EnumSource;
    UPROPERTY() FString GameplayTagSource;
    UPROPERTY() FString Severity = TEXT("error");
    UPROPERTY() FString Exception;
    UPROPERTY() FString Section;
    UPROPERTY() FString DefaultValue;
};

USTRUCT()
struct UECOMMANDFORGERUNTIME_API FCommandForgeDataSchemaException
{
    GENERATED_BODY()

    UPROPERTY() FString Field;
    UPROPERTY() FString Value;
    UPROPERTY() FString Reason;
    UPROPERTY() FString Severity = TEXT("warning");
};

USTRUCT()
struct UECOMMANDFORGERUNTIME_API FCommandForgeDataSchemaSpec
{
    GENERATED_BODY()

    UPROPERTY() FString Version;
    UPROPERTY() FString Kind;
    UPROPERTY() FString KeyField;
    UPROPERTY() FString SourceType;
    UPROPERTY() FString TargetAssetPath;
    UPROPERTY() FString RowStructPath;
    UPROPERTY() FString DataAssetClassPath;
    UPROPERTY() FString ConfigFile;
    UPROPERTY() FString ConfigSection;
    UPROPERTY() TArray<FCommandForgeDataSchemaField> Fields;
    UPROPERTY() TArray<FCommandForgeDataSchemaException> Exceptions;
};
