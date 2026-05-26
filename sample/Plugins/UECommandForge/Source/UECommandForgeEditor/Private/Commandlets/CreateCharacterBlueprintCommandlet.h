#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "CreateCharacterBlueprintCommandlet.generated.h"

UCLASS()
class UCreateCharacterBlueprintCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UCreateCharacterBlueprintCommandlet();
    virtual int32 Main(const FString& Params) override;
};
