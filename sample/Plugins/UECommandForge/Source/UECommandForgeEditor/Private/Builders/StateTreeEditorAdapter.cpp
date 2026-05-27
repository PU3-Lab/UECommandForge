#include "Builders/StateTreeEditorAdapter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Components/StateTreeAIComponentSchema.h"
#include "StateTreeFactory.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "ObjectTools.h"
#include "StateTree.h"
#include "StateTreeCompilerLog.h"
#include "StateTreeEditorData.h"
#include "StateTreeEditingSubsystem.h"
#include "StateTreeState.h"
#include "Tasks/CommandForgeStateTreeTasks.h"
#include "Tasks/StateTreeMoveToTask.h"
#include "UObject/GarbageCollection.h"
#include "UObject/SavePackage.h"

namespace UECommandForge
{
    namespace
    {
        void AddError(TArray<FCommandForgeError>& OutErrors,
                      const TCHAR* Code,
                      const FString& Message,
                      const TCHAR* Field)
        {
            OutErrors.Add({ Code, Message, Field });
        }

        FString PackageFileName(const FString& AssetPath)
        {
            return FPaths::ConvertRelativePathToFull(
                FPackageName::LongPackageNameToFilename(AssetPath, FPackageName::GetAssetPackageExtension()));
        }

        UStateTreeEditorData* GetEditorData(UStateTree* StateTree,
                                            TArray<FCommandForgeError>& OutErrors)
        {
            if (!StateTree)
            {
                AddError(OutErrors, TEXT("STATETREE_NULL"),
                    TEXT("StateTree 에셋이 없습니다."), TEXT("StateTree.AssetPath"));
                return nullptr;
            }

            UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
            if (!EditorData)
            {
                AddError(OutErrors, TEXT("STATETREE_EDITOR_DATA_MISSING"),
                    TEXT("StateTree EditorData를 찾을 수 없습니다."), TEXT("StateTree.AssetPath"));
                return nullptr;
            }
            return EditorData;
        }

        UStateTreeState* GetRootState(UStateTreeEditorData* EditorData)
        {
            if (EditorData->SubTrees.IsEmpty())
            {
                return &EditorData->AddRootState();
            }
            return EditorData->SubTrees[0];
        }

        UStateTreeState* FindChildState(UStateTreeState* RootState, const FName StateName)
        {
            for (TObjectPtr<UStateTreeState>& Child : RootState->Children)
            {
                if (Child && Child->Name == StateName)
                {
                    return Child.Get();
                }
            }
            return nullptr;
        }

        bool DeleteExistingAsset(const FString& AssetPath,
                                 TArray<FCommandForgeError>& OutErrors)
        {
            const FString AssetName = FPackageName::GetLongPackageAssetName(AssetPath);
            if (UPackage* ExistingPackage = FindPackage(nullptr, *AssetPath))
            {
                ExistingPackage->FullyLoad();
                if (UObject* ExistingAsset = FindObject<UObject>(ExistingPackage, *AssetName))
                {
                    TArray<UObject*> AssetsToDelete;
                    AssetsToDelete.Add(ExistingAsset);
                    if (ObjectTools::ForceDeleteObjects(AssetsToDelete, false) != AssetsToDelete.Num())
                    {
                        AddError(OutErrors, TEXT("STATETREE_DELETE_EXISTING_FAILED"),
                            FString::Printf(TEXT("기존 StateTree 삭제 실패: %s"), *AssetPath),
                            TEXT("StateTree.AssetPath"));
                        return false;
                    }
                    CollectGarbage(RF_NoFlags);
                }
            }

            const FString FileName = PackageFileName(AssetPath);
            IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
            if (PlatformFile.FileExists(*FileName) && !PlatformFile.DeleteFile(*FileName))
            {
                AddError(OutErrors, TEXT("STATETREE_DELETE_EXISTING_FILE_FAILED"),
                    FString::Printf(TEXT("기존 StateTree 파일 삭제 실패: %s"), *FileName),
                    TEXT("StateTree.AssetPath"));
                return false;
            }
            return true;
        }
    }

    UStateTree* FStateTreeEditorAdapter::CreateOrLoad(const FString& AssetPath,
                                                      TArray<FCommandForgeError>& OutErrors)
    {
        if (AssetPath.IsEmpty() || !AssetPath.StartsWith(TEXT("/Game/")))
        {
            AddError(OutErrors, TEXT("INVALID_STATETREE_PATH"),
                FString::Printf(TEXT("StateTree 경로는 /Game/ 으로 시작해야 합니다: %s"), *AssetPath),
                TEXT("StateTree.AssetPath"));
            return nullptr;
        }

        if (!DeleteExistingAsset(AssetPath, OutErrors))
        {
            return nullptr;
        }

        UStateTreeFactory* Factory = NewObject<UStateTreeFactory>();
        Factory->SetSchemaClass(UStateTreeAIComponentSchema::StaticClass());

        const FString AssetName = FPackageName::GetLongPackageAssetName(AssetPath);
        const FString PackagePath = FPackageName::GetLongPackagePath(AssetPath);
        IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();

        UObject* Asset = AssetTools.CreateAsset(AssetName, PackagePath, UStateTree::StaticClass(), Factory);
        UStateTree* StateTree = Cast<UStateTree>(Asset);
        if (!StateTree)
        {
            AddError(OutErrors, TEXT("STATETREE_CREATE_FAILED"),
                FString::Printf(TEXT("StateTree 생성 실패: %s"), *AssetPath),
                TEXT("StateTree.AssetPath"));
            return nullptr;
        }

        return StateTree;
    }

