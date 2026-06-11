#include "Commandlets/PlanAssetChangesCommandlet.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Reports/JsonReportWriter.h"
#include "Reports/ReportPaths.h"
#include "Reports/RollbackPlanWriter.h"
#include "Specs/AssetChangePlanSpec.h"
#include "Specs/CommandForgePolicyParser.h"

UPlanAssetChangesCommandlet::UPlanAssetChangesCommandlet()
{
    IsClient     = false;
    IsServer     = false;
    IsEditor     = true;
    LogToConsole = true;
}

namespace UECommandForge::PlanAssetChangesPrivate
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
        return Path.StartsWith(TEXT("/Game")) || Path.StartsWith(TEXT("/UECommandForge"));
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

    TArray<FString> NamesToStrings(const TArray<FName>& Names)
    {
        TArray<FString> Out;
        for (const FName& Name : Names)
        {
            Out.Add(Name.ToString());
        }
        return Out;
    }

    bool AssetExists(IAssetRegistry& AssetRegistry, const FString& PackageName)
    {
        TArray<FAssetData> Assets;
        AssetRegistry.GetAssetsByPackageName(FName(*PackageName), Assets);
        return !Assets.IsEmpty();
    }

    TArray<FString> PackageReferencers(IAssetRegistry& AssetRegistry, const FString& PackageName)
    {
        TArray<FName> Referencers;
        AssetRegistry.GetReferencers(FName(*PackageName), Referencers,
            UE::AssetRegistry::EDependencyCategory::Package);
        return NamesToStrings(Referencers);
    }

    TArray<FString> PackageDependencies(IAssetRegistry& AssetRegistry, const FString& PackageName)
    {
        TArray<FName> Dependencies;
        AssetRegistry.GetDependencies(FName(*PackageName), Dependencies,
            UE::AssetRegistry::EDependencyCategory::Package);
        return NamesToStrings(Dependencies);
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

    bool ParseAssetChangePlan(const FCommandForgePolicyDocument& Document,
        FCommandForgeAssetChangePlanSpec& OutPlan, TArray<FCommandForgeError>& OutErrors)
    {
        OutPlan = FCommandForgeAssetChangePlanSpec();
        OutPlan.Version = Document.Version;
        OutPlan.Kind = Document.Kind;

        if (Document.Kind != TEXT("asset_change_plan"))
        {
            OutErrors.Add({ TEXT("PLAN_KIND_MISMATCH"),
                TEXT("계획 kind는 'asset_change_plan'이어야 합니다."), TEXT("kind") });
            return false;
        }

        Document.RootObject->TryGetStringField(TEXT("transaction_id"), OutPlan.TransactionId);
        Document.RootObject->TryGetStringField(TEXT("rollback_plan"), OutPlan.RollbackPlanPath);
        Document.RootObject->TryGetBoolField(TEXT("allow_delete"), OutPlan.bAllowDelete);
        TryReadStringArray(Document.RootObject, TEXT("delete_assets"), OutPlan.DeleteAssets);

        const TArray<TSharedPtr<FJsonValue>>* OperationValues = nullptr;
        if (!Document.RootObject->TryGetArrayField(TEXT("operations"), OperationValues) ||
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

            FCommandForgeAssetChangeOperation Operation;
            (*OperationObject)->TryGetStringField(TEXT("operation"), Operation.Operation);
            (*OperationObject)->TryGetStringField(TEXT("path"), Operation.Path);
            (*OperationObject)->TryGetStringField(TEXT("source"), Operation.Source);
            (*OperationObject)->TryGetStringField(TEXT("target"), Operation.Target);
            (*OperationObject)->TryGetStringField(TEXT("asset"), Operation.Asset);
            if (Operation.Operation.IsEmpty())
            {
                OutErrors.Add({ TEXT("REQUIRED_FIELD"), TEXT("'operation' 필드는 필수입니다."),
                    FString::Printf(TEXT("operations[%d].operation"), Index) });
            }
            OutPlan.Operations.Add(Operation);
        }
        return OutErrors.IsEmpty();
    }

    void AddRollbackOperation(FCommandForgeRollbackPlan& RollbackPlan, const FString& Operation,
        const FString& BeforeAssetPath, const FString& AfterAssetPath,
        const TArray<FString>& DependencySnapshot)
    {
        FCommandForgeRollbackOperation RollbackOperation;
        RollbackOperation.PlannedOperation = Operation;
        RollbackOperation.BeforeAssetPath = BeforeAssetPath;
        RollbackOperation.AfterAssetPath = AfterAssetPath;
        RollbackOperation.DependencySnapshot = DependencySnapshot;
        RollbackOperation.ValidationSummary.Add(TEXT("preflight"), TEXT("ok"));
        RollbackPlan.Operations.Add(RollbackOperation);
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
            FString::Printf(TEXT("rollback_%s.json"), *TransactionId));

        FString Candidate = RawPath.TrimStartAndEnd();
        if (Candidate.IsEmpty())
        {
            OutPath = DefaultPath;
            return true;
        }

        Candidate = IsAbsoluteFilePath(Candidate)
            ? FPaths::ConvertRelativePathToFull(Candidate)
            : FPaths::Combine(ReportsDir, Candidate);
        FPaths::NormalizeFilename(Candidate);
        FPaths::CollapseRelativeDirectories(Candidate);

        FString ReportsPrefix = ReportsDir;
        FPaths::NormalizeFilename(ReportsPrefix);
        if (!ReportsPrefix.EndsWith(TEXT("/")))
        {
            ReportsPrefix += TEXT("/");
        }

        if (!Candidate.StartsWith(ReportsPrefix, ESearchCase::IgnoreCase))
        {
            AddError(Report, TEXT("ROLLBACK_PATH_OUTSIDE_REPORT_DIR"),
                TEXT("rollback_plan은 Saved/UECommandForge/Reports 아래에만 쓸 수 있습니다."),
                TEXT("rollback_plan"));
            return false;
        }

        OutPath = Candidate;
        return true;
    }
}

