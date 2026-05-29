#pragma once

#include "Commandlets/Commandlet.h"
#include "ValidateDataTableCommandlet.generated.h"

UCLASS()
class UValidateDataTableCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    UValidateDataTableCommandlet();
    virtual int32 Main(const FString& Params) override;
};