    UStateTreeState* FStateTreeEditorAdapter::AddState(UStateTree* StateTree,
                                                       const FString& StateName,
                                                       TArray<FCommandForgeError>& OutErrors)
    {
        if (StateName.IsEmpty())
        {
            AddError(OutErrors, TEXT("STATETREE_STATE_NAME_REQUIRED"),
                TEXT("StateTree StateName은 필수입니다."), TEXT("StateTree.Tasks[].StateName"));
            return nullptr;
        }

        UStateTreeEditorData* EditorData = GetEditorData(StateTree, OutErrors);
        if (!EditorData)
        {
            return nullptr;
        }

        UStateTreeState* RootState = GetRootState(EditorData);
        const FName StateFName(*StateName);
        if (UStateTreeState* ExistingState = FindChildState(RootState, StateFName))
        {
            return ExistingState;
        }

        return &RootState->AddChildState(StateFName);
    }

    bool FStateTreeEditorAdapter::AddTask(UStateTreeState* State,
                                          const FString& TaskType,
                                          TArray<FCommandForgeError>& OutErrors)
    {
        if (!State)
        {
            AddError(OutErrors, TEXT("STATETREE_STATE_MISSING"),
                TEXT("Task를 추가할 State가 없습니다."), TEXT("StateTree.Tasks[].StateName"));
            return false;
        }

        if (TaskType == TEXT("Wait"))
        {
            State->AddTask<FCommandForgeStateTreeWaitTask>();
            return true;
        }

        if (TaskType == TEXT("MoveTo"))
        {
            State->AddTask<FStateTreeMoveToTask>();
            return true;
        }

        if (TaskType == TEXT("PlayMontage"))
        {
            State->AddTask<FCommandForgeStateTreePlayMontageTask>();
            return true;
        }

        AddError(OutErrors, TEXT("INVALID_TASK_TYPE"),
            FString::Printf(TEXT("허용되지 않는 TaskType: %s"), *TaskType),
            TEXT("StateTree.Tasks[].TaskType"));
        return false;
    }

    bool FStateTreeEditorAdapter::Save(UStateTree* StateTree,
                                       TArray<FCommandForgeError>& OutErrors,
                                       TMap<FString, FString>& OutValidation)
    {
        if (!GetEditorData(StateTree, OutErrors))
        {
            return false;
        }

        UStateTreeEditingSubsystem::ValidateStateTree(StateTree);
        FStateTreeCompilerLog Log;
        const bool bCompiled = UStateTreeEditingSubsystem::CompileStateTree(StateTree, Log);
        OutValidation.Add(TEXT("compile_status"), bCompiled ? TEXT("ok") : TEXT("error"));
        if (!bCompiled)
        {
            AddError(OutErrors, TEXT("STATETREE_COMPILE_FAILED"),
                FString::Printf(TEXT("StateTree 컴파일 실패: %s"), *StateTree->GetPathName()),
                TEXT("StateTree.AssetPath"));
            return false;
        }

        const FString FileName = PackageFileName(StateTree->GetOutermost()->GetName());
        IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
        PlatformFile.CreateDirectoryTree(*FPaths::GetPath(FileName));

        UPackage* Package = StateTree->GetOutermost();
        Package->SetDirtyFlag(true);
        FSavePackageArgs SaveArgs;
        SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
        const bool bSavedOk = UPackage::SavePackage(Package, StateTree, *FileName, SaveArgs) &&
                               PlatformFile.FileExists(*FileName);
        OutValidation.Add(TEXT("asset_on_disk"), bSavedOk ? TEXT("true") : TEXT("false"));
        if (!bSavedOk)
        {
            AddError(OutErrors, TEXT("STATETREE_SAVE_FAILED"),
                FString::Printf(TEXT("StateTree 저장 실패: %s"), *FileName),
                TEXT("StateTree.AssetPath"));
            return false;
        }

        IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
        AR.ScanModifiedAssetFiles({ FileName });

        TArray<FAssetData> Assets;
        AR.GetAssetsByPackageName(*Package->GetName(), Assets);
        const bool bInRegistry = (Assets.Num() > 0);
        OutValidation.Add(TEXT("asset_in_registry"), bInRegistry ? TEXT("true") : TEXT("false"));

        return true;
    }
}
