#include "Builders/StateTreeBuilder.h"
#include "Builders/StateTreeEditorAdapter.h"
#include "StateTreeState.h"

namespace UECommandForge
{
    bool FStateTreeBuilder::Build(const FStateTreeSpec& Spec,
                                  TArray<FCommandForgeError>& OutErrors,
                                  TMap<FString, FString>& OutValidation)
    {
        UStateTree* StateTree = FStateTreeEditorAdapter::CreateOrLoad(Spec.AssetPath, OutErrors);
        if (!StateTree)
        {
            return false;
        }

        TSet<FString> StatesCreated;
        int32 TasksCreated = 0;
        for (const FStateTreeTaskSpec& TaskSpec : Spec.Tasks)
        {
            UStateTreeState* State = FStateTreeEditorAdapter::AddState(StateTree, TaskSpec.StateName, OutErrors);
            if (!State)
            {
                return false;
            }
            StatesCreated.Add(TaskSpec.StateName);

            if (!FStateTreeEditorAdapter::AddTask(State, TaskSpec.TaskType, OutErrors))
            {
                return false;
            }
            ++TasksCreated;
        }

        OutValidation.Add(TEXT("states_created"), FString::FromInt(StatesCreated.Num()));
        OutValidation.Add(TEXT("tasks_created"), FString::FromInt(TasksCreated));

        return FStateTreeEditorAdapter::Save(StateTree, OutErrors, OutValidation);
    }
}
