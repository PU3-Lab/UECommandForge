#include "Commandlets/ImportDataSourceCommandlet.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "CommandForgeTypes.h"
#include "Engine/DataTable.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Reports/JsonReportWriter.h"
#include "Reports/ReportPaths.h"
#include "Reports/RollbackPlanWriter.h"
#include "Specs/DataSchemaSpec.h"
#include "Specs/DataSchemaSpecParser.h"
#include "UObject/SavePackage.h"

UImportDataSourceCommandlet::UImportDataSourceCommandlet()
{
    IsClient     = false;
    IsServer     = false;
    IsEditor     = true;
    LogToConsole = true;
}

namespace UECommandForge::ImportDataSourcePrivate
{
    void AddError(FCommandForgeReport& Report, const FString& Code, const FString& Message,
        const FString& Field)
    {
        FCommandForgeError Error;
        Error.Code = Code;
        Error.Message = Message;
        Error.Field = Field;
        Report.Errors.Add(Error);
    }

    void AddIssue(FCommandForgeReport& Report, const FString& Severity, const FString& Code,
        const FString& Message, const FString& Field, const FString& AssetPath,
        const FString& SuggestedFix)
    {
        FCommandForgeValidationIssue Issue;
        Issue.Severity = Severity;
        Issue.Code = Code;
        Issue.Message = Message;
        Issue.Field = Field;
        Issue.AssetPath = AssetPath;
        Issue.SuggestedFix = SuggestedFix;
        Report.ValidationIssues.Add(Issue);
    }

    bool HasErrorIssue(const TArray<FCommandForgeValidationIssue>& Issues)
    {
        for (const FCommandForgeValidationIssue& Issue : Issues)
        {
            if (Issue.Severity.Equals(TEXT("error"), ESearchCase::IgnoreCase))
            {
                return true;
            }
        }
        return false;
    }

    FString ObjectPathOf(const FString& PackageName)
    {
        return PackageName + TEXT(".") + FPackageName::GetLongPackageAssetName(PackageName);
    }

    FString PackagePathOf(const FString& PackageName)
    {
        int32 SlashIndex = INDEX_NONE;
        if (PackageName.FindLastChar(TEXT('/'), SlashIndex))
        {
            return PackageName.Left(SlashIndex);
        }
        return PackageName;
    }

    bool IsAllowedMountPath(const FString& Path)
    {
        return Path.StartsWith(TEXT("/Game")) || Path.StartsWith(TEXT("/UECommandForge"));
    }

    bool IsValidPackagePath(const FString& Path)
    {
        return IsAllowedMountPath(Path) && FPackageName::IsValidLongPackageName(Path, false);
    }

    bool TryResolveStruct(const FString& RowStructPath, UScriptStruct*& OutStruct,
        FCommandForgeReport& Report)
    {
        OutStruct = nullptr;
        if (RowStructPath.IsEmpty())
        {
            AddError(Report, TEXT("MISSING_ROW_STRUCT"),
                TEXT("DataTable import에는 row_struct_path가 필요합니다."), TEXT("row_struct_path"));
            return false;
        }

        OutStruct = LoadObject<UScriptStruct>(nullptr, *RowStructPath);
        if (OutStruct == nullptr)
        {
            AddError(Report, TEXT("ROW_STRUCT_LOAD_FAILED"),
                TEXT("row struct를 로드할 수 없습니다."), TEXT("row_struct_path"));
            return false;
        }
        if (!OutStruct->IsChildOf(FTableRowBase::StaticStruct()))
        {
            AddError(Report, TEXT("ROW_STRUCT_INVALID"),
                TEXT("row struct는 FTableRowBase를 상속해야 합니다."), TEXT("row_struct_path"));
            return false;
        }
        return true;
    }

    FString Sha1ForString(const FString& Content)
    {
        FTCHARToUTF8 Converted(*Content);
        FSHA1 Sha;
        Sha.Update(reinterpret_cast<const uint8*>(Converted.Get()), Converted.Length());
        Sha.Final();

        uint8 Digest[20];
        Sha.GetHash(Digest);

        FString Result;
        for (const uint8 Byte : Digest)
        {
            Result += FString::Printf(TEXT("%02x"), Byte);
        }
        return Result;
    }

