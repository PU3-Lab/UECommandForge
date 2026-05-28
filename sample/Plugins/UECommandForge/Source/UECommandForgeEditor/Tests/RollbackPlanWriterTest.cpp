#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Reports/RollbackPlanWriter.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUECommandForgeRollbackPlanWriterTest,
    "UECommandForge.Reports.RollbackPlanWriter.WritesRollbackPlan",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUECommandForgeRollbackPlanWriterTest::RunTest(const FString& Parameters)
{
    const FString OutPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("CodexReports"), TEXT("test_rollback_plan.json"));
    IFileManager::Get().Delete(*OutPath);

    FCommandForgeRollbackPlan Plan;
    Plan.TransactionId = TEXT("tx-rollback-1");
    Plan.Commandlet = TEXT("ApplyAssetChanges");
    Plan.Timestamp = TEXT("2026-05-28T01:30:00Z");

    FCommandForgeRollbackOperation Operation;
    Operation.PlannedOperation = TEXT("move_asset");
    Operation.BeforeAssetPath = TEXT("/Game/AI/BP_Guard");
    Operation.AfterAssetPath = TEXT("/Game/Characters/BP_Guard");
    Operation.DependencySnapshot.Add(TEXT("/Game/Maps/TestMap"));
    Operation.ValidationSummary.Add(TEXT("referencers"), TEXT("1"));
    Plan.Operations.Add(Operation);

    UECommandForge::FRollbackPlanWriter::Write(OutPath, Plan);

    FString Content;
    TestTrue(TEXT("파일 존재"), FFileHelper::LoadFileToString(Content, *OutPath));
    TestTrue(TEXT("transaction_id 포함"),
        Content.Contains(TEXT("\"transaction_id\": \"tx-rollback-1\"")));
    TestTrue(TEXT("commandlet 포함"),
        Content.Contains(TEXT("\"commandlet\": \"ApplyAssetChanges\"")));
    TestTrue(TEXT("operation 포함"),
        Content.Contains(TEXT("\"planned_operation\": \"move_asset\"")));
    TestTrue(TEXT("before/after asset path 포함"),
        Content.Contains(TEXT("\"before_asset_path\": \"/Game/AI/BP_Guard\"")) &&
        Content.Contains(TEXT("\"after_asset_path\": \"/Game/Characters/BP_Guard\"")));
    TestTrue(TEXT("dependency snapshot 포함"),
        Content.Contains(TEXT("\"dependency_snapshot\"")) &&
        Content.Contains(TEXT("\"/Game/Maps/TestMap\"")));
    TestTrue(TEXT("validation summary 포함"),
        Content.Contains(TEXT("\"validation_summary\"")) &&
        Content.Contains(TEXT("\"referencers\": \"1\"")));
    return true;
}
