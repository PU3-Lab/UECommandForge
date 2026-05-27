#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "Reports/JsonReportWriter.h"
#include "Specs/AIFlowSpec.h"
#include "CreateAIFlowCommandlet.generated.h"

namespace UECommandForge
{
    class FCreateAIFlowWorkflow
    {
    public:
        static bool Execute(const FAIFlowSpec& Spec,
                            const FString& OutPath,
                            FCommandForgeReport& OutReport);
    };
}

UCLASS()
class UCreateAIFlowCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    UCreateAIFlowCommandlet();
    virtual int32 Main(const FString& Params) override;
};
