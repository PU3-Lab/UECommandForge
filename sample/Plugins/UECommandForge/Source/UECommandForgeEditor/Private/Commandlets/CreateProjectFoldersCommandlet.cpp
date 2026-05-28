#include "Commandlets/CreateProjectFoldersCommandlet.h"
#include "CommandForgeTypes.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Reports/JsonReportWriter.h"
#include "Reports/RollbackPlanWriter.h"
#include "Specs/CommandForgePolicyParser.h"

UCreateProjectFoldersCommandlet::UCreateProjectFoldersCommandlet()
{
    IsClient     = false;
    IsServer     = false;
    IsEditor     = true;
    LogToConsole = true;
}

namespace UECommandForge::CreateProjectFoldersPrivate
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

    void AddIssue(FCommandForgeReport& Report, const FString& Code, const FString& Message,
        const FString& Field, const FString& FolderPath, const FString& SuggestedFix)
    {
        FCommandForgeValidationIssue Issue;
        Issue.Severity = TEXT("error");
        Issue.Code = Code;
        Issue.Message = Message;
        Issue.Field = Field;
        Issue.AssetPath = FolderPath;
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

    FString NormalizePackagePath(const FString& RawPath)
    {
        return RawPath.TrimStartAndEnd().Replace(TEXT("\\"), TEXT("/"));
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

    bool IsValidFolderPath(const FString& Path)
    {
        return IsAllowedMountPath(Path) && !ContainsWildcard(Path) &&
            !Path.Contains(TEXT("..")) &&
            FPackageName::IsValidLongPackageName(Path, false) &&
            Path != TEXT("/Game") && Path != TEXT("/UECommandForge");
    }

    bool DirectoryExistsForPackage(const FString& PackagePath)
    {
        return FPaths::DirectoryExists(FPackageName::LongPackageNameToFilename(PackagePath));
    }

    FString RollbackReportDirectory()
    {
        return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CodexReports"));
    }

    FString RollbackPlanPathForTransaction(const FString& TransactionId)
    {
        return FPaths::Combine(RollbackReportDirectory(),
            FString::Printf(TEXT("rollback_%s.json"), *TransactionId));
    }

    TArray<FString> ExpandedFolderHierarchy(const TArray<FString>& RequestedFolders)
    {
        TArray<FString> Expanded;
        TSet<FString> Seen;

        for (const FString& RequestedFolder : RequestedFolders)
        {
            TArray<FString> Parts;
            RequestedFolder.ParseIntoArray(Parts, TEXT("/"), true);
            if (Parts.Num() < 2)
            {
                continue;
            }

            FString Current = FString::Printf(TEXT("/%s"), *Parts[0]);
            for (int32 Index = 1; Index < Parts.Num(); ++Index)
            {
                Current /= Parts[Index];
                if (!Seen.Contains(Current))
                {
                    Seen.Add(Current);
                    Expanded.Add(Current);
                }
            }
        }
        return Expanded;
    }

    bool ParseProjectFoldersSpec(const FCommandForgePolicyDocument& Document,
        FString& OutTransactionId, TArray<FString>& OutFolders, FCommandForgeReport& Report)
    {
        if (!Document.Kind.Equals(TEXT("project_folders"), ESearchCase::IgnoreCase))
        {
            AddError(Report, TEXT("INVALID_KIND"),
                TEXT("project_folders spec만 사용할 수 있습니다."), TEXT("kind"));
            return false;
        }

        Document.RootObject->TryGetStringField(TEXT("transaction_id"), OutTransactionId);
        OutTransactionId = OutTransactionId.TrimStartAndEnd();

        const TArray<TSharedPtr<FJsonValue>>* FolderValues = nullptr;
        if (!Document.RootObject->TryGetArrayField(TEXT("folders"), FolderValues) ||
            FolderValues == nullptr)
        {
            AddError(Report, TEXT("REQUIRED_FIELD"),
                TEXT("'folders' 배열은 필수입니다."), TEXT("folders"));
            return false;
        }

        for (int32 Index = 0; Index < FolderValues->Num(); ++Index)
        {
            FString FolderPath;
            if (!(*FolderValues)[Index]->TryGetString(FolderPath))
            {
                AddIssue(Report, TEXT("INVALID_FOLDER_ENTRY"),
                    TEXT("folders 항목은 string long package path여야 합니다."),
                    FString::Printf(TEXT("folders[%d]"), Index), TEXT(""),
                    TEXT("예: /Game/Characters/Hero/Blueprints"));
                continue;
            }

            OutFolders.Add(NormalizePackagePath(FolderPath));
        }
        return Report.Errors.IsEmpty();
    }
}

int32 UCreateProjectFoldersCommandlet::Main(const FString& Params)
{
    using namespace UECommandForge::CreateProjectFoldersPrivate;

    TArray<FString> Tokens;
    TArray<FString> Switches;
    TMap<FString, FString> ParamsMap;
    UCommandlet::ParseCommandLine(*Params, Tokens, Switches, ParamsMap);

    const FString OutPath = ParamsMap.FindRef(TEXT("Output"));
    const FString SpecPath = ParamsMap.FindRef(TEXT("Spec"));
    const bool bApply = Switches.Contains(TEXT("Apply"));

    FCommandForgeReport Report;
    Report.Commandlet = TEXT("CreateProjectFolders");
    Report.bDryRun = !bApply;
    Report.bApplied = false;

    if (OutPath.IsEmpty())
    {
        AddError(Report, TEXT("MISSING_ARG"), TEXT("-Output 인자가 없습니다."), TEXT("Output"));
        Report.bOk = false;
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    if (SpecPath.IsEmpty())
    {
        AddError(Report, TEXT("MISSING_ARG"), TEXT("-Spec 인자가 없습니다."), TEXT("Spec"));
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    FString SpecJson;
    if (!FFileHelper::LoadFileToString(SpecJson, *SpecPath))
    {
        AddError(Report, TEXT("SPEC_READ_FAILED"), TEXT("folder spec 파일을 읽을 수 없습니다."), TEXT("Spec"));
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    FCommandForgePolicyDocument Document;
    TArray<FCommandForgeError> ParseErrors;
    if (!UECommandForge::FCommandForgePolicyParser::ParseJson(SpecJson, Document, ParseErrors))
    {
        Report.Errors.Append(ParseErrors);
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    FString TransactionId;
    TArray<FString> RequestedFolders;
    if (!ParseProjectFoldersSpec(Document, TransactionId, RequestedFolders, Report))
    {
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    TransactionId = TransactionId.IsEmpty()
        ? FString::Printf(TEXT("tx-%s"), *FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ")))
        : TransactionId;
    Report.TransactionId = TransactionId;
    Report.RollbackPlanPath = RollbackPlanPathForTransaction(TransactionId);

    for (const FString& Folder : RequestedFolders)
    {
        if (!IsValidFolderPath(Folder))
        {
            AddIssue(Report, TEXT("INVALID_FOLDER_PATH"),
                TEXT("folder path가 유효하지 않거나 허용되지 않은 mount입니다."),
                TEXT("folders"), Folder,
                TEXT("/Game 또는 /UECommandForge 아래의 wildcard 없는 long package path를 사용하세요."));
        }
    }

    const TArray<FString> ExpandedFolders = ExpandedFolderHierarchy(RequestedFolders);
    int32 WouldCreateCount = 0;
    for (const FString& Folder : ExpandedFolders)
    {
        if (IsValidFolderPath(Folder) && !DirectoryExistsForPackage(Folder))
        {
            ++WouldCreateCount;
        }
    }

    Report.Validation.Add(TEXT("spec"), SpecPath);
    Report.Validation.Add(TEXT("folder_count"), FString::FromInt(RequestedFolders.Num()));
    Report.Validation.Add(TEXT("expanded_folder_count"), FString::FromInt(ExpandedFolders.Num()));
    Report.Validation.Add(TEXT("would_create_count"), FString::FromInt(WouldCreateCount));

    if (HasErrorIssue(Report.ValidationIssues))
    {
        Report.Validation.Add(TEXT("issue_count"), FString::FromInt(Report.ValidationIssues.Num()));
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::ValidationFailed);
    }

    FCommandForgeRollbackPlan RollbackPlan;
    RollbackPlan.TransactionId = TransactionId;
    RollbackPlan.Commandlet = TEXT("CreateProjectFolders");
    RollbackPlan.Timestamp = FDateTime::UtcNow().ToIso8601();

    int32 CreatedFolderCount = 0;
    if (bApply)
    {
        IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
        for (const FString& Folder : ExpandedFolders)
        {
            const FString DiskPath = FPackageName::LongPackageNameToFilename(Folder);
            if (FPaths::DirectoryExists(DiskPath))
            {
                continue;
            }

            if (!PlatformFile.CreateDirectoryTree(*DiskPath))
            {
                AddIssue(Report, TEXT("FOLDER_CREATE_FAILED"),
                    TEXT("folder 생성에 실패했습니다."), TEXT("folders"), Folder,
                    TEXT("디스크 권한과 Content 경로를 확인하세요."));
                continue;
            }

            FCommandForgeRollbackOperation RollbackOperation;
            RollbackOperation.PlannedOperation = TEXT("delete_folder");
            RollbackOperation.AfterAssetPath = Folder;
            RollbackOperation.AfterFilePath = DiskPath;
            RollbackPlan.Operations.Add(RollbackOperation);
            Report.ChangedFiles.Add(DiskPath);
            ++CreatedFolderCount;
        }
    }

    bool bPostValidationOk = true;
    if (bApply)
    {
        for (const FString& Folder : RequestedFolders)
        {
            if (!DirectoryExistsForPackage(Folder))
            {
                AddIssue(Report, TEXT("POST_FOLDER_MISSING"),
                    TEXT("folder 생성 후 요청 폴더가 존재하지 않습니다."),
                    TEXT("folders"), Folder, TEXT("수동으로 Content 폴더를 확인하세요."));
                bPostValidationOk = false;
            }
        }
        Report.PostValidation.Add(TEXT("status"), bPostValidationOk ? TEXT("passed") : TEXT("failed"));
    }
    else
    {
        Report.PostValidation.Add(TEXT("status"), TEXT("skipped"));
    }

    Report.Validation.Add(TEXT("created_folder_count"), FString::FromInt(CreatedFolderCount));
    Report.Validation.Add(TEXT("issue_count"), FString::FromInt(Report.ValidationIssues.Num()));
    Report.bApplied = bApply && CreatedFolderCount > 0;
    Report.bOk = Report.Errors.IsEmpty() && !HasErrorIssue(Report.ValidationIssues) && bPostValidationOk;

    if (Report.bApplied && !RollbackPlan.Operations.IsEmpty())
    {
        if (!UECommandForge::FRollbackPlanWriter::Write(Report.RollbackPlanPath, RollbackPlan))
        {
            AddError(Report, TEXT("ROLLBACK_WRITE_FAILED"),
                TEXT("rollback plan을 쓸 수 없습니다."), TEXT("rollback_plan"));
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
