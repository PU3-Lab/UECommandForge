#include "Misc/AutomationTest.h"
#include "Commandlets/ApplyAssetChangesCommandlet.h"
#include "CommandForgeTypes.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FApplyAssetChangesCreateFolderTest,
    "UECommandForge.Commandlets.ApplyAssetChanges.CreatesFolderAndRollbackPlan",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FApplyAssetChangesDeleteGuardTest,
    "UECommandForge.Commandlets.ApplyAssetChanges.RejectsUnsafeDelete",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
    bool LoadApplyJsonObject(const FString& Path, TSharedPtr<FJsonObject>& OutObject)
    {
        FString Content;
        if (!FFileHelper::LoadFileToString(Content, *Path))
        {
            return false;
        }

        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
        return FJsonSerializer::Deserialize(Reader, OutObject) && OutObject.IsValid();
    }
}

bool FApplyAssetChangesCreateFolderTest::RunTest(const FString& Parameters)
{
    const FString ReportPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("CodexReports"), TEXT("test_apply_asset_changes_create_folder.json"));
    const FString PlanPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("CodexReports"), TEXT("test_apply_asset_change_plan.json"));
    const FString RollbackPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("CodexReports"), TEXT("test_apply_asset_change_rollback.json"));
    const FString FolderPackagePath = TEXT("/Game/Tests/ApplyCommandletFolder");
    const FString FolderDiskPath = FPackageName::LongPackageNameToFilename(FolderPackagePath);

    IFileManager::Get().Delete(*ReportPath);
    IFileManager::Get().Delete(*PlanPath);
    IFileManager::Get().Delete(*RollbackPath);
    IFileManager::Get().DeleteDirectory(*FolderDiskPath, false, true);

    const FString PlanJson = TEXT(R"({
        "version": "1",
        "kind": "asset_change_plan",
        "transaction_id": "tx-apply-create-folder",
        "rollback_plan": "test_apply_asset_change_rollback.json",
        "operations": [
            {
                "operation": "create_folder",
                "path": "/Game/Tests/ApplyCommandletFolder"
            }
        ]
    })");
    TestTrue(TEXT("apply plan fixture 저장"), FFileHelper::SaveStringToFile(PlanJson, *PlanPath));

    UApplyAssetChangesCommandlet* Commandlet = NewObject<UApplyAssetChangesCommandlet>();
    const int32 ExitCode = Commandlet->Main(FString::Printf(
        TEXT("-Plan=\"%s\" -Output=\"%s\""),
        *PlanPath, *ReportPath));

    TestEqual(TEXT("apply create folder exit code"), ExitCode, 0);
    TestTrue(TEXT("folder 실제 생성"), FPaths::DirectoryExists(FolderDiskPath));
    TestTrue(TEXT("rollback plan 생성"), FPaths::FileExists(RollbackPath));

    TSharedPtr<FJsonObject> RootObject;
    TestTrue(TEXT("apply report JSON 파싱"), LoadApplyJsonObject(ReportPath, RootObject));
    TestTrue(TEXT("apply ok true"), RootObject->GetBoolField(TEXT("ok")));
    TestFalse(TEXT("apply dry_run false"), RootObject->GetBoolField(TEXT("dry_run")));
    TestTrue(TEXT("apply applied true"), RootObject->GetBoolField(TEXT("applied")));
    TestEqual(TEXT("commandlet 이름"), RootObject->GetStringField(TEXT("commandlet")),
        TEXT("ApplyAssetChanges"));

    const TArray<TSharedPtr<FJsonValue>>* ChangedFiles = nullptr;
    TestTrue(TEXT("changed_files 배열 존재"), RootObject->TryGetArrayField(TEXT("changed_files"), ChangedFiles));
    TestTrue(TEXT("changed_files 기록"), ChangedFiles != nullptr && ChangedFiles->Num() > 0);

    IFileManager::Get().DeleteDirectory(*FolderDiskPath, false, true);
    return true;
}

bool FApplyAssetChangesDeleteGuardTest::RunTest(const FString& Parameters)
{
    const FString ReportPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("CodexReports"), TEXT("test_apply_asset_changes_delete_guard.json"));
    const FString PlanPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("CodexReports"), TEXT("test_apply_asset_change_delete_guard_plan.json"));

    IFileManager::Get().Delete(*ReportPath);
    IFileManager::Get().Delete(*PlanPath);

    const FString PlanJson = TEXT(R"({
        "version": "1",
        "kind": "asset_change_plan",
        "transaction_id": "tx-apply-delete-guard",
        "operations": [
            {
                "operation": "delete_unused_asset_candidate",
                "asset": "/Game/Tests/*"
            }
        ]
    })");
    TestTrue(TEXT("delete guard plan fixture 저장"), FFileHelper::SaveStringToFile(PlanJson, *PlanPath));

    UApplyAssetChangesCommandlet* Commandlet = NewObject<UApplyAssetChangesCommandlet>();
    const int32 ExitCode = Commandlet->Main(FString::Printf(
        TEXT("-Plan=\"%s\" -Output=\"%s\""),
        *PlanPath, *ReportPath));

    TestEqual(TEXT("unsafe delete exit code"), ExitCode,
        static_cast<int32>(ECommandForgeExitCode::ValidationFailed));

    TSharedPtr<FJsonObject> RootObject;
    TestTrue(TEXT("delete guard report JSON 파싱"), LoadApplyJsonObject(ReportPath, RootObject));
    TestFalse(TEXT("delete guard ok false"), RootObject->GetBoolField(TEXT("ok")));
    TestFalse(TEXT("unsafe delete not applied"), RootObject->GetBoolField(TEXT("applied")));

    const TArray<TSharedPtr<FJsonValue>>* Issues = nullptr;
    TestTrue(TEXT("issues 배열 존재"), RootObject->TryGetArrayField(TEXT("issues"), Issues));
    TestTrue(TEXT("delete guard issues 발생"), Issues != nullptr && Issues->Num() > 0);
    return true;
}
