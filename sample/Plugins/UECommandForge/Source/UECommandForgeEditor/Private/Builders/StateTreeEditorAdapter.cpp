#include "Builders/StateTreeEditorAdapter.h"
#include "StateTree.h"
#include "StateTreeEditorData.h"
#include "StateTreeState.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "HAL/PlatformFileManager.h"

namespace UECommandForge
{
    // TaskType 문자열 → UScriptStruct 경로 매핑
    static const TCHAR* GetTaskStructPath(const FString& TaskType)
    {
        if (TaskType == TEXT("Wait") || TaskType == TEXT("Delay"))
            return TEXT("/Script/StateTreeModule.StateTreeDelayTask");
        if (TaskType == TEXT("MoveTo"))
            return TEXT("/Script/GameplayStateTreeModule.StateTreeMoveToTask");
        return nullptr;
    }

    // EditorData에서 이름으로 자식 State를 찾는다 (루트 직속 자식만)
    static UStateTreeState* FindChildState(UStateTreeState* Root, const FName& Name)
    {
        for (UStateTreeState* Child : Root->Children)
        {
            if (Child && Child->Name == Name)
                return Child;
        }
        return nullptr;
    }

    UStateTree* FStateTreeEditorAdapter::CreateOrLoad(const FString& AssetPath,
                                                      TArray<FCommandForgeError>& OutErrors)
    {
        // 이미 존재하면 반환
        if (UStateTree* Existing = LoadObject<UStateTree>(nullptr, *AssetPath))
            return Existing;

        UPackage* Package = CreatePackage(*AssetPath);
        if (!Package)
        {
            OutErrors.Add({ TEXT("PACKAGE_CREATE_FAILED"),
                FString::Printf(TEXT("패키지 생성 실패: %s"), *AssetPath), TEXT("") });
            return nullptr;
        }
        Package->FullyLoad();

        const FString AssetName = FPackageName::GetLongPackageAssetName(AssetPath);
        UStateTree* ST = NewObject<UStateTree>(Package, UStateTree::StaticClass(),
                                               FName(*AssetName), RF_Public | RF_Standalone);
        if (!ST)
        {
            OutErrors.Add({ TEXT("ST_CREATE_FAILED"),
                FString::Printf(TEXT("UStateTree 생성 실패: %s"), *AssetPath), TEXT("") });
            return nullptr;
        }

        // 에디터 데이터 수동 생성 (UStateTreeFactory는 스키마 없이 nullptr 반환하므로 직접 생성)
        UStateTreeEditorData* EditorData = NewObject<UStateTreeEditorData>(
            ST, UStateTreeEditorData::StaticClass(), FName(), RF_Transactional);
        ST->EditorData = EditorData;
        EditorData->AddRootState();

        return ST;
    }

    bool FStateTreeEditorAdapter::AddState(UStateTree* StateTree, const FString& StateName,
                                           TArray<FCommandForgeError>& OutErrors)
    {
        if (!StateTree)
        {
            OutErrors.Add({ TEXT("NULL_STATETREE"), TEXT("StateTree가 null입니다"), TEXT("") });
            return false;
        }

        UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
        if (!EditorData || EditorData->SubTrees.IsEmpty())
        {
            OutErrors.Add({ TEXT("NO_EDITOR_DATA"), TEXT("EditorData 또는 루트 State가 없습니다"), TEXT("") });
            return false;
        }

        UStateTreeState* Root = EditorData->SubTrees[0];
        const FName StateNameKey(*StateName);

        // 멱등: 이미 존재하면 성공 처리
        if (FindChildState(Root, StateNameKey))
            return true;

        Root->AddChildState(StateNameKey);
        return true;
    }

