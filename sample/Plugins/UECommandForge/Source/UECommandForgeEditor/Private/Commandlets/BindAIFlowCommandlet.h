#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "BindAIFlowCommandlet.generated.h"

UCLASS()
class UBindAIFlowCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UBindAIFlowCommandlet();
    virtual int32 Main(const FString& Params) override;
};
