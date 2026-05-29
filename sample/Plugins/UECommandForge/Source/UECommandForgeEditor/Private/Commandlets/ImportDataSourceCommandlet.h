#pragma once

#include "Commandlets/Commandlet.h"
#include "ImportDataSourceCommandlet.generated.h"

UCLASS()
class UImportDataSourceCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    UImportDataSourceCommandlet();
    virtual int32 Main(const FString& Params) override;
};
