#pragma once

#include "Commandlets/Commandlet.h"
#include "ValidateCppReflectionCommandlet.generated.h"

UCLASS()
class UValidateCppReflectionCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    UValidateCppReflectionCommandlet();
    virtual int32 Main(const FString& Params) override;
};
