#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "CommandForgeStateTreeTasks.generated.h"

USTRUCT()
struct UECOMMANDFORGEEDITOR_API FCommandForgeStateTreeNoopTaskInstanceData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = Parameter)
    float DurationSeconds = 0.0f;
};

USTRUCT(meta = (DisplayName = "CommandForge Wait", Category = "UECommandForge"))
struct UECOMMANDFORGEEDITOR_API FCommandForgeStateTreeWaitTask : public FStateTreeTaskCommonBase
{
    GENERATED_BODY()

    using FInstanceDataType = FCommandForgeStateTreeNoopTaskInstanceData;

    FCommandForgeStateTreeWaitTask()
    {
        bShouldCallTick = false;
    }

    virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
                                           const FStateTreeTransitionResult& Transition) const override
    {
        return EStateTreeRunStatus::Succeeded;
    }
};

USTRUCT(meta = (DisplayName = "CommandForge Play Montage", Category = "UECommandForge"))
struct UECOMMANDFORGEEDITOR_API FCommandForgeStateTreePlayMontageTask : public FStateTreeTaskCommonBase
{
    GENERATED_BODY()

    using FInstanceDataType = FCommandForgeStateTreeNoopTaskInstanceData;

    FCommandForgeStateTreePlayMontageTask()
    {
        bShouldCallTick = false;
    }

    virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
                                           const FStateTreeTransitionResult& Transition) const override
    {
        return EStateTreeRunStatus::Succeeded;
    }
};
