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

    UECommandForge::FJsonReportWriter::Write(OutPath, Report);

    FString Content;
    TestTrue(TEXT("파일 존재"), FFileHelper::LoadFileToString(Content, *OutPath));
    TestTrue(TEXT("ok=true 포함"), Content.Contains(TEXT("\"ok\": true")));
    TestTrue(TEXT("commandlet=Hello 포함"),
        Content.Contains(TEXT("\"commandlet\": \"Hello\"")));
    return true;
}