int32 UPlanAssetChangesCommandlet::Main(const FString& Params)
{
    using namespace UECommandForge::PlanAssetChangesPrivate;

    TArray<FString> Tokens;
    TArray<FString> Switches;
    TMap<FString, FString> ParamsMap;
    UCommandlet::ParseCommandLine(*Params, Tokens, Switches, ParamsMap);

    const FString OutPath = ParamsMap.FindRef(TEXT("Output"));
    const FString PlanPath = ParamsMap.FindRef(TEXT("Plan"));

    FCommandForgeReport Report;
    Report.Commandlet = TEXT("PlanAssetChanges");
    Report.bDryRun = true;
    Report.bApplied = false;

    if (OutPath.IsEmpty())
    {
        AddError(Report, TEXT("MISSING_ARG"), TEXT("-Output 인자가 없습니다."), TEXT("Output"));
        Report.bOk = false;
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    if (PlanPath.IsEmpty())
    {
        AddError(Report, TEXT("MISSING_ARG"), TEXT("-Plan 인자가 없습니다."), TEXT("Plan"));
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    FString PlanJson;
    if (!FFileHelper::LoadFileToString(PlanJson, *PlanPath))
    {
        AddError(Report, TEXT("PLAN_READ_FAILED"), TEXT("계획 파일을 읽을 수 없습니다."), TEXT("Plan"));
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    FCommandForgePolicyDocument PlanDocument;
    TArray<FCommandForgeError> ParseErrors;
    if (!UECommandForge::FCommandForgePolicyParser::ParseJson(PlanJson, PlanDocument, ParseErrors))
    {
        Report.Errors.Append(ParseErrors);
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    FCommandForgeAssetChangePlanSpec Plan;
    TArray<FCommandForgeError> PlanErrors;
    if (!ParseAssetChangePlan(PlanDocument, Plan, PlanErrors))
    {
        Report.Errors.Append(PlanErrors);
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    Report.TransactionId = Plan.TransactionId.IsEmpty()
        ? FString::Printf(TEXT("tx-%s"), *FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ")))
        : Plan.TransactionId;

    if (!TryResolveRollbackPlanPath(Plan.RollbackPlanPath, Report.TransactionId,
        Report.RollbackPlanPath, Report))
    {
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    FAssetRegistryModule& AssetRegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

    TSet<FString> ExplicitDeleteAssets;
    for (const FString& DeleteAsset : Plan.DeleteAssets)
    {
        ExplicitDeleteAssets.Add(NormalizePackageName(DeleteAsset));
    }

    FCommandForgeRollbackPlan RollbackPlan;
    RollbackPlan.TransactionId = Report.TransactionId;
    RollbackPlan.Commandlet = TEXT("PlanAssetChanges");
    RollbackPlan.Timestamp = FDateTime::UtcNow().ToIso8601();

    int32 PlannedChangeCount = 0;
    for (const FCommandForgeAssetChangeOperation& Operation : Plan.Operations)
    {
        const FString OperationName = Operation.Operation;
        if (OperationName == TEXT("create_folder"))
        {
            const FString FolderPath = NormalizePackageName(Operation.Path);
            if (!IsValidPackagePath(FolderPath) || ContainsWildcard(FolderPath))
            {
                AddIssue(Report, TEXT("error"), TEXT("INVALID_FOLDER_PATH"),
                    TEXT("생성할 폴더 경로가 유효하지 않습니다."), TEXT("path"), FolderPath,
                    TEXT("/Game 또는 /UECommandForge 아래의 명시 경로를 사용하세요."));
                continue;
            }

            AddRollbackOperation(RollbackPlan, TEXT("delete_folder"), TEXT(""), FolderPath, {});
            Report.ChangedFiles.Add(FPackageName::LongPackageNameToFilename(FolderPath));
            ++PlannedChangeCount;
            continue;
        }

        if (OperationName == TEXT("rename_asset") || OperationName == TEXT("move_asset"))
        {
            const FString Source = NormalizePackageName(Operation.Source);
            const FString Target = NormalizePackageName(Operation.Target);
            if (!IsValidPackagePath(Source) || ContainsWildcard(Source))
            {
                AddIssue(Report, TEXT("error"), TEXT("INVALID_SOURCE_PATH"),
                    TEXT("source asset path가 유효하지 않습니다."), TEXT("source"), Source,
                    TEXT("명시적인 long package name을 사용하세요."));
                continue;
            }
            if (!IsValidPackagePath(Target) || ContainsWildcard(Target))
            {
                AddIssue(Report, TEXT("error"), TEXT("INVALID_TARGET_PATH"),
                    TEXT("target asset path가 유효하지 않습니다."), TEXT("target"), Target,
                    TEXT("명시적인 long package name을 사용하세요."));
                continue;
            }
            if (!AssetExists(AssetRegistry, Source))
            {
                AddIssue(Report, TEXT("error"), TEXT("ASSET_NOT_FOUND"),
                    TEXT("source asset을 AssetRegistry에서 찾을 수 없습니다."),
                    TEXT("source"), Source, TEXT("snapshot 후 경로를 확인하세요."));
                continue;
            }
            if (AssetExists(AssetRegistry, Target))
            {
                AddIssue(Report, TEXT("error"), TEXT("TARGET_ALREADY_EXISTS"),
                    TEXT("target asset이 이미 존재합니다."), TEXT("target"), Target,
                    TEXT("다른 target 경로를 사용하세요."));
                continue;
            }

            const TArray<FString> Dependencies = PackageDependencies(AssetRegistry, Source);
            AddRollbackOperation(RollbackPlan, OperationName, Source, Target, Dependencies);
            AddRollbackOperation(RollbackPlan, TEXT("fix_redirector"), PackagePathOf(Source),
                PackagePathOf(Target), {});
            Report.ChangedAssets.Add(Source);
            Report.ChangedAssets.Add(Target);
            ++PlannedChangeCount;
            continue;
        }

        if (OperationName == TEXT("fix_redirector"))
        {
            const FString Path = NormalizePackageName(Operation.Path);
            if (!IsValidPackagePath(Path) || ContainsWildcard(Path))
            {
                AddIssue(Report, TEXT("error"), TEXT("INVALID_REDIRECTOR_PATH"),
                    TEXT("redirector fixup path가 유효하지 않습니다."), TEXT("path"), Path,
                    TEXT("명시적인 경로를 사용하세요."));
                continue;
            }

            AddRollbackOperation(RollbackPlan, TEXT("fix_redirector"), Path, Path, {});
            ++PlannedChangeCount;
            continue;
        }

        if (OperationName == TEXT("delete_unused_asset_candidate"))
        {
            const FString Asset = NormalizePackageName(Operation.Asset);
            if (ContainsWildcard(Asset))
            {
                AddIssue(Report, TEXT("error"), TEXT("WILDCARD_DELETE_FORBIDDEN"),
                    TEXT("delete 작업에는 wildcard를 사용할 수 없습니다."), TEXT("asset"), Asset,
                    TEXT("삭제할 asset을 명시적으로 나열하세요."));
                continue;
            }
            if (!Plan.bAllowDelete)
            {
                AddIssue(Report, TEXT("error"), TEXT("DELETE_NOT_ALLOWED"),
                    TEXT("delete 작업에는 allow_delete=true가 필요합니다."), TEXT("allow_delete"),
                    Asset, TEXT("삭제 승인 후 allow_delete를 true로 설정하세요."));
                continue;
            }
            if (!ExplicitDeleteAssets.Contains(Asset))
            {
                AddIssue(Report, TEXT("error"), TEXT("DELETE_NOT_EXPLICIT"),
                    TEXT("delete asset은 delete_assets 배열에 명시되어야 합니다."),
                    TEXT("delete_assets"), Asset, TEXT("명시 삭제 목록에 추가하세요."));
                continue;
            }
            if (!IsValidPackagePath(Asset) || !AssetExists(AssetRegistry, Asset))
            {
                AddIssue(Report, TEXT("error"), TEXT("ASSET_NOT_FOUND"),
                    TEXT("삭제 대상 asset을 AssetRegistry에서 찾을 수 없습니다."),
                    TEXT("asset"), Asset, TEXT("snapshot 후 경로를 확인하세요."));
                continue;
            }

            const TArray<FString> Referencers = PackageReferencers(AssetRegistry, Asset);
            if (!Referencers.IsEmpty())
            {
                AddIssue(Report, TEXT("error"), TEXT("DELETE_HAS_REFERENCERS"),
                    TEXT("referencer가 있는 asset은 삭제할 수 없습니다."), TEXT("referencers"),
                    Asset, FString::Join(Referencers, TEXT(",")));
                continue;
            }

            AddRollbackOperation(RollbackPlan, TEXT("restore_deleted_asset"), Asset, TEXT(""),
                PackageDependencies(AssetRegistry, Asset));
            Report.ChangedAssets.Add(Asset);
            ++PlannedChangeCount;
            continue;
        }

        AddIssue(Report, TEXT("error"), TEXT("UNKNOWN_OPERATION"),
            TEXT("지원하지 않는 asset change operation입니다."), TEXT("operation"),
            OperationName, TEXT("지원 작업: create_folder, rename_asset, move_asset, fix_redirector, delete_unused_asset_candidate"));
    }

    Report.Validation.Add(TEXT("operation_count"), FString::FromInt(Plan.Operations.Num()));
    Report.Validation.Add(TEXT("planned_change_count"), FString::FromInt(PlannedChangeCount));
    Report.Validation.Add(TEXT("issue_count"), FString::FromInt(Report.ValidationIssues.Num()));
    Report.Validation.Add(TEXT("plan"), PlanPath);

    Report.bOk = Report.Errors.IsEmpty() && !HasErrorIssue(Report.ValidationIssues);
    if (Report.bOk && !RollbackPlan.Operations.IsEmpty())
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
