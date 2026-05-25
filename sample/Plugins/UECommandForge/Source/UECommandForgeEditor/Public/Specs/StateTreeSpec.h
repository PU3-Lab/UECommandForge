#pragma once
#include "CoreMinimal.h"
#include "StateTreeSpec.generated.h"

USTRUCT()
struct UECOMMANDFORGEEDITOR_API FStateTreeTaskSpec
{
	GENERATED_BODY()
	UPROPERTY() FString TaskType;
	UPROPERTY() FString StateName;
};

USTRUCT()
struct UECOMMANDFORGEEDITOR_API FStateTreeSpec
{
	GENERATED_BODY()
	UPROPERTY() FString AssetPath;
	UPROPERTY() TArray<FStateTreeTaskSpec> Tasks;
};
