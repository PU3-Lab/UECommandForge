#include "Commandlets/ValidatePluginDependenciesCommandlet.h"

#include "CommandForgeTypes.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ProjectDescriptor.h"
#include "Reports/JsonReportWriter.h"
#include "Specs/CommandForgePolicyParser.h"

UValidatePluginDependenciesCommandlet::UValidatePluginDependenciesCommandlet()
{
    IsClient     = false;
    IsServer     = false;
    IsEditor     = true;
    LogToConsole = true;
}

namespace UECommandForge::ValidatePluginDepsPrivate
{
    struct FPluginPolicy
    {
        TArray<FString> Required;
        TArray<FString> ForbiddenInShipping;
        TArray<FString> Optional;
        TArray<FString> AllowedEditorOnly;
    };

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
        const FString& Message, const FString& Field, const FString& FilePath,
        const FString& SuggestedFix)
    {
        FCommandForgeValidationIssue Issue;
        Issue.Severity = Severity;
        Issue.Code = Code;
        Issue.Message = Message;
        Issue.Field = Field;
        Issue.FilePath = FilePath;
        Issue.SuggestedFix = SuggestedFix;
        Report.ValidationIssues.Add(Issue);
    }

    bool HasErrorIssue(const TArray<FCommandForgeValidationIssue>& Issues)
    {
        for (const FCommandForgeValidationIssue& I : Issues)
        {
            if (I.Severity.Equals(TEXT("error"), ESearchCase::IgnoreCase)) { return true; }
        }
        return false;
    }

    bool TryGetStringArray(const TSharedPtr<FJsonObject>& Object, const FString& FieldName,
        TArray<FString>& OutValues)
    {
        const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
        if (!Object->TryGetArrayField(FieldName, Values) || Values == nullptr)
        {
            return true; // optional field
        }
        for (const TSharedPtr<FJsonValue>& Value : *Values)
        {
            FString S;
            if (!Value->TryGetString(S)) { return false; }
            OutValues.Add(S.TrimStartAndEnd());
        }
        return true;
    }

    bool ParsePolicy(const FCommandForgePolicyDocument& Document, FPluginPolicy& Out,
        FCommandForgeReport& Report)
    {
        if (!Document.Kind.Equals(TEXT("plugin_dependency_policy"), ESearchCase::IgnoreCase))
        {
            AddError(Report, TEXT("POLICY_KIND_MISMATCH"),
                TEXT("정책 kind는 'plugin_dependency_policy'여야 합니다."), TEXT("kind"));
            return false;
        }
        bool bValid = true;
        bValid &= TryGetStringArray(Document.RootObject, TEXT("required"), Out.Required);
        bValid &= TryGetStringArray(Document.RootObject, TEXT("forbiddenInShipping"), Out.ForbiddenInShipping);
        bValid &= TryGetStringArray(Document.RootObject, TEXT("optional"), Out.Optional);
        bValid &= TryGetStringArray(Document.RootObject, TEXT("allowedEditorOnly"), Out.AllowedEditorOnly);
        if (!bValid)
        {
            AddError(Report, TEXT("PLUGIN_POLICY_INVALID"),
                TEXT("정책 배열은 string 배열이어야 합니다."), TEXT("policy"));
        }
        return bValid;
    }

    bool LoadEnabledPlugins(const FString& ProjectPath, TMap<FString, bool>& OutEnabled,
        FCommandForgeReport& Report)
    {
        FProjectDescriptor Descriptor;
        FText FailReason;
        if (!Descriptor.Load(ProjectPath, FailReason))
        {
            AddError(Report, TEXT("PROJECT_READ_FAILED"),
                FString::Printf(TEXT(".uproject 로드 실패: %s"), *FailReason.ToString()),
                TEXT("Project"));
            return false;
        }
        for (const FPluginReferenceDescriptor& Ref : Descriptor.Plugins)
        {
            OutEnabled.Add(Ref.Name, Ref.bEnabled);
        }
        return true;
    }
}

int32 UValidatePluginDependenciesCommandlet::Main(const FString& Params)
{
    namespace Private = UECommandForge::ValidatePluginDepsPrivate;

    TArray<FString> Tokens;
    TArray<FString> Switches;
    TMap<FString, FString> ParamsMap;
    UCommandlet::ParseCommandLine(*Params, Tokens, Switches, ParamsMap);

    const FString OutPath = ParamsMap.FindRef(TEXT("Output"));
    const FString PolicyPath = ParamsMap.FindRef(TEXT("Policy"));

    FCommandForgeReport Report;
    Report.Commandlet = TEXT("ValidatePluginDependencies");
    Report.bDryRun = true;

    if (OutPath.IsEmpty())
    {
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }
    if (PolicyPath.IsEmpty())
    {
        Private::AddError(Report, TEXT("MISSING_ARG"), TEXT("-Policy 인자가 없습니다."), TEXT("Policy"));
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    FString PolicyJson;
    if (!FFileHelper::LoadFileToString(PolicyJson, *PolicyPath))
    {
        Private::AddError(Report, TEXT("POLICY_READ_FAILED"),
            TEXT("정책 파일을 읽을 수 없습니다."), TEXT("Policy"));
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    FCommandForgePolicyDocument Document;
    TArray<FCommandForgeError> ParseErrors;
    if (!UECommandForge::FCommandForgePolicyParser::ParseJson(PolicyJson, Document, ParseErrors))
    {
        Report.Errors.Append(ParseErrors);
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    Private::FPluginPolicy Policy;
    if (!Private::ParsePolicy(Document, Policy, Report))
    {
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    const FString ProjectPath = ParamsMap.FindRef(TEXT("Project"));
    TMap<FString, bool> EnabledPlugins;
    if (!ProjectPath.IsEmpty())
    {
        if (!Private::LoadEnabledPlugins(ProjectPath, EnabledPlugins, Report))
        {
            Report.bOk = false;
            UECommandForge::FJsonReportWriter::Write(OutPath, Report);
            return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
        }
    }

    int32 EnabledCount = 0;
    for (const TPair<FString, bool>& Pair : EnabledPlugins)
    {
        if (Pair.Value) { ++EnabledCount; }
    }
    Report.Validation.Add(TEXT("enabled_plugin_count"), FString::FromInt(EnabledCount));

    for (const FString& Name : Policy.Required)
    {
        const bool* Enabled = EnabledPlugins.Find(Name);
        if (Enabled == nullptr)
        {
            Private::AddIssue(Report, TEXT("error"), TEXT("PLUGIN_REQUIRED_MISSING"),
                TEXT("Required plugin is missing."),
                FString::Printf(TEXT("required[%s]"), *Name), ProjectPath,
                TEXT("Enable the plugin in the project or remove it from the required policy."));
        }
        else if (!*Enabled)
        {
            Private::AddIssue(Report, TEXT("error"), TEXT("PLUGIN_REQUIRED_DISABLED"),
                TEXT("Required plugin is present but disabled."),
                FString::Printf(TEXT("required[%s]"), *Name), ProjectPath,
                TEXT("Set Enabled=true for this plugin in the .uproject."));
        }
    }

    Report.Validation.Add(TEXT("issue_count"), FString::FromInt(Report.ValidationIssues.Num()));
    Report.bOk = Report.Errors.IsEmpty() && !Private::HasErrorIssue(Report.ValidationIssues);
    const bool bWrote = UECommandForge::FJsonReportWriter::Write(OutPath, Report);
    if (!bWrote) { return static_cast<int32>(ECommandForgeExitCode::EngineError); }
    return Report.bOk ? 0 : static_cast<int32>(ECommandForgeExitCode::ValidationFailed);
}
