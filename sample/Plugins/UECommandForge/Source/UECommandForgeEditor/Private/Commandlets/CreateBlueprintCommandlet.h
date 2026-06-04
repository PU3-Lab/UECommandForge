#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "CreateBlueprintCommandlet.generated.h"

UCLASS()
class UCreateBlueprintCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UCreateBlueprintCommandlet();
    virtual int32 Main(const FString& Params) override;
};
