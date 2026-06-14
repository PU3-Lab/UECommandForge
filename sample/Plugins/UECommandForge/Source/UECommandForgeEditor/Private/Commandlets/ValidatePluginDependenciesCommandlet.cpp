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

    Report.bOk = true; // rules added in later tasks
    UECommandForge::FJsonReportWriter::Write(OutPath, Report);
    return 0;
}