    int32 CountDataRows(const FString& SourceType, const FString& SourceContent)
    {
        if (SourceType.Equals(TEXT("json"), ESearchCase::IgnoreCase))
        {
            TSharedPtr<FJsonValue> RootValue;
            const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(SourceContent);
            if (!FJsonSerializer::Deserialize(Reader, RootValue) || !RootValue.IsValid())
            {
                return 0;
            }
            if (RootValue->Type == EJson::Array)
            {
                return RootValue->AsArray().Num();
            }
            if (RootValue->Type == EJson::Object)
            {
                const TSharedPtr<FJsonObject> RootObject = RootValue->AsObject();
                const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
                if ((RootObject->TryGetArrayField(TEXT("rows"), Rows) ||
                        RootObject->TryGetArrayField(TEXT("data"), Rows)) &&
                    Rows != nullptr)
                {
                    return Rows->Num();
                }
            }
            return 0;
        }

        TArray<FString> Lines;
        SourceContent.ParseIntoArrayLines(Lines, true);
        return FMath::Max(0, Lines.Num() - 1);
    }

    FString RollbackReportDirectory()
    {
        return UECommandForge::GetReportsDir();
    }

    bool IsAbsoluteFilePath(const FString& Path)
    {
        return Path.StartsWith(TEXT("/")) || Path.StartsWith(TEXT("\\")) ||
            (Path.Len() > 1 && Path[1] == TEXT(':'));
    }

    bool TryResolveRollbackPlanPath(const FString& RawPath, const FString& TransactionId,
        FString& OutPath, FCommandForgeReport& Report)
    {
        const FString ReportsDir = RollbackReportDirectory();
        const FString DefaultPath = FPaths::Combine(ReportsDir,
            FString::Printf(TEXT("rollback_import_%s.json"), *TransactionId));

        FString Candidate = RawPath.TrimStartAndEnd();
        if (Candidate.IsEmpty())
        {
            OutPath = DefaultPath;
            return true;
        }

        FString ReportsPrefix = ReportsDir;
        FPaths::NormalizeFilename(ReportsPrefix);
        if (!ReportsPrefix.EndsWith(TEXT("/")))
        {
            ReportsPrefix += TEXT("/");
        }

        auto NormalizeCandidate = [](const FString& Path)
        {
            FString Normalized = FPaths::ConvertRelativePathToFull(Path);
            FPaths::NormalizeFilename(Normalized);
            FPaths::CollapseRelativeDirectories(Normalized);
            return Normalized;
        };

        Candidate = NormalizeCandidate(Candidate);
        if (!Candidate.StartsWith(ReportsPrefix, ESearchCase::IgnoreCase))
        {
            Candidate = NormalizeCandidate(FPaths::Combine(ReportsDir, RawPath.TrimStartAndEnd()));
        }

        if (!Candidate.StartsWith(ReportsPrefix, ESearchCase::IgnoreCase))
        {
            AddError(Report, TEXT("ROLLBACK_PATH_OUTSIDE_REPORT_DIR"),
                TEXT("rollback_plan은 Saved/UECommandForge/Reports 아래에만 쓸 수 있습니다."),
                TEXT("RollbackPlan"));
            return false;
        }

        OutPath = Candidate;
        return true;
    }

    bool WriteSnapshot(const FString& SnapshotPath, const FString& TargetAssetPath,
        bool bTargetExists, const FString& SourceHash)
    {
        TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
        Root->SetStringField(TEXT("target_asset_path"), TargetAssetPath);
        Root->SetStringField(TEXT("disk_path"),
            FPackageName::LongPackageNameToFilename(TargetAssetPath, FPackageName::GetAssetPackageExtension()));
        Root->SetBoolField(TEXT("target_exists"), bTargetExists);
        Root->SetStringField(TEXT("source_hash"), SourceHash);

        FString Serialized;
        const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
        FJsonSerializer::Serialize(Root, Writer);
        FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FPaths::GetPath(SnapshotPath));
        return FFileHelper::SaveStringToFile(Serialized, *SnapshotPath,
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    }

