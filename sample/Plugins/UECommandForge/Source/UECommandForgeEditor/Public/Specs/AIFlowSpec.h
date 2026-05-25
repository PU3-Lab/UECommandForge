#pragma once
#include "CoreMinimal.h"
#include "Specs/BlueprintSpec.h"
#include "Specs/StateTreeSpec.h"
#include "Specs/PlacementSpec.h"
#include "AIFlowSpec.generated.h"

USTRUCT()
struct UECOMMANDFORGEEDITOR_API FAIFlowSpec
{
	GENERATED_BODY()
	UPROPERTY() FString Version = TEXT("1");
	UPROPERTY() FBlueprintSpec Character;
	UPROPERTY() FBlueprintSpec AIController;
	UPROPERTY() FStateTreeSpec StateTree;
	UPROPERTY() FPlacementSpec Placement;
};
