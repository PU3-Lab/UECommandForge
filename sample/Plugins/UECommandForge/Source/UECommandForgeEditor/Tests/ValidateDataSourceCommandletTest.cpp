#include "Misc/AutomationTest.h"
#include "Commandlets/ValidateDataSourceCommandlet.h"
#include "CommandForgeTypes.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FValidateDataSourceCommandletTest,
    "UECommandForge.Commandlets.ValidateDataSource.ReportsCsvPolicyIssues",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
    bool LoadValidateDataSourceJsonObject(const FString& Path, TSharedPtr<FJsonObject>& OutObject)
    {
        FString Content;
        if (!FFileHelper::LoadFileToString(Content, *Path))
        {
            return false;
        }

        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
        return FJsonSerializer::Deserialize(Reader, OutObject) && OutObject.IsValid();
    }

    bool HasValidateDataSourceIssueCode(
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

bool FValidateDataSourceCommandletTest::RunTest(const FString& Parameters)
{
    const FString SchemaPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("CodexReports"), TEXT("test_data_schema.json"));
    const FString SourcePath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("CodexReports"), TEXT("test_data_source.csv"));
    const FString ReportPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("CodexReports"), TEXT("test_validate_data_source_report.json"));

    IFileManager::Get().Delete(*SchemaPath);
    IFileManager::Get().Delete(*SourcePath);
    IFileManager::Get().Delete(*ReportPath);
    FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FPaths::GetPath(SchemaPath));

    const FString SchemaJson = TEXT(R"({
        "version": "1",
        "kind": "data_schema",
        "key_field": "id",
        "source_type": "csv",
        "fields": [
            { "name": "id", "type": "string", "required": true, "unique": true },
            { "name": "level", "type": "int", "required": true, "min": 1, "max": 10 },
            { "name": "rarity", "type": "enum", "allowed_values": ["common", "rare"] }
        ]
    })");
    const FString SourceCsv = TEXT(R"(id,level,rarity
row_1,5,common
row_1,11,legendary
row_3,abc,rare
)");

    TestTrue(TEXT("data schema fixture 저장"),
        FFileHelper::SaveStringToFile(SchemaJson, *SchemaPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
    TestTrue(TEXT("data source fixture 저장"),
        FFileHelper::SaveStringToFile(SourceCsv, *SourcePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));

    UValidateDataSourceCommandlet* Commandlet = NewObject<UValidateDataSourceCommandlet>();
    const int32 ExitCode = Commandlet->Main(FString::Printf(
        TEXT("-Schema=\"%s\" -Source=\"%s\" -Output=\"%s\""),
        *SchemaPath, *SourcePath, *ReportPath));

    TestEqual(TEXT("data source validation exit code"), ExitCode,
        static_cast<int32>(ECommandForgeExitCode::ValidationFailed));

    TSharedPtr<FJsonObject> RootObject;
    TestTrue(TEXT("data source report JSON 파싱"), LoadValidateDataSourceJsonObject(ReportPath, RootObject));
    TestFalse(TEXT("data source report ok false"), RootObject->GetBoolField(TEXT("ok")));
    TestEqual(TEXT("commandlet 이름"), RootObject->GetStringField(TEXT("commandlet")),
        TEXT("ValidateDataSource"));

    const TArray<TSharedPtr<FJsonValue>>* Issues = nullptr;
    TestTrue(TEXT("issues 배열 존재"), RootObject->TryGetArrayField(TEXT("issues"), Issues));
    TestTrue(TEXT("policy issues 발생"), Issues != nullptr && Issues->Num() >= 4);
    if (Issues == nullptr)
    {
        return false;
    }

    TestTrue(TEXT("duplicate value issue"),
        HasValidateDataSourceIssueCode(*Issues, TEXT("DATA_SOURCE_DUPLICATE_VALUE")));
    TestTrue(TEXT("range violation issue"),
        HasValidateDataSourceIssueCode(*Issues, TEXT("DATA_SOURCE_RANGE_VIOLATION")));
    TestTrue(TEXT("allowed value issue"),
        HasValidateDataSourceIssueCode(*Issues, TEXT("DATA_SOURCE_ALLOWED_VALUE_MISMATCH")));
    TestTrue(TEXT("type mismatch issue"),
        HasValidateDataSourceIssueCode(*Issues, TEXT("DATA_SOURCE_TYPE_MISMATCH")));
    return true;
}
