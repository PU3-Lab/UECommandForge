#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "AssetSnapshotCommandlet.generated.h"

UCLASS()
class UAssetSnapshotCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UAssetSnapshotCommandlet();
    virtual int32 Main(const FString& Params) override;
};
