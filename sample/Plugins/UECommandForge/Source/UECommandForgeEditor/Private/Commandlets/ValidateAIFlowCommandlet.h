#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "ValidateAIFlowCommandlet.generated.h"

UCLASS()
class UValidateAIFlowCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UValidateAIFlowCommandlet();
    virtual int32 Main(const FString& Params) override;
};
