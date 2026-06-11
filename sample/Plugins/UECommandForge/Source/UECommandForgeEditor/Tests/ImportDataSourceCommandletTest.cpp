#include "Misc/AutomationTest.h"
#include "Commandlets/ImportDataSourceCommandlet.h"
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
    FImportDataSourceDryRunTest,
    "UECommandForge.Commandlets.ImportDataSource.DryRunReportsDiff",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FImportDataSourceApplyDataTableTest,
    "UECommandForge.Commandlets.ImportDataSource.ApplyCreatesDataTable",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
    const FString ImportDataSourcePackageName = TEXT("/Game/Tests/DataImport/DT_CodexImportDataSource");

    bool LoadImportDataSourceJsonObject(const FString& Path, TSharedPtr<FJsonObject>& OutObject)
    {
        FString Content;
        if (!FFileHelper::LoadFileToString(Content, *Path))
        {
            return false;
        }

        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
        return FJsonSerializer::Deserialize(Reader, OutObject) && OutObject.IsValid();
    }

    FString ImportDataSourceObjectPath()
    {
        return ImportDataSourcePackageName + TEXT(".DT_CodexImportDataSource");
    }

    void DeleteImportDataSourceAsset()
    {
        if (UObject* Existing = LoadObject<UObject>(nullptr, *ImportDataSourceObjectPath()))
        {
            TArray<UObject*> ObjectsToDelete;
            ObjectsToDelete.Add(Existing);
            ObjectTools::ForceDeleteObjects(ObjectsToDelete, false);
        }

        const FString PackageFilename = FPackageName::LongPackageNameToFilename(
            ImportDataSourcePackageName, FPackageName::GetAssetPackageExtension());
        IFileManager::Get().Delete(*PackageFilename);
    }

    bool SaveImportDataSourceFixtures(const FString& SchemaPath, const FString& SourcePath)
    {
        const FString SchemaJson = FString::Printf(TEXT(R"({
            "version": "1",
            "kind": "data_schema",
            "source_type": "csv",
            "target_asset_path": "%s",
            "row_struct_path": "/Script/GameplayTags.GameplayTagTableRow",
            "fields": [
                { "name": "Tag", "type": "string", "required": true },
                { "name": "DevComment", "type": "string" }
            ]
        })"), *ImportDataSourcePackageName);
        const FString SourceCsv = TEXT(R"(,Tag,DevComment
0,Codex.Import.Row0,first row
1,Codex.Import.Row1,second row
)");

        FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FPaths::GetPath(SchemaPath));
        return FFileHelper::SaveStringToFile(SchemaJson, *SchemaPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM) &&
            FFileHelper::SaveStringToFile(SourceCsv, *SourcePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    }
}

bool FImportDataSourceDryRunTest::RunTest(const FString& Parameters)
{
    const FString SchemaPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("UECommandForge"), TEXT("Reports"), TEXT("test_import_data_source_schema_dry_run.json"));
    const FString SourcePath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("UECommandForge"), TEXT("Reports"), TEXT("test_import_data_source_dry_run.csv"));
    const FString ReportPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("UECommandForge"), TEXT("Reports"), TEXT("test_import_data_source_dry_run_report.json"));

    DeleteImportDataSourceAsset();
    IFileManager::Get().Delete(*SchemaPath);
    IFileManager::Get().Delete(*SourcePath);
    IFileManager::Get().Delete(*ReportPath);
    TestTrue(TEXT("import dry-run fixture 저장"),
        SaveImportDataSourceFixtures(SchemaPath, SourcePath));

    UImportDataSourceCommandlet* Commandlet = NewObject<UImportDataSourceCommandlet>();
    const int32 ExitCode = Commandlet->Main(FString::Printf(
        TEXT("-Schema=\"%s\" -Source=\"%s\" -Output=\"%s\" -DryRun"),
        *SchemaPath, *SourcePath, *ReportPath));

    TestEqual(TEXT("import dry-run exit code"), ExitCode, 0);
    TestNull(TEXT("dry-run does not create asset"),
        LoadObject<UDataTable>(nullptr, *ImportDataSourceObjectPath()));

    TSharedPtr<FJsonObject> RootObject;
    TestTrue(TEXT("import dry-run report JSON 파싱"),
        LoadImportDataSourceJsonObject(ReportPath, RootObject));
    TestTrue(TEXT("dry-run ok true"), RootObject->GetBoolField(TEXT("ok")));
    TestTrue(TEXT("dry-run flag true"), RootObject->GetBoolField(TEXT("dry_run")));
    TestFalse(TEXT("dry-run not applied"), RootObject->GetBoolField(TEXT("applied")));
    TestEqual(TEXT("commandlet 이름"), RootObject->GetStringField(TEXT("commandlet")),
        TEXT("ImportDataSource"));
    TestTrue(TEXT("rollback not needed in dry-run"),
        !RootObject->GetBoolField(TEXT("rollback_available")));

    TSharedPtr<FJsonObject> Validation = RootObject->GetObjectField(TEXT("validation"));
    TestEqual(TEXT("dry-run planned target"), Validation->GetStringField(TEXT("target_asset_path")),
        ImportDataSourcePackageName);
    TestEqual(TEXT("dry-run row count"), Validation->GetStringField(TEXT("row_count")), TEXT("2"));
    TestFalse(TEXT("source hash recorded"), Validation->GetStringField(TEXT("source_hash")).IsEmpty());
    return true;
}

