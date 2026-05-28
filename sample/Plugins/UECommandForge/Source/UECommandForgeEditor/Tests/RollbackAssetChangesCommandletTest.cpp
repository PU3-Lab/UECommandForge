#include "Misc/AutomationTest.h"
#include "Commandlets/RollbackAssetChangesCommandlet.h"
#include "CommandForgeTypes.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Reports/RollbackPlanWriter.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRollbackAssetChangesDeleteFolderTest,
    "UECommandForge.Commandlets.RollbackAssetChanges.DeletesCreatedFolder",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRollbackAssetChangesCollisionTest,
    "UECommandForge.Commandlets.RollbackAssetChanges.RejectsPathCollision",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRollbackAssetChangesNonEmptyFolderTest,
    "UECommandForge.Commandlets.RollbackAssetChanges.RejectsNonEmptyFolderDelete",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
    bool LoadRollbackAssetChangesJsonObject(const FString& Path, TSharedPtr<FJsonObject>& OutObject)
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

bool FRollbackAssetChangesNonEmptyFolderTest::RunTest(const FString& Parameters)
{
    const FString ReportPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("CodexReports"), TEXT("test_rollback_asset_changes_non_empty_folder.json"));
    const FString RollbackPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("CodexReports"), TEXT("test_rollback_asset_changes_non_empty_folder_plan.json"));
    const FString FolderPackagePath = TEXT("/Game/Tests/RollbackNonEmptyFolder");
    const FString FolderDiskPath = FPackageName::LongPackageNameToFilename(FolderPackagePath);
    const FString MarkerPath = FPaths::Combine(FolderDiskPath, TEXT("marker.txt"));

    IFileManager::Get().Delete(*ReportPath);
    IFileManager::Get().Delete(*RollbackPath);
    IFileManager::Get().DeleteDirectory(*FolderDiskPath, false, true);
    TestTrue(TEXT("non-empty rollback 대상 폴더 생성"),
        FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FolderDiskPath));
    TestTrue(TEXT("non-empty marker 저장"),
        FFileHelper::SaveStringToFile(TEXT("keep"), *MarkerPath));

    FCommandForgeRollbackPlan Plan;
    Plan.TransactionId = TEXT("tx-rollback-non-empty-folder");
    Plan.Commandlet = TEXT("ApplyAssetChanges");
    Plan.Timestamp = TEXT("2026-05-28T04:42:00Z");

    FCommandForgeRollbackOperation Operation;
    Operation.PlannedOperation = TEXT("delete_folder");
    Operation.AfterAssetPath = FolderPackagePath;
    Operation.AfterFilePath = FolderDiskPath;
    Plan.Operations.Add(Operation);
    TestTrue(TEXT("non-empty rollback plan 저장"),
        UECommandForge::FRollbackPlanWriter::Write(RollbackPath, Plan));

    URollbackAssetChangesCommandlet* Commandlet = NewObject<URollbackAssetChangesCommandlet>();
    const int32 ExitCode = Commandlet->Main(FString::Printf(
        TEXT("-RollbackPlan=\"%s\" -Output=\"%s\""),
        *RollbackPath, *ReportPath));

    TestEqual(TEXT("non-empty folder rollback exit code"), ExitCode,
        static_cast<int32>(ECommandForgeExitCode::ValidationFailed));
    TestTrue(TEXT("non-empty 폴더 유지"), FPaths::DirectoryExists(FolderDiskPath));
    TestTrue(TEXT("non-empty marker 유지"), FPaths::FileExists(MarkerPath));

    TSharedPtr<FJsonObject> RootObject;
    TestTrue(TEXT("non-empty report JSON 파싱"),
        LoadRollbackAssetChangesJsonObject(ReportPath, RootObject));
    TestFalse(TEXT("non-empty ok false"), RootObject->GetBoolField(TEXT("ok")));
    TestFalse(TEXT("non-empty not applied"), RootObject->GetBoolField(TEXT("applied")));

    const TArray<TSharedPtr<FJsonValue>>* Issues = nullptr;
    TestTrue(TEXT("non-empty issues 배열 존재"),
        RootObject->TryGetArrayField(TEXT("issues"), Issues));

    bool bFoundNonEmptyIssue = false;
    for (const TSharedPtr<FJsonValue>& IssueValue : *Issues)
    {
        const TSharedPtr<FJsonObject>* IssueObject = nullptr;
        if (IssueValue->TryGetObject(IssueObject) &&
            IssueObject != nullptr && IssueObject->IsValid() &&
            (*IssueObject)->GetStringField(TEXT("code")) == TEXT("ROLLBACK_FOLDER_NOT_EMPTY"))
        {
            bFoundNonEmptyIssue = true;
        }
    }
    TestTrue(TEXT("non-empty folder issue"), bFoundNonEmptyIssue);

    IFileManager::Get().Delete(*MarkerPath);
    IFileManager::Get().DeleteDirectory(*FolderDiskPath, false, true);
    return true;
}

