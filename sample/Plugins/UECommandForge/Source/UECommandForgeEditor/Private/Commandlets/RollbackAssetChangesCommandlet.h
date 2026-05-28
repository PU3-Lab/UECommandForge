#pragma once

#include "Commandlets/Commandlet.h"
#include "RollbackAssetChangesCommandlet.generated.h"

UCLASS()
class URollbackAssetChangesCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    URollbackAssetChangesCommandlet();
    virtual int32 Main(const FString& Params) override;
};
