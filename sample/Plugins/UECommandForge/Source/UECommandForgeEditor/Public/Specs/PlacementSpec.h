#pragma once
#include "CoreMinimal.h"
#include "PlacementSpec.generated.h"

USTRUCT()
struct UECOMMANDFORGEEDITOR_API FPlacementSpec
{
	GENERATED_BODY()
	UPROPERTY() FString MapPath;
	UPROPERTY() FString BlueprintPath;
	UPROPERTY() FVector Location = FVector::ZeroVector;
	UPROPERTY() FRotator Rotation = FRotator::ZeroRotator;
};
