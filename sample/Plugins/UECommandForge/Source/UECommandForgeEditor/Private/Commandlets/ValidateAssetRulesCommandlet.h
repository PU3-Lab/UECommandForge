#pragma once

#include "Commandlets/Commandlet.h"
#include "ValidateAssetRulesCommandlet.generated.h"

UCLASS()
class UValidateAssetRulesCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UValidateAssetRulesCommandlet();
    virtual int32 Main(const FString& Params) override;
};
