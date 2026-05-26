#pragma once
#include "CoreMinimal.h"
#include "Specs/StateTreeSpec.h"
#include "CommandForgeTypes.h"

class UStateTree;

namespace UECommandForge
{
    class FStateTreeEditorAdapter
    {
    public:
        // StateTree 에셋 생성 (이미 존재하면 기존 에셋 반환)
        static UStateTree* CreateOrLoad(const FString& AssetPath,
                                        TArray<FCommandForgeError>& OutErrors);

        // State 추가 — 이름이 이미 존재하면 멱등(no-op)
        static bool AddState(UStateTree* StateTree, const FString& StateName,
                             TArray<FCommandForgeError>& OutErrors);

        // Task 추가 — TaskType 화이트리스트: Wait, MoveTo
        static bool AddTask(UStateTree* StateTree, const FString& StateName,
                            const FString& TaskType,
                            TArray<FCommandForgeError>& OutErrors);

        // 에셋 저장
        static bool Save(UStateTree* StateTree, TArray<FCommandForgeError>& OutErrors);
    };
}
