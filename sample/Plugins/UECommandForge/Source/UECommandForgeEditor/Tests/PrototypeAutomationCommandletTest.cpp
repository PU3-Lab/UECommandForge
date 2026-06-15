#include "Misc/AutomationTest.h"
#include "Commandlets/PrototypeAutomationCommandlet.h"
#include "CommandForgeTypes.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPrototypeAutomationCommandletTest,
    "UECommandForge.Commandlets.PrototypeAutomation.AggregatesRuleReports",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPrototypeAutomationCommandletFailureTest,
    "UECommandForge.Commandlets.PrototypeAutomation.ReportsFailedSuite",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
    bool LoadPrototypeAutomationJsonObject(const FString& Path, TSharedPtr<FJsonObject>& OutObject)
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

bool FPrototypeAutomationCommandletTest::RunTest(const FString& Parameters)
{
    const FString FixtureDir = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("UECommandForge"), TEXT("Reports"), TEXT("PrototypeAutomationTest"));
    const FString AssetPolicyPath = FPaths::Combine(FixtureDir, TEXT("asset_policy.json"));
    const FString BuildCsPolicyPath = FPaths::Combine(FixtureDir, TEXT("buildcs_policy.json"));
    const FString DataSchemaPath = FPaths::Combine(FixtureDir, TEXT("data_schema.json"));
    const FString DataSourcePath = FPaths::Combine(FixtureDir, TEXT("data.csv"));
    const FString ConfigRulesPath = FPaths::Combine(FixtureDir, TEXT("config_rules.json"));
    const FString ConfigPath = FPaths::Combine(FixtureDir, TEXT("config.ini"));
    const FString PluginPolicyPath = FPaths::Combine(FixtureDir, TEXT("plugin_policy.json"));
    const FString DiffAllowlistPath = FPaths::Combine(FixtureDir, TEXT("diff_allowlist.json"));
    const FString ReportPath = FPaths::Combine(FixtureDir, TEXT("prototype_report.json"));
    const FString MarkdownPath = FPaths::Combine(FixtureDir, TEXT("prototype_report.md"));

    IFileManager::Get().DeleteDirectory(*FixtureDir, false, true);
    FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FixtureDir);

    const FString AssetPolicyJson = TEXT(R"({
        "version": "1",
        "kind": "asset_policy",
        "rules": [
            { "asset_class": "*", "allowed_path": "/Game/Tests" }
        ]
    })");
    const FString BuildCsPolicyJson = TEXT(R"({
        "version": "1",
        "kind": "buildcs_policy",
        "modules": [
            {
                "name": "UECommandForgeRuntime",
                "required_public": ["Core"],
                "required_private": []
            }
        ]
    })");
    const FString DataSchemaJson = TEXT(R"({
        "version": "1",
        "kind": "data_schema",
        "key_field": "id",
        "source_type": "csv",
        "fields": [
            { "name": "id", "type": "string", "required": true, "unique": true },
            { "name": "level", "type": "int", "required": true, "min": 1, "max": 10 }
        ]
    })");
    const FString DataSourceCsv = TEXT(R"(id,level
row_1,5
row_2,7
)");
    const FString ConfigRulesJson = TEXT(R"({
        "version": "1",
        "kind": "config_rules",
        "config_section": "/Script/UECommandForgeSample.PrototypeSettings",
        "fields": [
            { "name": "bEnabled", "type": "bool", "required": true },
            { "name": "MaxCount", "type": "int", "required": true, "min": 1, "max": 10 }
        ]
    })");
    const FString ConfigIni = TEXT(R"([/Script/UECommandForgeSample.PrototypeSettings]
bEnabled=true
MaxCount=5
)");
    const FString PluginPolicyJson = TEXT(R"({
        "version": "1",
        "kind": "plugin_dependency_policy",
        "required": [],
        "forbiddenInShipping": [],
        "optional": [],
        "allowedEditorOnly": []
    })");
    const FString DiffAllowlistJson = TEXT(R"([
        { "section": "/Script/Engine.RendererSettings", "key": "r.*" }
    ])");

    TestTrue(TEXT("asset policy 저장"),
        FFileHelper::SaveStringToFile(AssetPolicyJson, *AssetPolicyPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
    TestTrue(TEXT("Build.cs policy 저장"),
        FFileHelper::SaveStringToFile(BuildCsPolicyJson, *BuildCsPolicyPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
    TestTrue(TEXT("data schema 저장"),
        FFileHelper::SaveStringToFile(DataSchemaJson, *DataSchemaPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
    TestTrue(TEXT("data source 저장"),
        FFileHelper::SaveStringToFile(DataSourceCsv, *DataSourcePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
    TestTrue(TEXT("config rules 저장"),
        FFileHelper::SaveStringToFile(ConfigRulesJson, *ConfigRulesPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
    TestTrue(TEXT("config ini 저장"),
        FFileHelper::SaveStringToFile(ConfigIni, *ConfigPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
    TestTrue(TEXT("plugin policy 저장"),
        FFileHelper::SaveStringToFile(PluginPolicyJson, *PluginPolicyPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
    TestTrue(TEXT("diff allowlist 저장"),
        FFileHelper::SaveStringToFile(DiffAllowlistJson, *DiffAllowlistPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));

    UPrototypeAutomationCommandlet* Commandlet = NewObject<UPrototypeAutomationCommandlet>();
    const int32 ExitCode = Commandlet->Main(FString::Printf(
        TEXT("-AssetPolicy=\"%s\" -AssetRootPaths=\"/Game/Tests\" -BuildCsPolicy=\"%s\" -DataSchema=\"%s\" -DataSource=\"%s\" -ConfigRules=\"%s\" -Config=\"%s\" -PluginPolicy=\"%s\" -DiffPlatforms=\"%s\" -DiffAllowlist=\"%s\" -Output=\"%s\" -Markdown=\"%s\""),
        *AssetPolicyPath, *BuildCsPolicyPath, *DataSchemaPath, *DataSourcePath,
        *ConfigRulesPath, *ConfigPath, *PluginPolicyPath, TEXT("Mac"), *DiffAllowlistPath, *ReportPath, *MarkdownPath));

    TestEqual(TEXT("prototype automation exit code"), ExitCode, 0);

    TSharedPtr<FJsonObject> RootObject;
    TestTrue(TEXT("prototype report JSON 파싱"),
        LoadPrototypeAutomationJsonObject(ReportPath, RootObject));
    TestTrue(TEXT("prototype ok true"), RootObject->GetBoolField(TEXT("ok")));
    TestEqual(TEXT("commandlet 이름"), RootObject->GetStringField(TEXT("commandlet")),
        TEXT("PrototypeAutomation"));
    TestEqual(TEXT("suite count"),
        RootObject->GetObjectField(TEXT("validation"))->GetStringField(TEXT("suite_count")),
        TEXT("6"));
    TestEqual(TEXT("failed suite count"),
        RootObject->GetObjectField(TEXT("validation"))->GetStringField(TEXT("failed_suite_count")),
        TEXT("0"));
    TestEqual(TEXT("issue count"), RootObject->GetArrayField(TEXT("issues")).Num(), 0);

    FString Markdown;
    TestTrue(TEXT("prototype markdown 저장"),
        FFileHelper::LoadFileToString(Markdown, *MarkdownPath));
    TestTrue(TEXT("markdown title"), Markdown.Contains(TEXT("# PrototypeAutomation Report")));
    TestTrue(TEXT("markdown suite table"), Markdown.Contains(TEXT("ValidateDataSource")));
    TestTrue(TEXT("markdown suite table plugin_deps"), Markdown.Contains(TEXT("ValidatePluginDependencies")));
    TestTrue(TEXT("markdown suite table platform_config_diff"), Markdown.Contains(TEXT("DiffPlatformConfig")));
    return true;
}

bool FPrototypeAutomationCommandletFailureTest::RunTest(const FString& Parameters)
{
    const FString FixtureDir = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("UECommandForge"), TEXT("Reports"), TEXT("PrototypeAutomationFailureTest"));
    const FString DataSchemaPath = FPaths::Combine(FixtureDir, TEXT("data_schema.json"));
    const FString DataSourcePath = FPaths::Combine(FixtureDir, TEXT("data.csv"));
    const FString ReportPath = FPaths::Combine(FixtureDir, TEXT("prototype_report.json"));
    const FString MarkdownPath = FPaths::Combine(FixtureDir, TEXT("prototype_report.md"));

    IFileManager::Get().DeleteDirectory(*FixtureDir, false, true);
    FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FixtureDir);

    const FString DataSchemaJson = TEXT(R"({
        "version": "1",
        "kind": "data_schema",
        "key_field": "id",
        "source_type": "csv",
        "fields": [
            { "name": "id", "type": "string", "required": true, "unique": true },
            { "name": "level", "type": "int", "required": true, "min": 1, "max": 10 }
        ]
    })");
    const FString DataSourceCsv = TEXT(R"(id,level
row_1,5
row_1,42
)");

    TestTrue(TEXT("failure data schema 저장"),
        FFileHelper::SaveStringToFile(DataSchemaJson, *DataSchemaPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
    TestTrue(TEXT("failure data source 저장"),
        FFileHelper::SaveStringToFile(DataSourceCsv, *DataSourcePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));

    UPrototypeAutomationCommandlet* Commandlet = NewObject<UPrototypeAutomationCommandlet>();
    const int32 ExitCode = Commandlet->Main(FString::Printf(
        TEXT("-DataSchema=\"%s\" -DataSource=\"%s\" -Output=\"%s\" -Markdown=\"%s\""),
        *DataSchemaPath, *DataSourcePath, *ReportPath, *MarkdownPath));

    TestEqual(TEXT("prototype automation failure exit code"), ExitCode,
        static_cast<int32>(ECommandForgeExitCode::ValidationFailed));

    TSharedPtr<FJsonObject> RootObject;
    TestTrue(TEXT("prototype failure report JSON 파싱"),
        LoadPrototypeAutomationJsonObject(ReportPath, RootObject));
    TestFalse(TEXT("prototype failure ok false"), RootObject->GetBoolField(TEXT("ok")));
    TestEqual(TEXT("failure suite count"),
        RootObject->GetObjectField(TEXT("validation"))->GetStringField(TEXT("suite_count")),
        TEXT("1"));
    TestEqual(TEXT("failed suite count"),
        RootObject->GetObjectField(TEXT("validation"))->GetStringField(TEXT("failed_suite_count")),
        TEXT("1"));

    const TArray<TSharedPtr<FJsonValue>>& Steps = RootObject->GetArrayField(TEXT("steps"));
    TestEqual(TEXT("failure step count"), Steps.Num(), 1);
    const TSharedPtr<FJsonObject>* StepObject = nullptr;
    TestTrue(TEXT("failure step object"), Steps[0]->TryGetObject(StepObject));
    TestFalse(TEXT("failure step ok false"), (*StepObject)->GetBoolField(TEXT("ok")));
    TestEqual(TEXT("failure step exit code"),
        (*StepObject)->GetObjectField(TEXT("validation"))->GetStringField(TEXT("exit_code")),
        FString::FromInt(static_cast<int32>(ECommandForgeExitCode::ValidationFailed)));

    const TArray<TSharedPtr<FJsonValue>>& Issues = RootObject->GetArrayField(TEXT("issues"));
    TestEqual(TEXT("prototype suite issue count"), Issues.Num(), 1);
    const TSharedPtr<FJsonObject>* IssueObject = nullptr;
    TestTrue(TEXT("prototype suite issue object"), Issues[0]->TryGetObject(IssueObject));
    TestEqual(TEXT("prototype suite issue code"),
        (*IssueObject)->GetStringField(TEXT("code")), TEXT("PROTOTYPE_SUITE_FAILED"));

    FString Markdown;
    TestTrue(TEXT("prototype failure markdown 저장"),
        FFileHelper::LoadFileToString(Markdown, *MarkdownPath));
    TestTrue(TEXT("markdown failure status"), Markdown.Contains(TEXT("- Status: FAIL")));
    return true;
}

void LinkPrototypeAutomationCommandletTest()
{
}

