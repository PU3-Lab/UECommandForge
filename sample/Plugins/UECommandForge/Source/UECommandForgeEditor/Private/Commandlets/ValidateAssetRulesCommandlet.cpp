#include "Commandlets/ValidateAssetRulesCommandlet.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Misc/FileHelper.h"
#include "Reports/JsonReportWriter.h"
#include "Specs/AssetPolicySpec.h"
#include "Specs/CommandForgePolicyParser.h"

UValidateAssetRulesCommandlet::UValidateAssetRulesCommandlet()
{
    IsClient     = false;
    IsServer     = false;
    IsEditor     = true;
    LogToConsole = true;
}

namespace
{
    TArray<FString> ParseValidateAssetRulesRootPaths(const FString& RootPathsArg)
    {
        TArray<FString> RootPaths;
        if (RootPathsArg.IsEmpty())
        {
            RootPaths.Add(TEXT("/Game"));
            RootPaths.Add(TEXT("/UECommandForge"));
            return RootPaths;
        }

        RootPathsArg.ParseIntoArray(RootPaths, TEXT(","), true);
        for (FString& RootPath : RootPaths)
        {
            RootPath = RootPath.TrimStartAndEnd();
        }
        return RootPaths;
    }

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

    bool TryReadStringArray(const TSharedPtr<FJsonObject>& Object, const FString& Field,
        TArray<FString>& OutValues)
    {
        FString SingleValue;
        if (Object->TryGetStringField(Field, SingleValue))
        {
            OutValues.Add(SingleValue);
            return true;
        }

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

    bool ParseAssetPolicy(const FCommandForgePolicyDocument& Document,
        FCommandForgeAssetPolicySpec& OutPolicy, TArray<FCommandForgeError>& OutErrors)
    {
        OutPolicy = FCommandForgeAssetPolicySpec();
        OutPolicy.Version = Document.Version;
        OutPolicy.Kind = Document.Kind;

        if (Document.Kind != TEXT("asset_policy"))
        {
            OutErrors.Add({ TEXT("POLICY_KIND_MISMATCH"),
                TEXT("정책 kind는 'asset_policy'여야 합니다."), TEXT("kind") });
            return false;
        }

        const TArray<TSharedPtr<FJsonValue>>* RuleValues = nullptr;
        if (!Document.RootObject->TryGetArrayField(TEXT("rules"), RuleValues) || RuleValues == nullptr)
        {
            OutErrors.Add({ TEXT("REQUIRED_FIELD"), TEXT("'rules' 배열은 필수입니다."), TEXT("rules") });
            return false;
        }

        for (int32 Index = 0; Index < RuleValues->Num(); ++Index)
        {
            const TSharedPtr<FJsonObject>* RuleObject = nullptr;
            if (!(*RuleValues)[Index]->TryGetObject(RuleObject) ||
                RuleObject == nullptr || !RuleObject->IsValid())
            {
                OutErrors.Add({ TEXT("INVALID_RULE"), TEXT("rules 항목은 object여야 합니다."),
                    FString::Printf(TEXT("rules[%d]"), Index) });
                continue;
            }

            FCommandForgeAssetPolicyRule Rule;
            (*RuleObject)->TryGetStringField(TEXT("asset_class"), Rule.AssetClass);
            (*RuleObject)->TryGetStringField(TEXT("prefix"), Rule.Prefix);
            (*RuleObject)->TryGetStringField(TEXT("suffix"), Rule.Suffix);
            TryReadStringArray(*RuleObject, TEXT("allowed_path"), Rule.AllowedPaths);
            TryReadStringArray(*RuleObject, TEXT("allowed_paths"), Rule.AllowedPaths);

            if (Rule.AssetClass.IsEmpty())
            {
                OutErrors.Add({ TEXT("REQUIRED_FIELD"), TEXT("'asset_class' 필드는 필수입니다."),
                    FString::Printf(TEXT("rules[%d].asset_class"), Index) });
            }
            OutPolicy.Rules.Add(Rule);
        }

        const TArray<TSharedPtr<FJsonValue>>* ExceptionValues = nullptr;
        if (Document.RootObject->TryGetArrayField(TEXT("exceptions"), ExceptionValues) &&
            ExceptionValues != nullptr)
        {
            for (const TSharedPtr<FJsonValue>& ExceptionValue : *ExceptionValues)
            {
                const TSharedPtr<FJsonObject>* ExceptionObject = nullptr;
                if (!ExceptionValue->TryGetObject(ExceptionObject) ||
                    ExceptionObject == nullptr || !ExceptionObject->IsValid())
                {
                    continue;
                }

                FCommandForgeAssetPolicyException Exception;
                (*ExceptionObject)->TryGetStringField(TEXT("package_name"), Exception.PackageName);
                (*ExceptionObject)->TryGetStringField(TEXT("object_path"), Exception.ObjectPath);
                (*ExceptionObject)->TryGetStringField(TEXT("reason"), Exception.Reason);
                OutPolicy.Exceptions.Add(Exception);
            }
        }

        const TSharedPtr<FJsonObject>* UnusedObject = nullptr;
        if (Document.RootObject->TryGetObjectField(TEXT("unused_asset_candidates"), UnusedObject) &&
            UnusedObject != nullptr && UnusedObject->IsValid())
        {
            (*UnusedObject)->TryGetBoolField(TEXT("enabled"), OutPolicy.UnusedAssetCandidates.bEnabled);
            (*UnusedObject)->TryGetStringField(TEXT("severity"), OutPolicy.UnusedAssetCandidates.Severity);
        }

        const TSharedPtr<FJsonObject>* RedirectorObject = nullptr;
        if (Document.RootObject->TryGetObjectField(TEXT("redirectors"), RedirectorObject) &&
            RedirectorObject != nullptr && RedirectorObject->IsValid())
        {
            (*RedirectorObject)->TryGetBoolField(TEXT("allowed"), OutPolicy.Redirectors.bAllowed);
            (*RedirectorObject)->TryGetStringField(TEXT("severity"), OutPolicy.Redirectors.Severity);
        }

        return OutErrors.IsEmpty();
    }

    bool MatchesAssetClass(const FCommandForgeAssetPolicyRule& Rule, const FAssetData& AssetData)
    {
        if (Rule.AssetClass == TEXT("*"))
        {
            return true;
        }

        const FString FullClass = AssetData.AssetClassPath.ToString();
        const FString ShortClass = AssetData.AssetClassPath.GetAssetName().ToString();
        return Rule.AssetClass == FullClass || Rule.AssetClass == ShortClass;
    }

    bool IsAllowedPath(const FString& PackagePath, const TArray<FString>& AllowedPaths)
    {
        if (AllowedPaths.IsEmpty())
        {
            return true;
        }

        for (const FString& AllowedPath : AllowedPaths)
        {
            if (PackagePath == AllowedPath || PackagePath.StartsWith(AllowedPath + TEXT("/")))
            {
                return true;
            }
        }
        return false;
    }

    bool IsExcepted(const FCommandForgeAssetPolicySpec& Policy, const FAssetData& AssetData)
    {
        const FString PackageName = AssetData.PackageName.ToString();
        const FString ObjectPath = AssetData.GetObjectPathString();
        for (const FCommandForgeAssetPolicyException& Exception : Policy.Exceptions)
        {
            if ((!Exception.PackageName.IsEmpty() && Exception.PackageName == PackageName) ||
                (!Exception.ObjectPath.IsEmpty() && Exception.ObjectPath == ObjectPath))
            {
                return true;
            }
        }
        return false;
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
}

int32 UValidateAssetRulesCommandlet::Main(const FString& Params)
{
    TArray<FString> Tokens;
    TArray<FString> Switches;
    TMap<FString, FString> ParamsMap;
    UCommandlet::ParseCommandLine(*Params, Tokens, Switches, ParamsMap);

    const FString OutPath = ParamsMap.FindRef(TEXT("Output"));
    const FString PolicyPath = ParamsMap.FindRef(TEXT("Policy"));

    FCommandForgeReport Report;
    Report.Commandlet = TEXT("ValidateAssetRules");
    Report.bDryRun = true;
    Report.bApplied = false;

    if (OutPath.IsEmpty())
    {
        AddError(Report, TEXT("MISSING_ARG"), TEXT("-Output 인자가 없습니다."), TEXT("Output"));
        Report.bOk = false;
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    if (PolicyPath.IsEmpty())
    {
        AddError(Report, TEXT("MISSING_ARG"), TEXT("-Policy 인자가 없습니다."), TEXT("Policy"));
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    FString PolicyJson;
    if (!FFileHelper::LoadFileToString(PolicyJson, *PolicyPath))
    {
        AddError(Report, TEXT("POLICY_READ_FAILED"), TEXT("정책 파일을 읽을 수 없습니다."), TEXT("Policy"));
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    FCommandForgePolicyDocument PolicyDocument;
    TArray<FCommandForgeError> ParseErrors;
    if (!UECommandForge::FCommandForgePolicyParser::ParseJson(PolicyJson, PolicyDocument, ParseErrors))
    {
        Report.Errors.Append(ParseErrors);
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    FCommandForgeAssetPolicySpec Policy;
    TArray<FCommandForgeError> PolicyErrors;
    if (!ParseAssetPolicy(PolicyDocument, Policy, PolicyErrors))
    {
        Report.Errors.Append(PolicyErrors);
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    FAssetRegistryModule& AssetRegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

    TArray<FAssetData> AllAssets;
    const TArray<FString> RootPaths = ParseValidateAssetRulesRootPaths(ParamsMap.FindRef(TEXT("RootPaths")));
    for (const FString& RootPath : RootPaths)
    {
        if (!RootPath.IsEmpty())
        {
            AssetRegistry.GetAssetsByPath(FName(*RootPath), AllAssets, true);
        }
    }

    AllAssets.Sort([](const FAssetData& A, const FAssetData& B)
    {
        if (A.PackageName == B.PackageName)
        {
            return A.AssetName.LexicalLess(B.AssetName);
        }
        return A.PackageName.LexicalLess(B.PackageName);
    });

    TSet<FName> SeenPackages;
    for (const FAssetData& AssetData : AllAssets)
    {
        if (SeenPackages.Contains(AssetData.PackageName))
        {
            continue;
        }
        SeenPackages.Add(AssetData.PackageName);
        if (IsExcepted(Policy, AssetData))
        {
            continue;
        }

        const FString AssetName = AssetData.AssetName.ToString();
        const FString PackagePath = AssetData.PackagePath.ToString();
        const FString ObjectPath = AssetData.GetObjectPathString();

        if (!Policy.Redirectors.bAllowed &&
            AssetData.AssetClassPath.GetAssetName() == FName(TEXT("ObjectRedirector")))
        {
            AddIssue(Report, Policy.Redirectors.Severity, TEXT("REDIRECTOR_FOUND"),
                TEXT("Redirector asset이 남아 있습니다."), TEXT("asset_class"), ObjectPath,
                TEXT("Fix redirectors 후 다시 검증하세요."));
        }

        for (const FCommandForgeAssetPolicyRule& Rule : Policy.Rules)
        {
            if (!MatchesAssetClass(Rule, AssetData))
            {
                continue;
            }

            if (!Rule.Prefix.IsEmpty() && !AssetName.StartsWith(Rule.Prefix))
            {
                AddIssue(Report, TEXT("error"), TEXT("ASSET_PREFIX_MISMATCH"),
                    FString::Printf(TEXT("Asset name은 '%s' prefix로 시작해야 합니다."), *Rule.Prefix),
                    TEXT("asset_name"), ObjectPath,
                    FString::Printf(TEXT("이름을 '%s%s' 형태로 변경하세요."), *Rule.Prefix, *AssetName));
            }

            if (!Rule.Suffix.IsEmpty() && !AssetName.EndsWith(Rule.Suffix))
            {
                AddIssue(Report, TEXT("error"), TEXT("ASSET_SUFFIX_MISMATCH"),
                    FString::Printf(TEXT("Asset name은 '%s' suffix로 끝나야 합니다."), *Rule.Suffix),
                    TEXT("asset_name"), ObjectPath,
                    FString::Printf(TEXT("이름 끝에 '%s'를 추가하세요."), *Rule.Suffix));
            }

            if (!IsAllowedPath(PackagePath, Rule.AllowedPaths))
            {
                AddIssue(Report, TEXT("error"), TEXT("ASSET_PATH_NOT_ALLOWED"),
                    TEXT("Asset package path가 policy allowed path 밖에 있습니다."),
                    TEXT("package_path"), ObjectPath,
                    Rule.AllowedPaths.IsEmpty() ? TEXT("") :
                        FString::Printf(TEXT("허용 경로로 이동하세요: %s"), *FString::Join(Rule.AllowedPaths, TEXT(","))));
            }
        }

        TArray<FName> Dependencies;
        AssetRegistry.GetDependencies(
            AssetData.PackageName, Dependencies, UE::AssetRegistry::EDependencyCategory::Package);
        for (const FName& Dependency : Dependencies)
        {
            const FString DependencyName = Dependency.ToString();
            if (!DependencyName.StartsWith(TEXT("/Game")) && !DependencyName.StartsWith(TEXT("/UECommandForge")))
            {
                continue;
            }

            TArray<FAssetData> DependencyAssets;
            AssetRegistry.GetAssetsByPackageName(Dependency, DependencyAssets);
            if (DependencyAssets.IsEmpty())
            {
                AddIssue(Report, TEXT("error"), TEXT("MISSING_DEPENDENCY"),
                    TEXT("AssetRegistry dependency package를 찾을 수 없습니다."),
                    TEXT("dependencies"), ObjectPath, DependencyName);
            }
        }

        if (Policy.UnusedAssetCandidates.bEnabled &&
            AssetData.AssetClassPath.GetAssetName() != FName(TEXT("World")))
        {
            TArray<FName> Referencers;
            AssetRegistry.GetReferencers(
                AssetData.PackageName, Referencers, UE::AssetRegistry::EDependencyCategory::Package);
            if (Referencers.IsEmpty())
            {
                AddIssue(Report, Policy.UnusedAssetCandidates.Severity,
                    TEXT("UNUSED_ASSET_CANDIDATE"),
                    TEXT("AssetRegistry 기준 referencer가 없는 에셋 후보입니다."),
                    TEXT("referencers"), ObjectPath,
                    TEXT("삭제 전 사람이 참조 의도를 확인해야 합니다."));
            }
        }
    }

    Report.Validation.Add(TEXT("root_paths"), ParamsMap.FindRef(TEXT("RootPaths")).IsEmpty()
        ? TEXT("/Game,/UECommandForge")
        : ParamsMap.FindRef(TEXT("RootPaths")));
    Report.Validation.Add(TEXT("asset_count"), FString::FromInt(SeenPackages.Num()));
    Report.Validation.Add(TEXT("issue_count"), FString::FromInt(Report.ValidationIssues.Num()));
    Report.Validation.Add(TEXT("policy"), PolicyPath);

    Report.bOk = !HasErrorIssue(Report.ValidationIssues);
    const bool bWrote = UECommandForge::FJsonReportWriter::Write(OutPath, Report);
    if (!bWrote)
    {
        return static_cast<int32>(ECommandForgeExitCode::EngineError);
    }
    return Report.bOk ? 0 : static_cast<int32>(ECommandForgeExitCode::ValidationFailed);
}
