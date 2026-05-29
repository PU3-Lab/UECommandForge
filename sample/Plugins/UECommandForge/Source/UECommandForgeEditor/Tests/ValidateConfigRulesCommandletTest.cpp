#include "Misc/AutomationTest.h"
#include "Commandlets/ValidateConfigRulesCommandlet.h"
#include "CommandForgeTypes.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FValidateConfigRulesCommandletTest,
    "UECommandForge.Commandlets.ValidateConfigRules.ReportsConfigIssues",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
    bool LoadValidateConfigRulesJsonObject(const FString& Path, TSharedPtr<FJsonObject>& OutObject)
    {
        FString Content;
        if (!FFileHelper::LoadFileToString(Content, *Path))
        {
            return false;
        }

        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
        return FJsonSerializer::Deserialize(Reader, OutObject) && OutObject.IsValid();
    }

    bool HasValidateConfigRulesIssueCode(
        const TArray<TSharedPtr<FJsonValue>>& Issues, const FString& ExpectedCode)
    {
        for (const TSharedPtr<FJsonValue>& IssueValue : Issues)
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

bool FValidateConfigRulesCommandletTest::RunTest(const FString& Parameters)
{
    const FString SchemaPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("CodexReports"), TEXT("test_config_rules_schema.json"));
    const FString ConfigPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("CodexReports"), TEXT("test_config_rules.ini"));
    const FString ReportPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("CodexReports"), TEXT("test_validate_config_rules_report.json"));

    IFileManager::Get().Delete(*SchemaPath);
    IFileManager::Get().Delete(*ConfigPath);
    IFileManager::Get().Delete(*ReportPath);
    FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FPaths::GetPath(SchemaPath));

    const FString SchemaJson = TEXT(R"({
        "version": "1",
        "kind": "config_rules",
        "config_section": "/Script/UECommandForgeSample.TestSettings",
        "fields": [
            { "name": "RequiredName", "type": "string", "required": true },
            { "name": "bEnabled", "type": "bool", "required": true },
            { "name": "MaxCount", "type": "int", "required": true, "min": 1, "max": 10 },
            { "name": "Mode", "type": "string", "required": true, "allowed_values": ["Local", "Remote"] },
            { "name": "MissingWithDefault", "type": "float", "required": true, "default": "0.5" }
        ]
    })");
    const FString ConfigIni = TEXT(R"([/Script/UECommandForgeSample.TestSettings]
bEnabled=maybe
MaxCount=42
Mode=Experimental
)");

    TestTrue(TEXT("config rules schema fixture 저장"),
        FFileHelper::SaveStringToFile(SchemaJson, *SchemaPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
    TestTrue(TEXT("config fixture 저장"),
        FFileHelper::SaveStringToFile(ConfigIni, *ConfigPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));

    UValidateConfigRulesCommandlet* Commandlet = NewObject<UValidateConfigRulesCommandlet>();
    const int32 ExitCode = Commandlet->Main(FString::Printf(
        TEXT("-Schema=\"%s\" -Config=\"%s\" -Output=\"%s\""),
        *SchemaPath, *ConfigPath, *ReportPath));

    TestEqual(TEXT("config rules validation exit code"), ExitCode,
        static_cast<int32>(ECommandForgeExitCode::ValidationFailed));

    TSharedPtr<FJsonObject> RootObject;
    TestTrue(TEXT("config rules report JSON 파싱"), LoadValidateConfigRulesJsonObject(ReportPath, RootObject));
    TestFalse(TEXT("config rules report ok false"), RootObject->GetBoolField(TEXT("ok")));
    TestEqual(TEXT("commandlet 이름"), RootObject->GetStringField(TEXT("commandlet")),
        TEXT("ValidateConfigRules"));

    const TArray<TSharedPtr<FJsonValue>>* Issues = nullptr;
    TestTrue(TEXT("issues 배열 존재"), RootObject->TryGetArrayField(TEXT("issues"), Issues));
    TestTrue(TEXT("policy issues 발생"), Issues != nullptr && Issues->Num() >= 5);
    if (Issues == nullptr)
    {
        return false;
    }

    TestTrue(TEXT("missing required key issue"),
        HasValidateConfigRulesIssueCode(*Issues, TEXT("CONFIG_REQUIRED_KEY_MISSING")));
    TestTrue(TEXT("default suggestion issue"),
        HasValidateConfigRulesIssueCode(*Issues, TEXT("CONFIG_DEFAULT_SUGGESTED")));
    TestTrue(TEXT("type mismatch issue"),
        HasValidateConfigRulesIssueCode(*Issues, TEXT("CONFIG_TYPE_MISMATCH")));
    TestTrue(TEXT("range violation issue"),
        HasValidateConfigRulesIssueCode(*Issues, TEXT("CONFIG_RANGE_VIOLATION")));
    TestTrue(TEXT("allowed value issue"),
        HasValidateConfigRulesIssueCode(*Issues, TEXT("CONFIG_ALLOWED_VALUE_MISMATCH")));
    return true;
}