bool FRollbackAssetChangesDeleteFolderTest::RunTest(const FString& Parameters)
{
    const FString ReportPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("CodexReports"), TEXT("test_rollback_asset_changes_delete_folder.json"));
    const FString RollbackPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("CodexReports"), TEXT("test_rollback_asset_changes_delete_folder_plan.json"));
    const FString FolderPackagePath = TEXT("/Game/Tests/RollbackCommandletFolder");
    const FString FolderDiskPath = FPackageName::LongPackageNameToFilename(FolderPackagePath);

    IFileManager::Get().Delete(*ReportPath);
    IFileManager::Get().Delete(*RollbackPath);
    IFileManager::Get().DeleteDirectory(*FolderDiskPath, false, true);
    TestTrue(TEXT("rollback 대상 폴더 생성"),
        FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FolderDiskPath));

    FCommandForgeRollbackPlan Plan;
    Plan.TransactionId = TEXT("tx-rollback-delete-folder");
    Plan.Commandlet = TEXT("ApplyAssetChanges");
    Plan.Timestamp = TEXT("2026-05-28T04:40:00Z");

    FCommandForgeRollbackOperation Operation;
    Operation.PlannedOperation = TEXT("delete_folder");
    Operation.AfterAssetPath = FolderPackagePath;
    Operation.AfterFilePath = FolderDiskPath;
    Plan.Operations.Add(Operation);
    TestTrue(TEXT("rollback plan 저장"), UECommandForge::FRollbackPlanWriter::Write(RollbackPath, Plan));

    URollbackAssetChangesCommandlet* Commandlet = NewObject<URollbackAssetChangesCommandlet>();
    const int32 ExitCode = Commandlet->Main(FString::Printf(
        TEXT("-RollbackPlan=\"%s\" -Output=\"%s\""),
        *RollbackPath, *ReportPath));

    TestEqual(TEXT("rollback delete folder exit code"), ExitCode, 0);
    TestFalse(TEXT("폴더 rollback 후 제거"), FPaths::DirectoryExists(FolderDiskPath));

    TSharedPtr<FJsonObject> RootObject;
    TestTrue(TEXT("rollback report JSON 파싱"),
        LoadRollbackAssetChangesJsonObject(ReportPath, RootObject));
    TestTrue(TEXT("rollback ok true"), RootObject->GetBoolField(TEXT("ok")));
    TestTrue(TEXT("rollback applied true"), RootObject->GetBoolField(TEXT("applied")));
    TestEqual(TEXT("commandlet 이름"), RootObject->GetStringField(TEXT("commandlet")),
        TEXT("RollbackAssetChanges"));
    TestEqual(TEXT("post validation status"),
        RootObject->GetObjectField(TEXT("post_validation"))->GetStringField(TEXT("status")),
        TEXT("passed"));
    return true;
}

bool FRollbackAssetChangesCollisionTest::RunTest(const FString& Parameters)
{
    const FString ReportPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("CodexReports"), TEXT("test_rollback_asset_changes_collision.json"));
    const FString RollbackPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("CodexReports"), TEXT("test_rollback_asset_changes_collision_plan.json"));

    IFileManager::Get().Delete(*ReportPath);
    IFileManager::Get().Delete(*RollbackPath);

    FCommandForgeRollbackPlan Plan;
    Plan.TransactionId = TEXT("tx-rollback-collision");
    Plan.Commandlet = TEXT("ApplyAssetChanges");
    Plan.Timestamp = TEXT("2026-05-28T04:41:00Z");

    FCommandForgeRollbackOperation Operation;
    Operation.PlannedOperation = TEXT("move_asset");
    Operation.BeforeAssetPath = TEXT("/Game/Tests/BP_TestCharacter");
    Operation.AfterAssetPath = TEXT("/Game/Tests/BP_TestController");
    Plan.Operations.Add(Operation);
    TestTrue(TEXT("collision rollback plan 저장"),
        UECommandForge::FRollbackPlanWriter::Write(RollbackPath, Plan));

    URollbackAssetChangesCommandlet* Commandlet = NewObject<URollbackAssetChangesCommandlet>();
    const int32 ExitCode = Commandlet->Main(FString::Printf(
        TEXT("-RollbackPlan=\"%s\" -Output=\"%s\""),
        *RollbackPath, *ReportPath));

    TestEqual(TEXT("collision rollback exit code"), ExitCode,
        static_cast<int32>(ECommandForgeExitCode::ValidationFailed));

    TSharedPtr<FJsonObject> RootObject;
    TestTrue(TEXT("collision report JSON 파싱"),
        LoadRollbackAssetChangesJsonObject(ReportPath, RootObject));
    TestFalse(TEXT("collision ok false"), RootObject->GetBoolField(TEXT("ok")));
    TestFalse(TEXT("collision not applied"), RootObject->GetBoolField(TEXT("applied")));

    const TArray<TSharedPtr<FJsonValue>>* Issues = nullptr;
    TestTrue(TEXT("issues 배열 존재"), RootObject->TryGetArrayField(TEXT("issues"), Issues));
    TestTrue(TEXT("collision issue 발생"), Issues != nullptr && Issues->Num() > 0);

    bool bFoundCollision = false;
    for (const TSharedPtr<FJsonValue>& IssueValue : *Issues)
    {
        const TSharedPtr<FJsonObject>* IssueObject = nullptr;
        if (IssueValue->TryGetObject(IssueObject) &&
            IssueObject != nullptr && IssueObject->IsValid() &&
            (*IssueObject)->GetStringField(TEXT("code")) == TEXT("PATH_COLLISION"))
        {
            bFoundCollision = true;
        }
    }
    TestTrue(TEXT("path collision issue"), bFoundCollision);
    return true;
}
