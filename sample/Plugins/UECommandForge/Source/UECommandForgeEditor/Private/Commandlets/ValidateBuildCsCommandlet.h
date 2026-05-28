#pragma once

#include "Commandlets/Commandlet.h"
#include "ValidateBuildCsCommandlet.generated.h"

UCLASS()
class UValidateBuildCsCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    UValidateBuildCsCommandlet();
    virtual int32 Main(const FString& Params) override;
};
