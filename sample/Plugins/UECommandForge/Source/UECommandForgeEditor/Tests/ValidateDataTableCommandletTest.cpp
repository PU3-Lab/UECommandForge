#include "Misc/AutomationTest.h"
#include "Commandlets/ValidateDataTableCommandlet.h"
#include "CommandForgeTypes.h"
#include "Dom/JsonObject.h"
#include "Engine/DataTable.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "ObjectTools.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FValidateDataTableCommandletTest,
    "UECommandForge.Commandlets.ValidateDataTable.ReportsDataTableIssues",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
    const FString ValidateDataTablePackageName = TEXT("/Game/Tests/DataValidation/DT_CodexValidateDataTable");

    FString ValidateDataTableObjectPath()
    {
        return ValidateDataTablePackageName + TEXT(".DT_CodexValidateDataTable");
    }

    bool LoadValidateDataTableJsonObject(const FString& Path, TSharedPtr<FJsonObject>& OutObject)
    {
        FString Content;
        if (!FFileHelper::LoadFileToString(Content, *Path))
        {
            return false;
        }

        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
        return FJsonSerializer::Deserialize(Reader, OutObject) && OutObject.IsValid();
    }

    bool HasValidateDataTableIssueCode(
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

    void DeleteValidateDataTableAsset()
    {
        if (UObject* Existing = LoadObject<UObject>(nullptr, *ValidateDataTableObjectPath()))
        {
            TArray<UObject*> ObjectsToDelete;
            ObjectsToDelete.Add(Existing);
            ObjectTools::ForceDeleteObjects(ObjectsToDelete, false);
        }

        const FString PackageFilename = FPackageName::LongPackageNameToFilename(
            ValidateDataTablePackageName, FPackageName::GetAssetPackageExtension());
        IFileManager::Get().Delete(*PackageFilename);
    }

    UDataTable* CreateValidateDataTableFixture()
    {
        DeleteValidateDataTableAsset();

        UPackage* Package = CreatePackage(*ValidateDataTablePackageName);
        if (Package == nullptr)
        {
            return nullptr;
        }

        UDataTable* DataTable = NewObject<UDataTable>(Package,
            FName(*FPackageName::GetLongPackageAssetName(ValidateDataTablePackageName)),
            RF_Public | RF_Standalone);
        if (DataTable == nullptr)
        {
            return nullptr;
        }

        UScriptStruct* RowStruct = LoadObject<UScriptStruct>(
            nullptr, TEXT("/Script/GameplayTags.GameplayTagTableRow"));
        DataTable->RowStruct = RowStruct;
        const FString SourceCsv = TEXT(R"(,Tag,DevComment
0,Codex.Validate.Row0,
1,Codex.Validate.Row1,unexpected comment
)");
        DataTable->CreateTableFromCSVString(SourceCsv);
        return DataTable;
    }
}

bool FValidateDataTableCommandletTest::RunTest(const FString& Parameters)
{
    const FString SchemaPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("CodexReports"), TEXT("test_validate_datatable_schema.json"));
    const FString ReportPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("CodexReports"), TEXT("test_validate_datatable_report.json"));

    IFileManager::Get().Delete(*SchemaPath);
    IFileManager::Get().Delete(*ReportPath);
    FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FPaths::GetPath(SchemaPath));

    const FString SchemaJson = FString::Printf(TEXT(R"({
        "version": "1",
        "kind": "data_schema",
        "target_asset_path": "%s",
        "row_struct_path": "/Script/GameplayTags.GameplayTagTableRow",
        "fields": [
            { "name": "Tag", "type": "gameplay_tag", "required": true },
            { "name": "DevComment", "type": "string", "required": true, "allowed_values": ["approved"] },
            { "name": "MissingField", "type": "string", "required": true }
        ]
    })"), *ValidateDataTablePackageName);
    TestTrue(TEXT("datatable schema fixture 저장"),
        FFileHelper::SaveStringToFile(SchemaJson, *SchemaPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
    TestNotNull(TEXT("DataTable fixture 생성"), CreateValidateDataTableFixture());

    UValidateDataTableCommandlet* Commandlet = NewObject<UValidateDataTableCommandlet>();
    const int32 ExitCode = Commandlet->Main(FString::Printf(
        TEXT("-Schema=\"%s\" -Asset=\"%s\" -Output=\"%s\""),
        *SchemaPath, *ValidateDataTablePackageName, *ReportPath));

    TestEqual(TEXT("datatable validation exit code"), ExitCode,
        static_cast<int32>(ECommandForgeExitCode::ValidationFailed));

    TSharedPtr<FJsonObject> RootObject;
    TestTrue(TEXT("datatable report JSON 파싱"),
        LoadValidateDataTableJsonObject(ReportPath, RootObject));
    TestFalse(TEXT("datatable report ok false"), RootObject->GetBoolField(TEXT("ok")));
    TestEqual(TEXT("commandlet 이름"), RootObject->GetStringField(TEXT("commandlet")),
        TEXT("ValidateDataTable"));

    const TSharedPtr<FJsonObject> Validation = RootObject->GetObjectField(TEXT("validation"));
    TestEqual(TEXT("validated asset path"), Validation->GetStringField(TEXT("asset_path")),
        ValidateDataTablePackageName);
    TestEqual(TEXT("row count recorded"), Validation->GetStringField(TEXT("row_count")), TEXT("2"));

    const TArray<TSharedPtr<FJsonValue>>* Issues = nullptr;
    TestTrue(TEXT("issues 배열 존재"), RootObject->TryGetArrayField(TEXT("issues"), Issues));
    TestTrue(TEXT("datatable issues 발생"), Issues != nullptr && Issues->Num() >= 3);
    if (Issues == nullptr)
    {
        DeleteValidateDataTableAsset();
        return false;
    }

    TestTrue(TEXT("missing row struct field issue"),
        HasValidateDataTableIssueCode(*Issues, TEXT("DATATABLE_FIELD_MISSING")));
    TestTrue(TEXT("required value issue"),
        HasValidateDataTableIssueCode(*Issues, TEXT("DATATABLE_REQUIRED_FIELD_MISSING")));
    TestTrue(TEXT("allowed value issue"),
        HasValidateDataTableIssueCode(*Issues, TEXT("DATATABLE_ALLOWED_VALUE_MISMATCH")));

    DeleteValidateDataTableAsset();
    return true;
}
