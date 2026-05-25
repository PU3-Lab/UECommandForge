#pragma once
#include "CoreMinimal.h"
#include "BlueprintSpec.generated.h"

USTRUCT()
struct UECOMMANDFORGEEDITOR_API FBlueprintSpec
{
	GENERATED_BODY()
	UPROPERTY() FString AssetPath;
	UPROPERTY() FString ParentClass;
	UPROPERTY() FString DisplayName;
};
