#pragma once

#include "Commandlets/Commandlet.h"
#include "ValidatePluginDependenciesCommandlet.generated.h"

UCLASS()
class UValidatePluginDependenciesCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    UValidatePluginDependenciesCommandlet();
    virtual int32 Main(const FString& Params) override;
};
