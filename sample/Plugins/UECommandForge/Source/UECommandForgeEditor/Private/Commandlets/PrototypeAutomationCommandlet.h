#pragma once

#include "Commandlets/Commandlet.h"
#include "PrototypeAutomationCommandlet.generated.h"

UCLASS()
class UECOMMANDFORGEEDITOR_API UPrototypeAutomationCommandlet final : public UCommandlet
{
    GENERATED_BODY()

public:
    UPrototypeAutomationCommandlet();
    virtual int32 Main(const FString& Params) override;
};
