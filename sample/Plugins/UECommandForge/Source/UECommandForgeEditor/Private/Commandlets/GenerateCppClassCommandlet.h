#pragma once

#include "Commandlets/Commandlet.h"
#include "GenerateCppClassCommandlet.generated.h"

UCLASS()
class UGenerateCppClassCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    UGenerateCppClassCommandlet();
    virtual int32 Main(const FString& Params) override;
};
