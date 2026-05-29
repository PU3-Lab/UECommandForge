#include "Commandlets/RollbackAssetChangesCommandlet.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetToolsModule.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "ObjectTools.h"
#include "Reports/JsonReportWriter.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/ObjectRedirector.h"

URollbackAssetChangesCommandlet::URollbackAssetChangesCommandlet()
{
    IsClient     = false;
    IsServer     = false;
    IsEditor     = true;
    LogToConsole = true;
}

namespace UECommandForge::RollbackAssetChangesPrivate
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

    FString NormalizePackageName(const FString& RawPath)
    {
        FString Path = RawPath.TrimStartAndEnd();
        FString PackageName;
        FString ObjectName;
        if (Path.Split(TEXT("."), &PackageName, &ObjectName))
        {
            Path = PackageName;
        }
        return Path;
    }

    bool ContainsWildcard(const FString& Path)
    {
        return Path.Contains(TEXT("*")) || Path.Contains(TEXT("?"));
    }

    bool IsAllowedMountPath(const FString& Path)
    {
        return Path == TEXT("/Game") || Path.StartsWith(TEXT("/Game/")) ||
            Path == TEXT("/UECommandForge") || Path.StartsWith(TEXT("/UECommandForge/"));
    }

    bool IsValidPackagePath(const FString& Path)
    {
        return IsAllowedMountPath(Path) && FPackageName::IsValidLongPackageName(Path, false);
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

    FString ObjectPathOf(const FString& PackageName)
    {
        return FString::Printf(TEXT("%s.%s"), *PackageName,
            *FPackageName::GetLongPackageAssetName(PackageName));
    }

    bool AssetExists(IAssetRegistry& AssetRegistry, const FString& PackageName)
    {
        TArray<FAssetData> Assets;
        AssetRegistry.GetAssetsByPackageName(FName(*PackageName), Assets);
        return !Assets.IsEmpty();
    }

    bool DeleteAssetPackage(const FString& PackageName, FString& OutError)
    {
        const FString ObjectPath = ObjectPathOf(PackageName);
        if (UObject* Existing = LoadObject<UObject>(nullptr, *ObjectPath))
        {
            TArray<UObject*> ObjectsToDelete;
            ObjectsToDelete.Add(Existing);
            const int32 DeletedCount = ObjectTools::ForceDeleteObjects(ObjectsToDelete, false);
            if (DeletedCount <= 0)
            {
                OutError = TEXT("asset 삭제에 실패했습니다.");
                return false;
            }
        }

        const FString PackageFilename = FPackageName::LongPackageNameToFilename(
            PackageName, FPackageName::GetAssetPackageExtension());
        if (FPaths::FileExists(PackageFilename) &&
            !IFileManager::Get().Delete(*PackageFilename))
        {
            OutError = FString::Printf(TEXT("package file 삭제에 실패했습니다: %s"), *PackageFilename);
            return false;
        }

        return true;
    }

    bool TryReadStringArray(const TSharedPtr<FJsonObject>& Object, const FString& Field,
        TArray<FString>& OutValues)
    {
        const TArray<TSharedPtr<FJsonValue>>* ArrayValues = nullptr;
        if (!Object->TryGetArrayField(Field, ArrayValues))
        {
            return false;
        }

        for (const TSharedPtr<FJsonValue>& Value : *ArrayValues)
        {
            FString StringValue;
            if (Value->TryGetString(StringValue))
            {
                OutValues.Add(StringValue);
            }
        }
        return true;
    }

    void ReadStringMap(const TSharedPtr<FJsonObject>& Object, const FString& Field,
        TMap<FString, FString>& OutValues)
    {
        const TSharedPtr<FJsonObject>* MapObject = nullptr;
        if (!Object->TryGetObjectField(Field, MapObject) || MapObject == nullptr ||
            !MapObject->IsValid())
        {
            return;
        }

        for (const auto& Pair : (*MapObject)->Values)
        {
            FString StringValue;
            if (Pair.Value.IsValid() && Pair.Value->TryGetString(StringValue))
            {
                OutValues.Add(Pair.Key, StringValue);
            }
        }
    }

    bool ParseRollbackPlan(const FString& Json, FCommandForgeRollbackPlan& OutPlan,
        TArray<FCommandForgeError>& OutErrors)
    {
        TSharedPtr<FJsonObject> RootObject;
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
        if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
        {
            OutErrors.Add({ TEXT("ROLLBACK_PARSE_FAILED"),
                TEXT("rollback plan JSON을 파싱할 수 없습니다."), TEXT("RollbackPlan") });
            return false;
        }

        RootObject->TryGetStringField(TEXT("transaction_id"), OutPlan.TransactionId);
        RootObject->TryGetStringField(TEXT("commandlet"), OutPlan.Commandlet);
        RootObject->TryGetStringField(TEXT("timestamp"), OutPlan.Timestamp);

        const TArray<TSharedPtr<FJsonValue>>* OperationValues = nullptr;
        if (!RootObject->TryGetArrayField(TEXT("operations"), OperationValues) ||
            OperationValues == nullptr)
        {
            OutErrors.Add({ TEXT("REQUIRED_FIELD"), TEXT("'operations' 배열은 필수입니다."),
                TEXT("operations") });
            return false;
        }

        for (int32 Index = 0; Index < OperationValues->Num(); ++Index)
        {
            const TSharedPtr<FJsonObject>* OperationObject = nullptr;
            if (!(*OperationValues)[Index]->TryGetObject(OperationObject) ||
                OperationObject == nullptr || !OperationObject->IsValid())
            {
                OutErrors.Add({ TEXT("INVALID_OPERATION"),
                    TEXT("operations 항목은 object여야 합니다."),
                    FString::Printf(TEXT("operations[%d]"), Index) });
                continue;
            }

            FCommandForgeRollbackOperation Operation;
            (*OperationObject)->TryGetStringField(TEXT("planned_operation"), Operation.PlannedOperation);
            (*OperationObject)->TryGetStringField(TEXT("before_asset_path"), Operation.BeforeAssetPath);
            (*OperationObject)->TryGetStringField(TEXT("before_file_path"), Operation.BeforeFilePath);
            (*OperationObject)->TryGetStringField(TEXT("after_asset_path"), Operation.AfterAssetPath);
            (*OperationObject)->TryGetStringField(TEXT("after_file_path"), Operation.AfterFilePath);
            TryReadStringArray(*OperationObject, TEXT("dependency_snapshot"), Operation.DependencySnapshot);
            ReadStringMap(*OperationObject, TEXT("validation_summary"), Operation.ValidationSummary);

            if (Operation.PlannedOperation.IsEmpty())
            {
                OutErrors.Add({ TEXT("REQUIRED_FIELD"),
                    TEXT("'planned_operation' 필드는 필수입니다."),
                    FString::Printf(TEXT("operations[%d].planned_operation"), Index) });
            }
            OutPlan.Operations.Add(Operation);
        }
        return OutErrors.IsEmpty();
    }

    bool ApplyRenameRollback(const FString& CurrentPath, const FString& RestorePath,
        FString& OutError)
    {
        UObject* Asset = LoadObject<UObject>(nullptr, *ObjectPathOf(CurrentPath));
        if (Asset == nullptr)
        {
            OutError = TEXT("rollback 대상 asset을 로드할 수 없습니다.");
            return false;
        }

        IAssetTools& AssetTools =
            FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
        TArray<FAssetRenameData> RenameData;
        RenameData.Emplace(Asset, PackagePathOf(RestorePath),
            FPackageName::GetLongPackageAssetName(RestorePath));
        if (!AssetTools.RenameAssets(RenameData))
        {
            OutError = TEXT("AssetTools RenameAssets rollback이 실패했습니다.");
            return false;
        }
        return true;
    }

    int32 ApplyRedirectorFixup(IAssetRegistry& AssetRegistry, const FString& Path)
    {
        TArray<FAssetData> Assets;
        AssetRegistry.GetAssetsByPath(FName(*Path), Assets, true);

        TArray<UObjectRedirector*> Redirectors;
        for (const FAssetData& AssetData : Assets)
        {
            if (AssetData.AssetClassPath.GetAssetName() != FName(TEXT("ObjectRedirector")))
            {
                continue;
            }

            if (UObjectRedirector* Redirector = Cast<UObjectRedirector>(AssetData.GetAsset()))
            {
                Redirectors.Add(Redirector);
            }
        }

        if (!Redirectors.IsEmpty())
        {
            IAssetTools& AssetTools =
                FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
            AssetTools.FixupReferencers(Redirectors, false);
        }
        return Redirectors.Num();
    }

    bool IsDirectoryEmpty(const FString& DiskPath)
    {
        TArray<FString> Entries;
        IFileManager::Get().FindFiles(Entries, *(DiskPath / TEXT("*")), true, true);
        return Entries.IsEmpty();
    }

    bool RunPostValidation(const TArray<FCommandForgeRollbackOperation>& Operations,
        IAssetRegistry& AssetRegistry, FCommandForgeReport& Report)
    {
        bool bPostValidationOk = true;
        for (const FCommandForgeRollbackOperation& Operation : Operations)
        {
            const FString OperationName = Operation.PlannedOperation;
            if (OperationName == TEXT("rename_asset") || OperationName == TEXT("move_asset"))
            {
                const FString RestorePath = NormalizePackageName(Operation.BeforeAssetPath);
                if (IsValidPackagePath(RestorePath) && !AssetExists(AssetRegistry, RestorePath))
                {
                    AddIssue(Report, TEXT("error"), TEXT("POST_RESTORE_MISSING"),
                        TEXT("rollback 후 복구 asset을 AssetRegistry에서 찾을 수 없습니다."),
                        TEXT("before_asset_path"), RestorePath,
                        TEXT("manual recovery: rollback plan과 Content 디렉터리를 확인하세요."));
                    bPostValidationOk = false;
                }
                continue;
            }

            if (OperationName == TEXT("delete_folder"))
            {
                const FString FolderPath = NormalizePackageName(Operation.AfterAssetPath);
                if (IsValidPackagePath(FolderPath) &&
                    FPaths::DirectoryExists(FPackageName::LongPackageNameToFilename(FolderPath)))
                {
                    AddIssue(Report, TEXT("error"), TEXT("POST_FOLDER_STILL_EXISTS"),
                        TEXT("rollback 후 삭제 대상 폴더가 아직 존재합니다."),
                        TEXT("after_asset_path"), FolderPath,
                        TEXT("manual recovery: 폴더 내부 파일을 확인하고 직접 삭제하세요."));
                    bPostValidationOk = false;
                }
            }

            if (OperationName == TEXT("delete_created_datatable"))
            {
                const FString PackageName = NormalizePackageName(Operation.AfterAssetPath);
                if (IsValidPackagePath(PackageName) && AssetExists(AssetRegistry, PackageName))
                {
                    AddIssue(Report, TEXT("error"), TEXT("POST_CREATED_DATATABLE_STILL_EXISTS"),
                        TEXT("rollback 후 생성된 DataTable asset이 아직 존재합니다."),
                        TEXT("after_asset_path"), PackageName,
                        TEXT("manual recovery: 생성된 DataTable asset을 확인하고 직접 삭제하세요."));
                    bPostValidationOk = false;
                }
            }
        }

        Report.PostValidation.Add(TEXT("status"), bPostValidationOk ? TEXT("passed") : TEXT("failed"));
        return bPostValidationOk;
    }
}

