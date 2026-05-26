#include "Builders/StateTreeBuilder.h"
#include "Builders/StateTreeEditorAdapter.h"

namespace UECommandForge
{
    bool FStateTreeBuilder::Build(const FStateTreeSpec& Spec, TArray<FCommandForgeError>& OutErrors)
    {
        if (Spec.AssetPath.IsEmpty())
        {
            OutErrors.Add({ TEXT("MISSING_ASSET_PATH"), TEXT("AssetPath가 비어 있습니다"), TEXT("AssetPath") });
            return false;
        }

        UStateTree* ST = FStateTreeEditorAdapter::CreateOrLoad(Spec.AssetPath, OutErrors);
        if (!ST)
            return false;

        for (const FStateTreeTaskSpec& TaskSpec : Spec.Tasks)
        {
            if (!FStateTreeEditorAdapter::AddState(ST, TaskSpec.StateName, OutErrors))
                return false;
            if (!FStateTreeEditorAdapter::AddTask(ST, TaskSpec.StateName, TaskSpec.TaskType, OutErrors))
                return false;
        }

        return FStateTreeEditorAdapter::Save(ST, OutErrors);
    }
}