    bool FStateTreeEditorAdapter::AddTask(UStateTree* StateTree, const FString& StateName,
                                          const FString& TaskType,
                                          TArray<FCommandForgeError>& OutErrors)
    {
        if (!StateTree)
        {
            OutErrors.Add({ TEXT("NULL_STATETREE"), TEXT("StateTree가 null입니다"), TEXT("") });
            return false;
        }

        const TCHAR* StructPath = GetTaskStructPath(TaskType);
        if (!StructPath)
        {
            OutErrors.Add({ TEXT("UNKNOWN_TASK_TYPE"),
                FString::Printf(TEXT("지원하지 않는 TaskType: %s"), *TaskType),
                TEXT("TaskType") });
            return false;
        }

        UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
        if (!EditorData || EditorData->SubTrees.IsEmpty())
        {
            OutErrors.Add({ TEXT("NO_EDITOR_DATA"), TEXT("EditorData 또는 루트 State가 없습니다"), TEXT("") });
            return false;
        }

        UStateTreeState* Root   = EditorData->SubTrees[0];
        UStateTreeState* Target = FindChildState(Root, FName(*StateName));
        if (!Target)
        {
            OutErrors.Add({ TEXT("STATE_NOT_FOUND"),
                FString::Printf(TEXT("State를 찾을 수 없습니다: %s"), *StateName),
                TEXT("StateName") });
            return false;
        }

        UScriptStruct* TaskStruct = FindObject<UScriptStruct>(nullptr, StructPath);
        if (!TaskStruct)
        {
            TaskStruct = LoadObject<UScriptStruct>(nullptr, StructPath);
        }
        if (!TaskStruct)
        {
            OutErrors.Add({ TEXT("TASK_STRUCT_NOT_FOUND"),
                FString::Printf(TEXT("Task 구조체를 찾을 수 없습니다: %s"), StructPath),
                TEXT("TaskType") });
            return false;
        }

        FStateTreeEditorNode& TaskItem = Target->Tasks.AddDefaulted_GetRef();
        TaskItem.ID = FGuid::NewGuid();
        TaskItem.Node.InitializeAs(TaskStruct);

        // 인스턴스 데이터 자동 초기화 (GetInstanceDataType() 가상 함수 이용)
        if (const FStateTreeNodeBase* NodeBase = TaskItem.Node.GetPtr<FStateTreeNodeBase>())
        {
            if (const UScriptStruct* InstanceType =
                    Cast<const UScriptStruct>(NodeBase->GetInstanceDataType()))
            {
                TaskItem.Instance.InitializeAs(InstanceType);
            }
            if (const UScriptStruct* RuntimeType =
                    Cast<const UScriptStruct>(NodeBase->GetExecutionRuntimeDataType()))
            {
                TaskItem.ExecutionRuntimeData.InitializeAs(RuntimeType);
            }
        }

        return true;
    }

    bool FStateTreeEditorAdapter::Save(UStateTree* StateTree, TArray<FCommandForgeError>& OutErrors)
    {
        if (!StateTree)
        {
            OutErrors.Add({ TEXT("NULL_STATETREE"), TEXT("StateTree가 null입니다"), TEXT("") });
            return false;
        }

        UPackage* Package = StateTree->GetOutermost();
        Package->SetDirtyFlag(true);

        FSavePackageArgs SaveArgs;
        SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;

        const FString PackageName = Package->GetName();
        const FString FileName    = FPackageName::LongPackageNameToFilename(
            PackageName, FPackageName::GetAssetPackageExtension());

        const bool bSavedOk =
            UPackage::SavePackage(Package, StateTree, *FileName, SaveArgs) &&
            FPlatformFileManager::Get().GetPlatformFile().FileExists(*FileName);

        if (!bSavedOk)
        {
            OutErrors.Add({ TEXT("ST_SAVE_FAILED"),
                FString::Printf(TEXT("StateTree 저장 실패: %s"), *FileName),
                TEXT("AssetPath") });
            return false;
        }

        IAssetRegistry& AR =
            FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
        AR.ScanModifiedAssetFiles({ FileName });

        return true;
    }
}
