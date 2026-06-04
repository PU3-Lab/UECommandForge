#pragma once
#include "CoreMinimal.h"
#include "BlueprintSpec.generated.h"

USTRUCT()
struct UECOMMANDFORGEEDITOR_API FBlueprintSpec
{
	GENERATED_BODY()

	UPROPERTY()
	FString Version;

	UPROPERTY()
	FString Kind;

	UPROPERTY()
	FString TransactionId;

	UPROPERTY()
	FString AssetPath;

	UPROPERTY()
	FString ParentClass = TEXT("/Script/Engine.Actor");

	UPROPERTY()
	FString DisplayName;

	UPROPERTY()
	bool bCompile = true;

	UPROPERTY()
	bool bSave = true;

	UPROPERTY()
	bool bAllowReplace = false;
};

