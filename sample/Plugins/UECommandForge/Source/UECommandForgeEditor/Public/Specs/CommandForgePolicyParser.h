#pragma once

#include "CoreMinimal.h"
#include "CommandForgeTypes.h"
#include "Dom/JsonObject.h"

struct FCommandForgePolicyDocument
{
    FString Version;
    FString Kind;
    FString CanonicalJson;
    TSharedPtr<FJsonObject> RootObject;
};

namespace UECommandForge
{
    class FCommandForgePolicyParser
    {
    public:
        static bool ParseJson(const FString& Json, FCommandForgePolicyDocument& OutPolicy,
                              TArray<FCommandForgeError>& OutErrors);
    };
}
