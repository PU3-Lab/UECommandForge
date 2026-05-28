#pragma once

#include "Commandlets/Commandlet.h"
#include "CreateProjectFoldersCommandlet.generated.h"

UCLASS()
class UCreateProjectFoldersCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    UCreateProjectFoldersCommandlet();
    virtual int32 Main(const FString& Params) override;
};
