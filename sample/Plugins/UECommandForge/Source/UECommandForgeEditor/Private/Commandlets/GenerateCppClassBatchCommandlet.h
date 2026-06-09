#pragma once

#include "Commandlets/Commandlet.h"
#include "GenerateCppClassBatchCommandlet.generated.h"

UCLASS()
class UGenerateCppClassBatchCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    UGenerateCppClassBatchCommandlet();
    virtual int32 Main(const FString& Params) override;
};
