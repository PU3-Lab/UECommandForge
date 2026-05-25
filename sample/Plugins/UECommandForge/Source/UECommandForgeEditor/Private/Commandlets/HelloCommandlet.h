#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "HelloCommandlet.generated.h"

UCLASS()
class UHelloCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UHelloCommandlet();
    virtual int32 Main(const FString& Params) override;
};
