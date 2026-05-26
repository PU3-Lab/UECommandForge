#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "CreateAIControllerBlueprintCommandlet.generated.h"

UCLASS()
class UCreateAIControllerBlueprintCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UCreateAIControllerBlueprintCommandlet();
    virtual int32 Main(const FString& Params) override;
};
