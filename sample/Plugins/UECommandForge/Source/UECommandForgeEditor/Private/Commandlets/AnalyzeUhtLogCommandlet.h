#pragma once

#include "Commandlets/Commandlet.h"
#include "AnalyzeUhtLogCommandlet.generated.h"

UCLASS()
class UAnalyzeUhtLogCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    UAnalyzeUhtLogCommandlet();
    virtual int32 Main(const FString& Params) override;
};
