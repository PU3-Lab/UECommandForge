#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Reports/JsonReportWriter.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUECommandForgeJsonReportWriterTest,
    "UECommandForge.Reports.JsonReportWriter.WritesOkEnvelope",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUECommandForgeJsonReportWriterTest::RunTest(const FString& Parameters)
{
    const FString OutPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("UECommandForge"), TEXT("Reports"), TEXT("test_writer.json"));
    IFileManager::Get().Delete(*OutPath);

    FCommandForgeReport Report;
    Report.bOk = true;
    Report.Commandlet = TEXT("Hello");
    Report.bRollbackAvailable = true;
    Report.TransactionId = TEXT("tx-123");
    Report.bDryRun = true;
    Report.bApplied = false;
    Report.ChangedAssets.Add(TEXT("/Game/BP_Guard"));
    Report.ChangedFiles.Add(TEXT("Content/AI/GuardAI.json"));
    Report.RollbackPlanPath = TEXT("Saved/UECommandForge/Reports/rollback_tx-123.json");
    Report.PostValidation.Add(TEXT("asset_registry"), TEXT("passed"));

    FCommandForgeValidationIssue Issue;
    Issue.Severity = TEXT("warning");
    Issue.Code = TEXT("UCF_PATH_HINT");
    Issue.Message = TEXT("Blueprint should be created under /Game/AI.");
    Issue.Field = TEXT("asset_path");
    Issue.AssetPath = TEXT("/Game/BP_Guard");
    Issue.FilePath = TEXT("Content/AI/GuardAI.json");
    Issue.SuggestedFix = TEXT("Move asset to /Game/AI");
    Report.ValidationIssues.Add(Issue);

    FCommandForgeStepResult Step;
    Step.Step = TEXT("CreateCharacterBlueprint");
    Step.bOk = true;
    Step.CreatedAssets.Add(TEXT("/Game/BP_Guard"));
    Step.Validation.Add(TEXT("blueprint_compile"), TEXT("passed"));
    Report.Steps.Add(Step);

    UECommandForge::FJsonReportWriter::Write(OutPath, Report);

    FString Content;
    TestTrue(TEXT("파일 존재"), FFileHelper::LoadFileToString(Content, *OutPath));
    TestTrue(TEXT("ok=true 포함"), Content.Contains(TEXT("\"ok\": true")));
    TestTrue(TEXT("commandlet=Hello 포함"),
        Content.Contains(TEXT("\"commandlet\": \"Hello\"")));
    TestTrue(TEXT("rollback_available 포함"),
        Content.Contains(TEXT("\"rollback_available\": true")));
    TestTrue(TEXT("transaction_id 포함"),
        Content.Contains(TEXT("\"transaction_id\": \"tx-123\"")));
    TestTrue(TEXT("dry_run 포함"),
        Content.Contains(TEXT("\"dry_run\": true")));
    TestTrue(TEXT("applied 포함"),
        Content.Contains(TEXT("\"applied\": false")));
    TestTrue(TEXT("changed_assets 포함"),
        Content.Contains(TEXT("\"changed_assets\"")) && Content.Contains(TEXT("\"/Game/BP_Guard\"")));
    TestTrue(TEXT("changed_files 포함"),
        Content.Contains(TEXT("\"changed_files\"")) && Content.Contains(TEXT("\"Content/AI/GuardAI.json\"")));
    TestTrue(TEXT("rollback_plan_path 포함"),
        Content.Contains(TEXT("\"rollback_plan_path\": \"Saved/UECommandForge/Reports/rollback_tx-123.json\"")));
    TestTrue(TEXT("post_validation 포함"),
        Content.Contains(TEXT("\"post_validation\"")) && Content.Contains(TEXT("\"asset_registry\": \"passed\"")));
    TestTrue(TEXT("issues 포함"),
        Content.Contains(TEXT("\"issues\"")) &&
        Content.Contains(TEXT("\"code\": \"UCF_PATH_HINT\"")) &&
        Content.Contains(TEXT("\"suggested_fix\": \"Move asset to /Game/AI\"")));
    TestTrue(TEXT("steps 포함"), Content.Contains(TEXT("\"steps\"")));
    TestTrue(TEXT("step 이름 포함"),
        Content.Contains(TEXT("\"step\": \"CreateCharacterBlueprint\"")));
    TestTrue(TEXT("step created_assets 포함"),
        Content.Contains(TEXT("\"created_assets\"")) && Content.Contains(TEXT("\"/Game/BP_Guard\"")));
    TestTrue(TEXT("step validation 포함"),
        Content.Contains(TEXT("\"blueprint_compile\": \"passed\"")));
    return true;
}
