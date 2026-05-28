#pragma once

#include "Commandlets/Commandlet.h"
#include "PlanAssetChangesCommandlet.generated.h"

UCLASS()
class UPlanAssetChangesCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UPlanAssetChangesCommandlet();
    virtual int32 Main(const FString& Params) override;
};
