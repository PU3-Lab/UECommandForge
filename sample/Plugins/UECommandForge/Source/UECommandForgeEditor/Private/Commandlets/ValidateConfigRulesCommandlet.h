#pragma once

#include "Commandlets/Commandlet.h"
#include "ValidateConfigRulesCommandlet.generated.h"

UCLASS()
class UECOMMANDFORGEEDITOR_API UValidateConfigRulesCommandlet final : public UCommandlet
{
    GENERATED_BODY()

public:
    UValidateConfigRulesCommandlet();
    virtual int32 Main(const FString& Params) override;
};