bool FImportDataSourceApplyDataTableTest::RunTest(const FString& Parameters)
{
    const FString SchemaPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("UECommandForge"), TEXT("Reports"), TEXT("test_import_data_source_schema_apply.json"));
    const FString SourcePath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("UECommandForge"), TEXT("Reports"), TEXT("test_import_data_source_apply.csv"));
    const FString ReportPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("UECommandForge"), TEXT("Reports"), TEXT("test_import_data_source_apply_report.json"));
    const FString RollbackPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("UECommandForge"), TEXT("Reports"), TEXT("test_import_data_source_rollback.json"));

    DeleteImportDataSourceAsset();
    IFileManager::Get().Delete(*SchemaPath);
    IFileManager::Get().Delete(*SourcePath);
    IFileManager::Get().Delete(*ReportPath);
    IFileManager::Get().Delete(*RollbackPath);
    TestTrue(TEXT("import apply fixture 저장"),
        SaveImportDataSourceFixtures(SchemaPath, SourcePath));

    UImportDataSourceCommandlet* Commandlet = NewObject<UImportDataSourceCommandlet>();
    const int32 ExitCode = Commandlet->Main(FString::Printf(
        TEXT("-Schema=\"%s\" -Source=\"%s\" -Output=\"%s\" -RollbackPlan=\"%s\""),
        *SchemaPath, *SourcePath, *ReportPath, *RollbackPath));

    TestEqual(TEXT("import apply exit code"), ExitCode, 0);

    UDataTable* ImportedTable = LoadObject<UDataTable>(nullptr, *ImportDataSourceObjectPath());
    TestNotNull(TEXT("DataTable asset 생성"), ImportedTable);
    if (ImportedTable != nullptr)
    {
        TestEqual(TEXT("DataTable row count"), ImportedTable->GetRowMap().Num(), 2);
        TestNotNull(TEXT("DataTable row struct"), ImportedTable->GetRowStruct());
    }
    TestTrue(TEXT("rollback plan 생성"), FPaths::FileExists(RollbackPath));

    TSharedPtr<FJsonObject> RootObject;
    TestTrue(TEXT("import apply report JSON 파싱"),
        LoadImportDataSourceJsonObject(ReportPath, RootObject));
    TestTrue(TEXT("apply ok true"), RootObject->GetBoolField(TEXT("ok")));
    TestFalse(TEXT("apply dry_run false"), RootObject->GetBoolField(TEXT("dry_run")));
    TestTrue(TEXT("apply applied true"), RootObject->GetBoolField(TEXT("applied")));
    TestTrue(TEXT("rollback available"), RootObject->GetBoolField(TEXT("rollback_available")));

    DeleteImportDataSourceAsset();
    return true;
}
