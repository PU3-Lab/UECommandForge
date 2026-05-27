#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "PlaceActorCommandlet.generated.h"

UCLASS()
class UPlaceActorCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UPlaceActorCommandlet();
    virtual int32 Main(const FString& Params) override;
};
