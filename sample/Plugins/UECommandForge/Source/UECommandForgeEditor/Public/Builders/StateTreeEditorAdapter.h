#pragma once

#include "CoreMinimal.h"
#include "CommandForgeTypes.h"

class UStateTree;
class UStateTreeState;

namespace UECommandForge
{
    class FStateTreeEditorAdapter
    {
    public:
        static UStateTree* CreateOrLoad(const FString& AssetPath,
                                        TArray<FCommandForgeError>& OutErrors);

        static UStateTreeState* AddState(UStateTree* StateTree,
                                         const FString& StateName,
                                         TArray<FCommandForgeError>& OutErrors);

        static bool AddTask(UStateTreeState* State,
                            const FString& TaskType,
                            TArray<FCommandForgeError>& OutErrors);

        static bool Save(UStateTree* StateTree,
                         TArray<FCommandForgeError>& OutErrors,
                         TMap<FString, FString>& OutValidation);
    };
}
