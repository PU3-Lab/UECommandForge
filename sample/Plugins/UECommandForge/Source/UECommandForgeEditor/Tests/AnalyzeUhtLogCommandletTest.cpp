#include "Misc/AutomationTest.h"
#include "Commandlets/AnalyzeUhtLogCommandlet.h"
#include "CommandForgeTypes.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAnalyzeUhtLogReportsErrorsTest,
    "UECommandForge.Commandlets.AnalyzeUhtLog.ReportsErrorsAndMarkdown",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAnalyzeUhtLogCleanLogTest,
    "UECommandForge.Commandlets.AnalyzeUhtLog.PassesCleanLog",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace UECommandForgeAnalyzeUhtLogTest
{
    bool LoadAnalyzeUhtLogJsonObject(const FString& Path, TSharedPtr<FJsonObject>& OutObject)
    {
        FString Content;
        if (!FFileHelper::LoadFileToString(Content, *Path))
        {
            return false;
        }

        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
        return FJsonSerializer::Deserialize(Reader, OutObject) && OutObject.IsValid();
    }

    bool HasIssueCode(const TSharedPtr<FJsonObject>& RootObject, const FString& ExpectedCode)
    {
        const TArray<TSharedPtr<FJsonValue>>* Issues = nullptr;
        if (!RootObject->TryGetArrayField(TEXT("issues"), Issues) || Issues == nullptr)
        {
            return false;
        }

        for (const TSharedPtr<FJsonValue>& IssueValue : *Issues)
        {
            const TSharedPtr<FJsonObject>* IssueObject = nullptr;
            if (IssueValue->TryGetObject(IssueObject) && IssueObject != nullptr &&
                (*IssueObject)->GetStringField(TEXT("code")) == ExpectedCode)
            {
                return true;
            }
        }
        return false;
    }
}

bool FAnalyzeUhtLogReportsErrorsTest::RunTest(const FString& Parameters)
{
    using namespace UECommandForgeAnalyzeUhtLogTest;

    const FString UhtLogPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("UECommandForge"), TEXT("Reports"), TEXT("test_uht_error.log"));
    const FString ReportPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("UECommandForge"), TEXT("Reports"), TEXT("test_uht_error_report.json"));
    const FString MarkdownPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("UECommandForge"), TEXT("Reports"), TEXT("test_uht_error_report.md"));

    const FString LogText = TEXT(R"(LogInit: Running UnrealHeaderTool
/Project/Plugins/UECommandForge/Source/CodexBad/Public/BadComponent.h(17): Error: Unrecognized type 'FInventoryRow' - type must be a UCLASS, USTRUCT or UENUM
/Project/Plugins/UECommandForge/Source/CodexBad/Public/BadComponent.h(31): Error: BlueprintReadWrite should not be used on private members
/Project/Plugins/UECommandForge/Source/CodexBad/Public/BadComponent.h(42): Warning: Property has no Category specified
)");

    IFileManager::Get().Delete(*UhtLogPath);
    IFileManager::Get().Delete(*ReportPath);
    IFileManager::Get().Delete(*MarkdownPath);
    FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FPaths::GetPath(UhtLogPath));
    TestTrue(TEXT("UHT error log 저장"),
        FFileHelper::SaveStringToFile(LogText, *UhtLogPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));

    UAnalyzeUhtLogCommandlet* Commandlet = NewObject<UAnalyzeUhtLogCommandlet>();
    const int32 ExitCode = Commandlet->Main(FString::Printf(
        TEXT("-Log=\"%s\" -Output=\"%s\" -Markdown=\"%s\""), *UhtLogPath, *ReportPath, *MarkdownPath));

    TestEqual(TEXT("UHT error exit code"), ExitCode,
        static_cast<int32>(ECommandForgeExitCode::ValidationFailed));

    TSharedPtr<FJsonObject> RootObject;
    TestTrue(TEXT("UHT error report JSON 파싱"), LoadAnalyzeUhtLogJsonObject(ReportPath, RootObject));
    TestFalse(TEXT("UHT error ok false"), RootObject->GetBoolField(TEXT("ok")));
    TestEqual(TEXT("commandlet 이름"), RootObject->GetStringField(TEXT("commandlet")), TEXT("AnalyzeUhtLog"));
    TestEqual(TEXT("error count"), RootObject->GetObjectField(TEXT("validation"))->GetStringField(TEXT("error_count")), TEXT("2"));
    TestEqual(TEXT("warning count"), RootObject->GetObjectField(TEXT("validation"))->GetStringField(TEXT("warning_count")), TEXT("1"));
    TestTrue(TEXT("unknown reflected type issue"), HasIssueCode(RootObject, TEXT("UHT_UNKNOWN_REFLECTED_TYPE")));
    TestTrue(TEXT("private blueprint access issue"), HasIssueCode(RootObject, TEXT("UHT_PRIVATE_BLUEPRINT_ACCESS")));

    FString Markdown;
    TestTrue(TEXT("UHT markdown report 저장"), FFileHelper::LoadFileToString(Markdown, *MarkdownPath));
    TestTrue(TEXT("markdown commandlet title"), Markdown.Contains(TEXT("# AnalyzeUhtLog Report")));
    TestTrue(TEXT("markdown suggested fix"), Markdown.Contains(TEXT("UCLASS/USTRUCT/UENUM")));
    return true;
}

bool FAnalyzeUhtLogCleanLogTest::RunTest(const FString& Parameters)
{
    using namespace UECommandForgeAnalyzeUhtLogTest;

    const FString UhtLogPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("UECommandForge"), TEXT("Reports"), TEXT("test_uht_clean.log"));
    const FString ReportPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("UECommandForge"), TEXT("Reports"), TEXT("test_uht_clean_report.json"));

    const FString LogText = TEXT(R"(LogInit: Running UnrealHeaderTool
Reflection code generated for UECommandForgeEditor in 1.23 seconds
Total of 0 written
)");

    IFileManager::Get().Delete(*UhtLogPath);
    IFileManager::Get().Delete(*ReportPath);
    FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FPaths::GetPath(UhtLogPath));
    TestTrue(TEXT("clean UHT log 저장"),
        FFileHelper::SaveStringToFile(LogText, *UhtLogPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));

    UAnalyzeUhtLogCommandlet* Commandlet = NewObject<UAnalyzeUhtLogCommandlet>();
    const int32 ExitCode = Commandlet->Main(FString::Printf(
        TEXT("-Log=\"%s\" -Output=\"%s\""), *UhtLogPath, *ReportPath));

    TestEqual(TEXT("clean UHT exit code"), ExitCode, 0);

    TSharedPtr<FJsonObject> RootObject;
    TestTrue(TEXT("clean UHT report JSON 파싱"), LoadAnalyzeUhtLogJsonObject(ReportPath, RootObject));
    TestTrue(TEXT("clean UHT ok true"), RootObject->GetBoolField(TEXT("ok")));
    TestEqual(TEXT("clean UHT issue count"), RootObject->GetObjectField(TEXT("validation"))->GetStringField(TEXT("issue_count")), TEXT("0"));
    return true;
}
