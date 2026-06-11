#include "Misc/AutomationTest.h"
#include "Commandlets/PlanAssetChangesCommandlet.h"
#include "CommandForgeTypes.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPlanAssetChangesCommandletTest,
    "UECommandForge.Commandlets.PlanAssetChanges.WritesRollbackPlan",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPlanAssetChangesDeleteGuardTest,
    "UECommandForge.Commandlets.PlanAssetChanges.RejectsUnsafeDelete",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPlanAssetChangesRollbackPathGuardTest,
    "UECommandForge.Commandlets.PlanAssetChanges.RejectsRollbackPathOutsideReports",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
    bool LoadPlanAssetChangesJsonObject(const FString& Path, TSharedPtr<FJsonObject>& OutObject)
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

bool FPlanAssetChangesCommandletTest::RunTest(const FString& Parameters)
{
    const FString ReportPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("UECommandForge"), TEXT("Reports"), TEXT("test_plan_asset_changes.json"));
    const FString PlanPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("UECommandForge"), TEXT("Reports"), TEXT("test_asset_change_plan.json"));
    const FString RollbackPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("UECommandForge"), TEXT("Reports"), TEXT("test_asset_change_rollback.json"));

    IFileManager::Get().Delete(*ReportPath);
    IFileManager::Get().Delete(*PlanPath);
    IFileManager::Get().Delete(*RollbackPath);

    const FString PlanJson = TEXT(R"({
        "version": "1",
        "kind": "asset_change_plan",
        "transaction_id": "tx-plan-test",
        "rollback_plan": "test_asset_change_rollback.json",
        "operations": [
            {
                "operation": "create_folder",
                "path": "/Game/Tests/Planned"
            },
            {
                "operation": "rename_asset",
                "source": "/Game/Tests/BP_TestCharacter",
                "target": "/Game/Tests/BP_TestCharacter_Renamed"
            },
            {
                "operation": "move_asset",
                "source": "/Game/Tests/BP_TestController",
                "target": "/Game/Tests/Planned/BP_TestController"
            }
        ]
    })");
    TestTrue(TEXT("asset change plan fixture 저장"), FFileHelper::SaveStringToFile(PlanJson, *PlanPath));

    UPlanAssetChangesCommandlet* Commandlet = NewObject<UPlanAssetChangesCommandlet>();
    const int32 ExitCode = Commandlet->Main(FString::Printf(
        TEXT("-Plan=\"%s\" -Output=\"%s\""),
        *PlanPath, *ReportPath));

    TestEqual(TEXT("preflight success exit code"), ExitCode, 0);

    TSharedPtr<FJsonObject> RootObject;
    TestTrue(TEXT("asset change report JSON 파싱"), LoadPlanAssetChangesJsonObject(ReportPath, RootObject));
    TestTrue(TEXT("preflight ok true"), RootObject->GetBoolField(TEXT("ok")));
    TestEqual(TEXT("commandlet 이름"), RootObject->GetStringField(TEXT("commandlet")),
        TEXT("PlanAssetChanges"));
    TestTrue(TEXT("rollback available"), RootObject->GetBoolField(TEXT("rollback_available")));
    TestEqual(TEXT("operation count"), RootObject->GetObjectField(TEXT("validation"))->GetStringField(TEXT("operation_count")),
        TEXT("3"));
    TestTrue(TEXT("rollback plan file 생성"), FPaths::FileExists(RollbackPath));

    FString RollbackContent;
    TestTrue(TEXT("rollback plan 읽기"), FFileHelper::LoadFileToString(RollbackContent, *RollbackPath));
    TestTrue(TEXT("rename rollback operation 포함"),
        RollbackContent.Contains(TEXT("\"planned_operation\": \"rename_asset\"")));
    TestTrue(TEXT("move rollback operation 포함"),
        RollbackContent.Contains(TEXT("\"planned_operation\": \"move_asset\"")));
    TestTrue(TEXT("redirector fixup operation 포함"),
        RollbackContent.Contains(TEXT("\"planned_operation\": \"fix_redirector\"")));
    return true;
}

