#pragma once

#include "Commandlets/Commandlet.h"
#include "ApplyAssetChangesCommandlet.generated.h"

UCLASS()
class UApplyAssetChangesCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UApplyAssetChangesCommandlet();
    virtual int32 Main(const FString& Params) override;
};
