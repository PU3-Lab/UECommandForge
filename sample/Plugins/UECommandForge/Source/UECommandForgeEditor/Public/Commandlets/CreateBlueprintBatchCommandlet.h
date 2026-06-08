#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "CreateBlueprintBatchCommandlet.generated.h"

UCLASS()
class UECOMMANDFORGEEDITOR_API UCreateBlueprintBatchCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    UCreateBlueprintBatchCommandlet();
    virtual int32 Main(const FString& Params) override;
};
