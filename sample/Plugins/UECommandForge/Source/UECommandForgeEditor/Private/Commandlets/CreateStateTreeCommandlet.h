#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "CreateStateTreeCommandlet.generated.h"

UCLASS()
class UCreateStateTreeCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UCreateStateTreeCommandlet();
    virtual int32 Main(const FString& Params) override;
};
