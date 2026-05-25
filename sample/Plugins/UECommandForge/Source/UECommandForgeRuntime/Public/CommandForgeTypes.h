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