int32 URollbackAssetChangesCommandlet::Main(const FString& Params)
{
    using namespace UECommandForge::RollbackAssetChangesPrivate;

    TArray<FString> Tokens;
    TArray<FString> Switches;
    TMap<FString, FString> ParamsMap;
    UCommandlet::ParseCommandLine(*Params, Tokens, Switches, ParamsMap);

    const FString OutPath = ParamsMap.FindRef(TEXT("Output"));
    const FString RollbackPath = ParamsMap.FindRef(TEXT("RollbackPlan"));

    FCommandForgeReport Report;
    Report.Commandlet = TEXT("RollbackAssetChanges");
    Report.bDryRun = false;
    Report.bApplied = false;

    if (OutPath.IsEmpty())
    {
        AddError(Report, TEXT("MISSING_ARG"), TEXT("-Output 인자가 없습니다."), TEXT("Output"));
        Report.bOk = false;
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    if (RollbackPath.IsEmpty())
    {
        AddError(Report, TEXT("MISSING_ARG"),
            TEXT("-RollbackPlan 인자가 없습니다."), TEXT("RollbackPlan"));
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    FString RollbackJson;
    if (!FFileHelper::LoadFileToString(RollbackJson, *RollbackPath))
    {
        AddError(Report, TEXT("ROLLBACK_READ_FAILED"),
            TEXT("rollback plan 파일을 읽을 수 없습니다."), TEXT("RollbackPlan"));
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    FCommandForgeRollbackPlan Plan;
    TArray<FCommandForgeError> ParseErrors;
    if (!ParseRollbackPlan(RollbackJson, Plan, ParseErrors))
    {
        Report.Errors.Append(ParseErrors);
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    Report.TransactionId = Plan.TransactionId;
    Report.RollbackPlanPath = RollbackPath;

    FAssetRegistryModule& AssetRegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

    TArray<FString> PathsToScan;
    TArray<FCommandForgeRollbackOperation> AppliedOperations;
    int32 AppliedRollbackCount = 0;

    for (int32 Index = Plan.Operations.Num() - 1; Index >= 0; --Index)
    {
        const FCommandForgeRollbackOperation& Operation = Plan.Operations[Index];
        const FString OperationName = Operation.PlannedOperation;

        if (OperationName == TEXT("rename_asset") || OperationName == TEXT("move_asset"))
        {
            const FString RestorePath = NormalizePackageName(Operation.BeforeAssetPath);
            const FString CurrentPath = NormalizePackageName(Operation.AfterAssetPath);
            if (!IsValidPackagePath(RestorePath) || ContainsWildcard(RestorePath) ||
                !IsValidPackagePath(CurrentPath) || ContainsWildcard(CurrentPath))
            {
                AddIssue(Report, TEXT("error"), TEXT("INVALID_ROLLBACK_PATH"),
                    TEXT("rollback asset path가 유효하지 않습니다."),
                    TEXT("before_asset_path"), RestorePath,
                    TEXT("manual recovery: rollback plan의 asset path를 확인하세요."));
                continue;
            }
            if (!AssetExists(AssetRegistry, CurrentPath))
            {
                AddIssue(Report, TEXT("error"), TEXT("ROLLBACK_SOURCE_MISSING"),
                    TEXT("rollback할 현재 asset을 찾을 수 없습니다."),
                    TEXT("after_asset_path"), CurrentPath,
                    TEXT("manual recovery: 현재 asset 위치를 확인하세요."));
                continue;
            }
            if (AssetExists(AssetRegistry, RestorePath))
            {
                AddIssue(Report, TEXT("error"), TEXT("PATH_COLLISION"),
                    TEXT("복구 대상 경로에 이미 asset이 존재합니다."),
                    TEXT("before_asset_path"), RestorePath,
                    TEXT("manual recovery: 충돌 asset을 백업하거나 target 경로를 조정하세요."));
                continue;
            }

            FString ApplyError;
            if (!ApplyRenameRollback(CurrentPath, RestorePath, ApplyError))
            {
                AddIssue(Report, TEXT("error"), TEXT("ROLLBACK_RENAME_FAILED"),
                    ApplyError, TEXT("after_asset_path"), CurrentPath,
                    TEXT("manual recovery: AssetTools 로그와 rollback plan을 확인하세요."));
                continue;
            }

            Report.ChangedAssets.Add(CurrentPath);
            Report.ChangedAssets.Add(RestorePath);
            PathsToScan.Add(PackagePathOf(CurrentPath));
            PathsToScan.Add(PackagePathOf(RestorePath));
            AppliedOperations.Add(Operation);
            ++AppliedRollbackCount;
            continue;
        }

        if (OperationName == TEXT("delete_folder"))
        {
            const FString FolderPath = NormalizePackageName(Operation.AfterAssetPath);
            if (!IsValidPackagePath(FolderPath) || ContainsWildcard(FolderPath))
            {
                AddIssue(Report, TEXT("error"), TEXT("INVALID_ROLLBACK_FOLDER_PATH"),
                    TEXT("rollback folder path가 유효하지 않습니다."),
                    TEXT("after_asset_path"), FolderPath,
                    TEXT("manual recovery: rollback plan의 folder path를 확인하세요."));
                continue;
            }

            const FString FolderDiskPath = FPackageName::LongPackageNameToFilename(FolderPath);
            if (FPaths::DirectoryExists(FolderDiskPath) && !IsDirectoryEmpty(FolderDiskPath))
            {
                AddIssue(Report, TEXT("error"), TEXT("ROLLBACK_FOLDER_NOT_EMPTY"),
                    TEXT("rollback folder가 비어 있지 않아 자동 삭제를 중단했습니다."),
                    TEXT("after_asset_path"), FolderPath,
                    TEXT("manual recovery: 폴더 내부 asset을 확인한 뒤 필요한 항목만 직접 정리하세요."));
                continue;
            }
            if (FPaths::DirectoryExists(FolderDiskPath) &&
                !FPlatformFileManager::Get().GetPlatformFile().DeleteDirectoryRecursively(*FolderDiskPath))
            {
                AddIssue(Report, TEXT("error"), TEXT("ROLLBACK_FOLDER_DELETE_FAILED"),
                    TEXT("rollback folder 삭제에 실패했습니다."),
                    TEXT("after_asset_path"), FolderPath,
                    TEXT("manual recovery: 폴더 내부 파일을 확인하고 직접 삭제하세요."));
                continue;
            }

            Report.ChangedFiles.Add(FolderDiskPath);
            PathsToScan.Add(FolderPath);
            AppliedOperations.Add(Operation);
            ++AppliedRollbackCount;
            continue;
        }

        if (OperationName == TEXT("delete_created_datatable"))
        {
            const FString TargetPath = NormalizePackageName(Operation.AfterAssetPath);
            if (!IsValidPackagePath(TargetPath) || ContainsWildcard(TargetPath))
            {
                AddIssue(Report, TEXT("error"), TEXT("INVALID_ROLLBACK_PATH"),
                    TEXT("rollback DataTable path가 유효하지 않습니다."),
                    TEXT("after_asset_path"), TargetPath,
                    TEXT("manual recovery: rollback plan의 DataTable path를 확인하세요."));
                continue;
            }

            FString ApplyError;
            if (!DeleteAssetPackage(TargetPath, ApplyError))
            {
                AddIssue(Report, TEXT("error"), TEXT("ROLLBACK_DELETE_DATATABLE_FAILED"),
                    ApplyError, TEXT("after_asset_path"), TargetPath,
                    TEXT("manual recovery: 생성된 DataTable asset을 직접 삭제하세요."));
                continue;
            }

            Report.ChangedAssets.Add(TargetPath);
            Report.ChangedFiles.Add(FPackageName::LongPackageNameToFilename(
                TargetPath, FPackageName::GetAssetPackageExtension()));
            PathsToScan.Add(PackagePathOf(TargetPath));
            AppliedOperations.Add(Operation);
            ++AppliedRollbackCount;
            continue;
        }

        if (OperationName == TEXT("fix_redirector"))
        {
            const FString Path = NormalizePackageName(Operation.BeforeAssetPath.IsEmpty()
                ? Operation.AfterAssetPath : Operation.BeforeAssetPath);
            if (!IsValidPackagePath(Path) || ContainsWildcard(Path))
            {
                AddIssue(Report, TEXT("error"), TEXT("INVALID_REDIRECTOR_PATH"),
                    TEXT("redirector fixup path가 유효하지 않습니다."),
                    TEXT("path"), Path,
                    TEXT("manual recovery: redirector fixup 경로를 확인하세요."));
                continue;
            }

            const int32 RedirectorCount = ApplyRedirectorFixup(AssetRegistry, Path);
            Report.PostValidation.Add(TEXT("redirectors_fixed"),
                FString::FromInt(RedirectorCount));
            PathsToScan.Add(Path);
            AppliedOperations.Add(Operation);
            ++AppliedRollbackCount;
            continue;
        }

        if (OperationName == TEXT("restore_deleted_asset"))
        {
            AddIssue(Report, TEXT("error"), TEXT("MANUAL_RESTORE_REQUIRED"),
                TEXT("삭제된 asset 복원은 자동 rollback 범위를 벗어납니다."),
                TEXT("before_asset_path"), Operation.BeforeAssetPath,
                TEXT("manual recovery: source control 또는 백업에서 asset 파일을 복구한 뒤 AssetRegistry를 재스캔하세요."));
            continue;
        }

        AddIssue(Report, TEXT("error"), TEXT("UNKNOWN_ROLLBACK_OPERATION"),
            TEXT("지원하지 않는 rollback operation입니다."), TEXT("planned_operation"),
            OperationName, TEXT("manual recovery: rollback plan을 확인하세요."));
    }

    if (!PathsToScan.IsEmpty())
    {
        AssetRegistry.ScanPathsSynchronous(PathsToScan, true);
    }
    RunPostValidation(AppliedOperations, AssetRegistry, Report);

    Report.Validation.Add(TEXT("operation_count"), FString::FromInt(Plan.Operations.Num()));
    Report.Validation.Add(TEXT("applied_rollback_count"), FString::FromInt(AppliedRollbackCount));
    Report.Validation.Add(TEXT("issue_count"), FString::FromInt(Report.ValidationIssues.Num()));
    Report.Validation.Add(TEXT("rollback_plan"), RollbackPath);
    Report.PostValidation.Add(TEXT("asset_registry_rescan"),
        PathsToScan.IsEmpty() ? TEXT("skipped") : TEXT("passed"));
    Report.bApplied = AppliedRollbackCount > 0;
    Report.bOk = Report.Errors.IsEmpty() && !HasErrorIssue(Report.ValidationIssues);

    const bool bWrote = UECommandForge::FJsonReportWriter::Write(OutPath, Report);
    if (!bWrote)
    {
        return static_cast<int32>(ECommandForgeExitCode::EngineError);
    }
    return Report.bOk ? 0 : static_cast<int32>(ECommandForgeExitCode::ValidationFailed);
}