bool FPlanAssetChangesDeleteGuardTest::RunTest(const FString& Parameters)
{
    const FString ReportPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("UECommandForge"), TEXT("Reports"), TEXT("test_plan_asset_changes_delete_guard.json"));
    const FString PlanPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("UECommandForge"), TEXT("Reports"), TEXT("test_asset_change_delete_guard_plan.json"));

    IFileManager::Get().Delete(*ReportPath);
    IFileManager::Get().Delete(*PlanPath);

    const FString PlanJson = TEXT(R"({
        "version": "1",
        "kind": "asset_change_plan",
        "transaction_id": "tx-delete-guard",
        "operations": [
            {
                "operation": "delete_unused_asset_candidate",
                "asset": "/Game/Tests/*"
            }
        ]
    })");
    TestTrue(TEXT("delete guard plan fixture 저장"), FFileHelper::SaveStringToFile(PlanJson, *PlanPath));

    UPlanAssetChangesCommandlet* Commandlet = NewObject<UPlanAssetChangesCommandlet>();
    const int32 ExitCode = Commandlet->Main(FString::Printf(
        TEXT("-Plan=\"%s\" -Output=\"%s\""),
        *PlanPath, *ReportPath));

    TestEqual(TEXT("unsafe delete exit code"), ExitCode,
        static_cast<int32>(ECommandForgeExitCode::ValidationFailed));

    TSharedPtr<FJsonObject> RootObject;
    TestTrue(TEXT("delete guard report JSON 파싱"), LoadPlanAssetChangesJsonObject(ReportPath, RootObject));
    TestFalse(TEXT("delete guard ok false"), RootObject->GetBoolField(TEXT("ok")));

    const TArray<TSharedPtr<FJsonValue>>* Issues = nullptr;
    TestTrue(TEXT("issues 배열 존재"), RootObject->TryGetArrayField(TEXT("issues"), Issues));
    TestTrue(TEXT("delete guard issues 발생"), Issues != nullptr && Issues->Num() > 0);

    bool bFoundWildcardDelete = false;
    for (const TSharedPtr<FJsonValue>& IssueValue : *Issues)
    {
        const TSharedPtr<FJsonObject>* IssueObject = nullptr;
        if (IssueValue->TryGetObject(IssueObject) &&
            IssueObject != nullptr && IssueObject->IsValid() &&
            (*IssueObject)->GetStringField(TEXT("code")) == TEXT("WILDCARD_DELETE_FORBIDDEN"))
        {
            bFoundWildcardDelete = true;
        }
    }
    TestTrue(TEXT("wildcard delete 금지 issue"), bFoundWildcardDelete);
    return true;
}

bool FPlanAssetChangesRollbackPathGuardTest::RunTest(const FString& Parameters)
{
    const FString ReportPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("UECommandForge"), TEXT("Reports"), TEXT("test_plan_asset_changes_rollback_path_guard.json"));
    const FString PlanPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("UECommandForge"), TEXT("Reports"), TEXT("test_asset_change_rollback_path_guard_plan.json"));
    const FString UnsafeRollbackPath = FPaths::Combine(FPaths::ProjectDir(),
        TEXT("unsafe_rollback.json"));

    IFileManager::Get().Delete(*ReportPath);
    IFileManager::Get().Delete(*PlanPath);
    IFileManager::Get().Delete(*UnsafeRollbackPath);

    const FString PlanJson = FString::Printf(TEXT(R"({
        "version": "1",
        "kind": "asset_change_plan",
        "transaction_id": "tx-rollback-path-guard",
        "rollback_plan": "%s",
        "operations": [
            {
                "operation": "create_folder",
                "path": "/Game/Tests/Planned"
            }
        ]
    })"), *UnsafeRollbackPath.ReplaceCharWithEscapedChar());
    TestTrue(TEXT("unsafe rollback path plan fixture 저장"),
        FFileHelper::SaveStringToFile(PlanJson, *PlanPath));

    UPlanAssetChangesCommandlet* Commandlet = NewObject<UPlanAssetChangesCommandlet>();
    const int32 ExitCode = Commandlet->Main(FString::Printf(
        TEXT("-Plan=\"%s\" -Output=\"%s\""),
        *PlanPath, *ReportPath));

    TestEqual(TEXT("unsafe rollback path exit code"), ExitCode,
        static_cast<int32>(ECommandForgeExitCode::SpecParseFailed));
    TestFalse(TEXT("unsafe rollback file 미생성"), FPaths::FileExists(UnsafeRollbackPath));

    TSharedPtr<FJsonObject> RootObject;
    TestTrue(TEXT("unsafe rollback path report JSON 파싱"), LoadPlanAssetChangesJsonObject(ReportPath, RootObject));
    TestFalse(TEXT("unsafe rollback path ok false"), RootObject->GetBoolField(TEXT("ok")));

    const TArray<TSharedPtr<FJsonValue>>* Errors = nullptr;
    TestTrue(TEXT("errors 배열 존재"), RootObject->TryGetArrayField(TEXT("errors"), Errors));
    TestTrue(TEXT("rollback path guard error 발생"), Errors != nullptr && Errors->Num() > 0);

    bool bFoundRollbackPathGuard = false;
    for (const TSharedPtr<FJsonValue>& ErrorValue : *Errors)
    {
        const TSharedPtr<FJsonObject>* ErrorObject = nullptr;
        if (ErrorValue->TryGetObject(ErrorObject) &&
            ErrorObject != nullptr && ErrorObject->IsValid() &&
            (*ErrorObject)->GetStringField(TEXT("code")) == TEXT("ROLLBACK_PATH_OUTSIDE_REPORT_DIR"))
        {
            bFoundRollbackPathGuard = true;
        }
    }
    TestTrue(TEXT("rollback path report dir guard error"), bFoundRollbackPathGuard);
    return true;
}
