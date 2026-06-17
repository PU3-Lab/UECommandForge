#pragma once

#include "Commandlets/Commandlet.h"
#include "DiffPlatformConfigCommandlet.generated.h"

UCLASS()
class UDiffPlatformConfigCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    UDiffPlatformConfigCommandlet();
    virtual int32 Main(const FString& Params) override;
};
