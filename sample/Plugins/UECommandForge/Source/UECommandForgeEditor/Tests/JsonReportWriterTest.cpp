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
        TEXT("CodexReports"), TEXT("test_writer.json"));
    IFileManager::Get().Delete(*OutPath);

    FCommandForgeReport Report;
    Report.bOk = true;
    Report.Commandlet = TEXT("Hello");
    Report.bRollbackAvailable = true;

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
    TestTrue(TEXT("steps 포함"), Content.Contains(TEXT("\"steps\"")));
    TestTrue(TEXT("step 이름 포함"),
        Content.Contains(TEXT("\"step\": \"CreateCharacterBlueprint\"")));
    TestTrue(TEXT("step created_assets 포함"),
        Content.Contains(TEXT("\"created_assets\"")) && Content.Contains(TEXT("\"/Game/BP_Guard\"")));
    TestTrue(TEXT("step validation 포함"),
        Content.Contains(TEXT("\"blueprint_compile\": \"passed\"")));
    return true;
}