    void AddRollbackOperation(FCommandForgeRollbackPlan& RollbackPlan, const FString& Operation,
        const FString& BeforeAssetPath, const FString& AfterAssetPath, const FString& SourceHash)
    {
        FCommandForgeRollbackOperation RollbackOperation;
        RollbackOperation.PlannedOperation = Operation;
        RollbackOperation.BeforeAssetPath = BeforeAssetPath;
        RollbackOperation.AfterAssetPath = AfterAssetPath;
        RollbackOperation.BeforeFilePath = BeforeAssetPath.IsEmpty()
            ? TEXT("")
            : FPackageName::LongPackageNameToFilename(BeforeAssetPath, FPackageName::GetAssetPackageExtension());
        RollbackOperation.AfterFilePath = AfterAssetPath.IsEmpty()
            ? TEXT("")
            : FPackageName::LongPackageNameToFilename(AfterAssetPath, FPackageName::GetAssetPackageExtension());
        RollbackOperation.ValidationSummary.Add(TEXT("source_hash"), SourceHash);
        RollbackPlan.Operations.Add(RollbackOperation);
    }

    bool SaveImportedTable(UDataTable* DataTable, const FString& TargetAssetPath,
        FCommandForgeReport& Report)
    {
        UPackage* Package = DataTable->GetOutermost();
        Package->MarkPackageDirty();
        FAssetRegistryModule::AssetCreated(DataTable);

        const FString PackageFilename = FPackageName::LongPackageNameToFilename(
            TargetAssetPath, FPackageName::GetAssetPackageExtension());
        FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FPaths::GetPath(PackageFilename));

        FSavePackageArgs SaveArgs;
        const bool bSaved = UPackage::SavePackage(Package, DataTable, *PackageFilename, SaveArgs);
        if (!bSaved)
        {
            AddIssue(Report, TEXT("error"), TEXT("DATATABLE_SAVE_FAILED"),
                TEXT("DataTable package 저장에 실패했습니다."), TEXT("target_asset_path"),
                TargetAssetPath, PackageFilename);
        }
        return bSaved;
    }

    UDataTable* CreateOrLoadDataTable(const FString& TargetAssetPath, bool& bOutCreated)
    {
        bOutCreated = false;
        if (UDataTable* Existing = LoadObject<UDataTable>(nullptr, *ObjectPathOf(TargetAssetPath)))
        {
            return Existing;
        }

        UPackage* Package = CreatePackage(*TargetAssetPath);
        if (Package == nullptr)
        {
            return nullptr;
        }

        bOutCreated = true;
        return NewObject<UDataTable>(Package, FName(*FPackageName::GetLongPackageAssetName(TargetAssetPath)),
            RF_Public | RF_Standalone);
    }
}

