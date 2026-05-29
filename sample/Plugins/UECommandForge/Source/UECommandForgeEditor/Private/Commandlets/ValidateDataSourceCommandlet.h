#pragma once

#include "Commandlets/Commandlet.h"
#include "ValidateDataSourceCommandlet.generated.h"

UCLASS()
class UValidateDataSourceCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    UValidateDataSourceCommandlet();
    virtual int32 Main(const FString& Params) override;
};
