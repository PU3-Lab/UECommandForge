#pragma once

#include "CoreMinimal.h"
#include "CppClassSpec.generated.h"

USTRUCT()
struct UECOMMANDFORGERUNTIME_API FCommandForgeCppMetadata
{
    GENERATED_BODY()

    UPROPERTY() TMap<FString, FString> Values;
};

USTRUCT()
struct UECOMMANDFORGERUNTIME_API FCommandForgeCppParamSpec
{
    GENERATED_BODY()

    UPROPERTY() FString Name;
    UPROPERTY() FString Type;
};

USTRUCT()
struct UECOMMANDFORGERUNTIME_API FCommandForgeCppPropertySpec
{
    GENERATED_BODY()

    UPROPERTY() FString Name;
    UPROPERTY() FString Type;
    UPROPERTY() FString DefaultValue;
    UPROPERTY() FCommandForgeCppMetadata Metadata;
};

USTRUCT()
struct UECOMMANDFORGERUNTIME_API FCommandForgeCppFunctionSpec
{
    GENERATED_BODY()

    UPROPERTY() FString Name;
    UPROPERTY() FString ReturnType;
    UPROPERTY() TArray<FCommandForgeCppParamSpec> Params;
    UPROPERTY() FCommandForgeCppMetadata Metadata;
};

USTRUCT()
struct UECOMMANDFORGERUNTIME_API FCommandForgeCppClassInfoSpec
{
    GENERATED_BODY()

    UPROPERTY() FString Name;
    UPROPERTY() FString Type;
    UPROPERTY() FString Module;
    UPROPERTY() FString OutputPath;
    UPROPERTY() FString BaseClass;
    UPROPERTY() FString GeneratedHeaderInclude;
    UPROPERTY() bool bBlueprintType = false;
    UPROPERTY() bool bTick = false;
};

USTRUCT()
struct UECOMMANDFORGERUNTIME_API FCommandForgeCppBuildDependencySpec
{
    GENERATED_BODY()

    UPROPERTY() TArray<FString> PublicDependencies;
    UPROPERTY() TArray<FString> PrivateDependencies;
};

USTRUCT()
struct UECOMMANDFORGERUNTIME_API FCommandForgeCppClassSpec
{
    GENERATED_BODY()

    UPROPERTY() FString Version;
    UPROPERTY() FString Kind;
    UPROPERTY() FString TransactionId;
    UPROPERTY() FCommandForgeCppClassInfoSpec ClassInfo;
    UPROPERTY() TArray<FString> Includes;
    UPROPERTY() TArray<FCommandForgeCppPropertySpec> Properties;
    UPROPERTY() TArray<FCommandForgeCppFunctionSpec> Functions;
    UPROPERTY() FCommandForgeCppBuildDependencySpec BuildDependencies;
};