int32 UImportDataSourceCommandlet::Main(const FString& Params)
{
    namespace ImportPrivate = UECommandForge::ImportDataSourcePrivate;

    TArray<FString> Tokens;
    TArray<FString> Switches;
    TMap<FString, FString> ParamsMap;
    UCommandlet::ParseCommandLine(*Params, Tokens, Switches, ParamsMap);

    const FString SchemaPath = ParamsMap.FindRef(TEXT("Schema"));
    const FString SourcePath = ParamsMap.FindRef(TEXT("Source"));
    const FString OutPath = ParamsMap.FindRef(TEXT("Output"));
    const FString RollbackPlanArg = ParamsMap.FindRef(TEXT("RollbackPlan"));
    const bool bDryRun = Switches.Contains(TEXT("DryRun")) ||
        ParamsMap.FindRef(TEXT("DryRun")).Equals(TEXT("true"), ESearchCase::IgnoreCase);

    FCommandForgeReport Report;
    Report.Commandlet = TEXT("ImportDataSource");
    Report.bDryRun = bDryRun;
    Report.bApplied = false;
    Report.TransactionId = FString::Printf(TEXT("tx-import-%s"),
        *FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ")));

    if (OutPath.IsEmpty())
    {
        ImportPrivate::AddError(Report, TEXT("MISSING_ARG"), TEXT("-Output 인자가 없습니다."), TEXT("Output"));
        Report.bOk = false;
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }
    if (SchemaPath.IsEmpty())
    {
        ImportPrivate::AddError(Report, TEXT("MISSING_ARG"), TEXT("-Schema 인자가 없습니다."), TEXT("Schema"));
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }
    if (SourcePath.IsEmpty())
    {
        ImportPrivate::AddError(Report, TEXT("MISSING_ARG"), TEXT("-Source 인자가 없습니다."), TEXT("Source"));
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    FString SchemaJson;
    if (!FFileHelper::LoadFileToString(SchemaJson, *SchemaPath))
    {
        ImportPrivate::AddError(Report, TEXT("DATA_SCHEMA_READ_FAILED"),
            TEXT("data schema 파일을 읽을 수 없습니다."), TEXT("Schema"));
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    FCommandForgeDataSchemaSpec Schema;
    TArray<FCommandForgeError> ParseErrors;
    if (!UECommandForge::FDataSchemaSpecParser::ParseJson(SchemaJson, Schema, ParseErrors))
    {
        Report.Errors.Append(ParseErrors);
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    FString SourceContent;
    if (!FFileHelper::LoadFileToString(SourceContent, *SourcePath))
    {
        ImportPrivate::AddError(Report, TEXT("DATA_SOURCE_READ_FAILED"),
            TEXT("data source 파일을 읽을 수 없습니다."), TEXT("Source"));
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    FString TargetAssetPath = Schema.TargetAssetPath.TrimStartAndEnd();
    if (!ImportPrivate::IsValidPackagePath(TargetAssetPath))
    {
        ImportPrivate::AddError(Report, TEXT("TARGET_ASSET_PATH_INVALID"),
            TEXT("target_asset_path는 /Game 또는 /UECommandForge 아래 long package name이어야 합니다."),
            TEXT("target_asset_path"));
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    UScriptStruct* RowStruct = nullptr;
    if (!ImportPrivate::TryResolveStruct(Schema.RowStructPath, RowStruct, Report))
    {
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    const FString SourceType = Schema.SourceType.IsEmpty()
        ? FPaths::GetExtension(SourcePath).ToLower()
        : Schema.SourceType.ToLower();
    const FString SourceHash = ImportPrivate::Sha1ForString(SourceContent);
    const int32 RowCount = ImportPrivate::CountDataRows(SourceType, SourceContent);
    const bool bTargetExists = LoadObject<UDataTable>(nullptr, *ImportPrivate::ObjectPathOf(TargetAssetPath)) != nullptr;

    Report.Validation.Add(TEXT("source"), SourcePath);
    Report.Validation.Add(TEXT("schema"), SchemaPath);
    Report.Validation.Add(TEXT("source_type"), SourceType);
    Report.Validation.Add(TEXT("source_hash"), SourceHash);
    Report.Validation.Add(TEXT("target_asset_path"), TargetAssetPath);
    Report.Validation.Add(TEXT("row_struct_path"), Schema.RowStructPath);
    Report.Validation.Add(TEXT("row_count"), FString::FromInt(RowCount));
    Report.Validation.Add(TEXT("diff"), bTargetExists ? TEXT("update_datatable") : TEXT("create_datatable"));

    if (!ImportPrivate::TryResolveRollbackPlanPath(RollbackPlanArg, Report.TransactionId,
        Report.RollbackPlanPath, Report))
    {
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    if (bDryRun)
    {
        Report.bOk = Report.Errors.IsEmpty() && !ImportPrivate::HasErrorIssue(Report.ValidationIssues);
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return Report.bOk ? 0 : static_cast<int32>(ECommandForgeExitCode::ValidationFailed);
    }

    const FString SnapshotPath = FPaths::Combine(ImportPrivate::RollbackReportDirectory(),
        FString::Printf(TEXT("snapshot_%s.json"), *Report.TransactionId));
    if (!ImportPrivate::WriteSnapshot(SnapshotPath, TargetAssetPath, bTargetExists, SourceHash))
    {
        ImportPrivate::AddError(Report, TEXT("SNAPSHOT_WRITE_FAILED"),
            TEXT("import 전 snapshot을 쓸 수 없습니다."), TEXT("snapshot"));
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::EngineError);
    }
    Report.Validation.Add(TEXT("snapshot_path"), SnapshotPath);

    bool bCreated = false;
    UDataTable* DataTable = ImportPrivate::CreateOrLoadDataTable(TargetAssetPath, bCreated);
    if (DataTable == nullptr)
    {
        ImportPrivate::AddError(Report, TEXT("DATATABLE_CREATE_FAILED"),
            TEXT("DataTable asset을 생성하거나 로드할 수 없습니다."), TEXT("target_asset_path"));
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::EngineError);
    }

    DataTable->RowStruct = RowStruct;
    DataTable->ImportKeyField = Schema.KeyField;
    TArray<FString> ImportProblems = SourceType == TEXT("json")
        ? DataTable->CreateTableFromJSONString(SourceContent)
        : DataTable->CreateTableFromCSVString(SourceContent);
    for (const FString& Problem : ImportProblems)
    {
        ImportPrivate::AddIssue(Report, TEXT("error"), TEXT("DATATABLE_IMPORT_PROBLEM"),
            Problem, TEXT("Source"), TargetAssetPath, TEXT("source와 row struct 필드를 확인하세요."));
    }

    if (!ImportPrivate::HasErrorIssue(Report.ValidationIssues) &&
        ImportPrivate::SaveImportedTable(DataTable, TargetAssetPath, Report))
    {
        FAssetRegistryModule& AssetRegistryModule =
            FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
        AssetRegistryModule.Get().ScanPathsSynchronous({ ImportPrivate::PackagePathOf(TargetAssetPath) }, true);

        Report.ChangedAssets.Add(TargetAssetPath);
        Report.ChangedFiles.Add(FPackageName::LongPackageNameToFilename(
            TargetAssetPath, FPackageName::GetAssetPackageExtension()));
        Report.bApplied = true;
        Report.PostValidation.Add(TEXT("datatable_asset"), TEXT("exists"));
        Report.PostValidation.Add(TEXT("row_count"),
            FString::FromInt(DataTable->GetRowMap().Num()));
    }

    Report.Validation.Add(TEXT("issue_count"), FString::FromInt(Report.ValidationIssues.Num()));
    Report.bOk = Report.Errors.IsEmpty() && !ImportPrivate::HasErrorIssue(Report.ValidationIssues) &&
        Report.bApplied;

    if (Report.bApplied)
    {
        FCommandForgeRollbackPlan RollbackPlan;
        RollbackPlan.TransactionId = Report.TransactionId;
        RollbackPlan.Commandlet = TEXT("ImportDataSource");
        RollbackPlan.Timestamp = FDateTime::UtcNow().ToIso8601();
        ImportPrivate::AddRollbackOperation(RollbackPlan,
            bCreated ? TEXT("delete_created_datatable") : TEXT("restore_datatable_from_backup"),
            bCreated ? TEXT("") : TargetAssetPath, TargetAssetPath, SourceHash);

        if (!UECommandForge::FRollbackPlanWriter::Write(Report.RollbackPlanPath, RollbackPlan))
        {
            ImportPrivate::AddError(Report, TEXT("ROLLBACK_WRITE_FAILED"),
                TEXT("rollback plan을 쓸 수 없습니다."), TEXT("RollbackPlan"));
            Report.bOk = false;
        }
        else
        {
            Report.bRollbackAvailable = true;
        }
    }

    const bool bWrote = UECommandForge::FJsonReportWriter::Write(OutPath, Report);
    if (!bWrote)
    {
        return static_cast<int32>(ECommandForgeExitCode::EngineError);
    }
    return Report.bOk ? 0 : static_cast<int32>(ECommandForgeExitCode::ValidationFailed);
}
