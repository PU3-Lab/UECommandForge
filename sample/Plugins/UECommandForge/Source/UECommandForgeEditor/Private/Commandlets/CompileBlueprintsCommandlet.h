#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "CompileBlueprintsCommandlet.generated.h"

UCLASS()
class UCompileBlueprintsCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UCompileBlueprintsCommandlet();
    virtual int32 Main(const FString& Params) override;
};
