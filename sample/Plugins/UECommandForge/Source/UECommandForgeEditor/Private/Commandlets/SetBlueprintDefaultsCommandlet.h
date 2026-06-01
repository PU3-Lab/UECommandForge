#pragma once

#include "Commandlets/Commandlet.h"
#include "SetBlueprintDefaultsCommandlet.generated.h"

UCLASS()
class USetBlueprintDefaultsCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    USetBlueprintDefaultsCommandlet();
    virtual int32 Main(const FString& Params) override;
};
