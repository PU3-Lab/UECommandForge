#include "Commandlets/PrototypeAutomationCommandlet.h"

#include "CommandForgeTypes.h"
#include "Commandlets/ValidateAssetRulesCommandlet.h"
#include "Commandlets/ValidateBuildCsCommandlet.h"
#include "Commandlets/ValidateConfigRulesCommandlet.h"
#include "Commandlets/ValidateDataSourceCommandlet.h"
#include "Commandlets/ValidatePluginDependenciesCommandlet.h"
#include "Commandlets/DiffPlatformConfigCommandlet.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Reports/JsonReportWriter.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

UPrototypeAutomationCommandlet::UPrototypeAutomationCommandlet()
{
    IsClient     = false;
    IsServer     = false;
    IsEditor     = true;
    LogToConsole = true;
}

namespace UECommandForge::PrototypeAutomationPrivate
{
    struct FSuiteRun
    {
        FString Name;
        FString Commandlet;
        FString Args;
        FString ReportPath;
        int32 ExitCode = 0;
        bool bOk = false;
        int32 IssueCount = 0;
        int32 ErrorCount = 0;
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

    void AddIssue(FCommandForgeReport& Report, const FString& Code, const FString& Message,
        const FString& Field, const FString& ReportPath, const FString& SuggestedFix)
    {
        FCommandForgeValidationIssue Issue;
        Issue.Severity = TEXT("error");
        Issue.Code = Code;
        Issue.Message = Message;
        Issue.Field = Field;
        Issue.FilePath = ReportPath;
        Issue.SuggestedFix = SuggestedFix;
        Report.ValidationIssues.Add(Issue);
    }

    FString QuoteArg(const FString& Key, const FString& Value)
    {
        return Value.IsEmpty() ? TEXT("") : FString::Printf(TEXT(" -%s=\"%s\""), *Key, *Value);
    }

    FString ChildReportPath(const FString& OutPath, const FString& SuiteName)
    {
        return FPaths::Combine(FPaths::GetPath(OutPath),
            FString::Printf(TEXT("%s_%s.json"), *FPaths::GetBaseFilename(OutPath), *SuiteName));
    }

    bool LoadJsonObject(const FString& Path, TSharedPtr<FJsonObject>& OutObject)
    {
        FString Content;
        if (!FFileHelper::LoadFileToString(Content, *Path))
        {
            return false;
        }

        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
        return FJsonSerializer::Deserialize(Reader, OutObject) && OutObject.IsValid();
    }

    int32 JsonArrayCount(const TSharedPtr<FJsonObject>& Object, const FString& Field)
    {
        const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
        if (!Object->TryGetArrayField(Field, Values) || Values == nullptr)
        {
            return 0;
        }
        return Values->Num();
    }

    void PopulateSuiteFromChildReport(FSuiteRun& Suite)
    {
        TSharedPtr<FJsonObject> RootObject;
        if (!LoadJsonObject(Suite.ReportPath, RootObject))
        {
            Suite.bOk = false;
            Suite.ErrorCount = 1;
            return;
        }

        Suite.bOk = RootObject->GetBoolField(TEXT("ok"));
        Suite.IssueCount = JsonArrayCount(RootObject, TEXT("issues"));
        Suite.ErrorCount = JsonArrayCount(RootObject, TEXT("errors"));
    }

    FCommandForgeStepResult ToStepResult(const FSuiteRun& Suite)
    {
        FCommandForgeStepResult Step;
        Step.Step = Suite.Commandlet;
        Step.bOk = Suite.bOk;
        Step.Validation.Add(TEXT("suite"), Suite.Name);
        Step.Validation.Add(TEXT("report_path"), Suite.ReportPath);
        Step.Validation.Add(TEXT("exit_code"), FString::FromInt(Suite.ExitCode));
        Step.Validation.Add(TEXT("issue_count"), FString::FromInt(Suite.IssueCount));
        Step.Validation.Add(TEXT("error_count"), FString::FromInt(Suite.ErrorCount));
        if (!Suite.bOk)
        {
            FCommandForgeError Error;
            Error.Code = TEXT("PROTOTYPE_SUITE_FAILED");
            Error.Message = FString::Printf(TEXT("%s suite가 실패했습니다."), *Suite.Commandlet);
            Error.Field = Suite.Name;
            Step.Errors.Add(Error);
        }
        return Step;
    }

    FString EscapeMarkdownCell(FString Value)
    {
        Value.ReplaceInline(TEXT("|"), TEXT("\\|"));
        Value.ReplaceInline(TEXT("\r"), TEXT(" "));
        Value.ReplaceInline(TEXT("\n"), TEXT(" "));
        return Value;
    }

    bool WriteMarkdownReport(const FString& MarkdownPath, const TArray<FSuiteRun>& Suites,
        bool bOk, const FString& JsonReportPath)
    {
        FString Markdown;
        Markdown += TEXT("# PrototypeAutomation Report\n\n");
        Markdown += FString::Printf(TEXT("- Status: %s\n"), bOk ? TEXT("PASS") : TEXT("FAIL"));
        Markdown += FString::Printf(TEXT("- JSON Report: `%s`\n\n"), *JsonReportPath);
        Markdown += TEXT("| Suite | Commandlet | Status | Issues | Errors | Report |\n");
        Markdown += TEXT("| --- | --- | --- | ---: | ---: | --- |\n");
        for (const FSuiteRun& Suite : Suites)
        {
            Markdown += FString::Printf(TEXT("| %s | %s | %s | %d | %d | `%s` |\n"),
                *EscapeMarkdownCell(Suite.Name),
                *EscapeMarkdownCell(Suite.Commandlet),
                Suite.bOk ? TEXT("PASS") : TEXT("FAIL"),
                Suite.IssueCount,
                Suite.ErrorCount,
                *EscapeMarkdownCell(Suite.ReportPath));
        }

        FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FPaths::GetPath(MarkdownPath));
        return FFileHelper::SaveStringToFile(Markdown, *MarkdownPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    }

    int32 RunSuite(const FString& CommandletName, const FString& Args)
    {
        if (CommandletName == TEXT("ValidateAssetRules"))
        {
            return NewObject<UValidateAssetRulesCommandlet>()->Main(Args);
        }
        if (CommandletName == TEXT("ValidateBuildCs"))
        {
            return NewObject<UValidateBuildCsCommandlet>()->Main(Args);
        }
        if (CommandletName == TEXT("ValidateDataSource"))
        {
            return NewObject<UValidateDataSourceCommandlet>()->Main(Args);
        }
        if (CommandletName == TEXT("ValidateConfigRules"))
        {
            return NewObject<UValidateConfigRulesCommandlet>()->Main(Args);
        }
        if (CommandletName == TEXT("ValidatePluginDependencies"))
        {
            return NewObject<UValidatePluginDependenciesCommandlet>()->Main(Args);
        }
        if (CommandletName == TEXT("DiffPlatformConfig"))
        {
            return NewObject<UDiffPlatformConfigCommandlet>()->Main(Args);
        }
        return static_cast<int32>(ECommandForgeExitCode::EngineError);
    }
}

int32 UPrototypeAutomationCommandlet::Main(const FString& Params)
{
    using namespace UECommandForge::PrototypeAutomationPrivate;

    TArray<FString> Tokens;
    TArray<FString> Switches;
    TMap<FString, FString> ParamsMap;
    UCommandlet::ParseCommandLine(*Params, Tokens, Switches, ParamsMap);

    const FString OutPath = ParamsMap.FindRef(TEXT("Output"));
    FString MarkdownPath = ParamsMap.FindRef(TEXT("Markdown"));

    FCommandForgeReport Report;
    Report.Commandlet = TEXT("PrototypeAutomation");
    Report.bDryRun = true;
    Report.bApplied = false;

    if (OutPath.IsEmpty())
    {
        AddError(Report, TEXT("MISSING_ARG"), TEXT("-Output 인자가 없습니다."), TEXT("Output"));
        Report.bOk = false;
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }
    if (MarkdownPath.IsEmpty())
    {
        MarkdownPath = FPaths::Combine(FPaths::GetPath(OutPath),
            FPaths::GetBaseFilename(OutPath) + TEXT(".md"));
    }

    TArray<FSuiteRun> Suites;
    const FString AssetPolicyPath = ParamsMap.FindRef(TEXT("AssetPolicy"));
    if (!AssetPolicyPath.IsEmpty())
    {
        FSuiteRun Suite;
        Suite.Name = TEXT("asset_policy");
        Suite.Commandlet = TEXT("ValidateAssetRules");
        Suite.ReportPath = ChildReportPath(OutPath, Suite.Name);
        Suite.Args = QuoteArg(TEXT("Policy"), AssetPolicyPath) +
            QuoteArg(TEXT("RootPaths"), ParamsMap.FindRef(TEXT("AssetRootPaths"))) +
            QuoteArg(TEXT("Output"), Suite.ReportPath);
        Suites.Add(Suite);
    }

    const FString BuildCsPolicyPath = ParamsMap.FindRef(TEXT("BuildCsPolicy"));
    if (!BuildCsPolicyPath.IsEmpty())
    {
        FSuiteRun Suite;
        Suite.Name = TEXT("buildcs_policy");
        Suite.Commandlet = TEXT("ValidateBuildCs");
        Suite.ReportPath = ChildReportPath(OutPath, Suite.Name);
        Suite.Args = QuoteArg(TEXT("Policy"), BuildCsPolicyPath) +
            QuoteArg(TEXT("Output"), Suite.ReportPath);
        Suites.Add(Suite);
    }

    const FString DataSchemaPath = ParamsMap.FindRef(TEXT("DataSchema"));
    const FString DataSourcePath = ParamsMap.FindRef(TEXT("DataSource"));
    if (!DataSchemaPath.IsEmpty() || !DataSourcePath.IsEmpty())
    {
        if (DataSchemaPath.IsEmpty() || DataSourcePath.IsEmpty())
        {
            AddError(Report, TEXT("DATA_ARGS_INCOMPLETE"),
                TEXT("-DataSchema와 -DataSource는 함께 제공해야 합니다."), TEXT("DataSchema"));
        }
        else
        {
            FSuiteRun Suite;
            Suite.Name = TEXT("data_source");
            Suite.Commandlet = TEXT("ValidateDataSource");
            Suite.ReportPath = ChildReportPath(OutPath, Suite.Name);
            Suite.Args = QuoteArg(TEXT("Schema"), DataSchemaPath) +
                QuoteArg(TEXT("Source"), DataSourcePath) +
                QuoteArg(TEXT("Output"), Suite.ReportPath);
            Suites.Add(Suite);
        }
    }

    const FString ConfigRulesPath = ParamsMap.FindRef(TEXT("ConfigRules"));
    if (!ConfigRulesPath.IsEmpty())
    {
        FSuiteRun Suite;
        Suite.Name = TEXT("config_rules");
        Suite.Commandlet = TEXT("ValidateConfigRules");
        Suite.ReportPath = ChildReportPath(OutPath, Suite.Name);
        Suite.Args = QuoteArg(TEXT("Schema"), ConfigRulesPath) +
            QuoteArg(TEXT("Config"), ParamsMap.FindRef(TEXT("Config"))) +
            QuoteArg(TEXT("Output"), Suite.ReportPath);
        Suites.Add(Suite);
    }

    const FString PluginPolicyPath = ParamsMap.FindRef(TEXT("PluginPolicy"));
    if (!PluginPolicyPath.IsEmpty())
    {
        FSuiteRun Suite;
        Suite.Name = TEXT("plugin_deps");
        Suite.Commandlet = TEXT("ValidatePluginDependencies");
        Suite.ReportPath = ChildReportPath(OutPath, Suite.Name);
        Suite.Args = QuoteArg(TEXT("Policy"), PluginPolicyPath) +
            QuoteArg(TEXT("Project"), ParamsMap.FindRef(TEXT("Project"))) +
            QuoteArg(TEXT("Configuration"), ParamsMap.FindRef(TEXT("Configuration"))) +
            QuoteArg(TEXT("TargetPlatform"), ParamsMap.FindRef(TEXT("TargetPlatform"))) +
            QuoteArg(TEXT("Output"), Suite.ReportPath);
        Suites.Add(Suite);
    }

    const FString DiffPlatforms = ParamsMap.FindRef(TEXT("DiffPlatforms"));
    if (!DiffPlatforms.IsEmpty())
    {
        FSuiteRun Suite;
        Suite.Name = TEXT("platform_config_diff");
        Suite.Commandlet = TEXT("DiffPlatformConfig");
        Suite.ReportPath = ChildReportPath(OutPath, Suite.Name);
        Suite.Args = QuoteArg(TEXT("Project"), ParamsMap.FindRef(TEXT("Project"))) +
            QuoteArg(TEXT("Platforms"), DiffPlatforms) +
            QuoteArg(TEXT("Allowlist"), ParamsMap.FindRef(TEXT("DiffAllowlist"))) +
            QuoteArg(TEXT("Categories"), ParamsMap.FindRef(TEXT("DiffCategories"))) +
            QuoteArg(TEXT("Output"), Suite.ReportPath);
        Suites.Add(Suite);
    }

    if (Suites.IsEmpty())
    {
        AddError(Report, TEXT("NO_SUITES"),
            TEXT("실행할 PrototypeAutomation suite 인자가 없습니다."), TEXT("AssetPolicy"));
    }

    if (!Report.Errors.IsEmpty())
    {
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    int32 FailedSuiteCount = 0;
    for (FSuiteRun& Suite : Suites)
    {
        Suite.ExitCode = RunSuite(Suite.Commandlet, Suite.Args);
        PopulateSuiteFromChildReport(Suite);
        if (Suite.ExitCode != 0)
        {
            Suite.bOk = false;
        }
        Report.Steps.Add(ToStepResult(Suite));

        if (!Suite.bOk)
        {
            ++FailedSuiteCount;
            AddIssue(Report, TEXT("PROTOTYPE_SUITE_FAILED"),
                FString::Printf(TEXT("%s suite가 실패했습니다."), *Suite.Commandlet),
                Suite.Name, Suite.ReportPath,
                TEXT("child report의 errors/issues를 확인하세요."));
        }
    }

    Report.Validation.Add(TEXT("suite_count"), FString::FromInt(Suites.Num()));
    Report.Validation.Add(TEXT("failed_suite_count"), FString::FromInt(FailedSuiteCount));
    Report.Validation.Add(TEXT("markdown_path"), MarkdownPath);
    Report.bOk = FailedSuiteCount == 0 && Report.Errors.IsEmpty();

    if (!WriteMarkdownReport(MarkdownPath, Suites, Report.bOk, OutPath))
    {
        AddError(Report, TEXT("MARKDOWN_WRITE_FAILED"),
            TEXT("PrototypeAutomation markdown report를 저장할 수 없습니다."), TEXT("Markdown"));
        Report.bOk = false;
    }

    const bool bWrote = UECommandForge::FJsonReportWriter::Write(OutPath, Report);
    if (!bWrote)
    {
        return static_cast<int32>(ECommandForgeExitCode::EngineError);
    }
    return Report.bOk ? 0 : static_cast<int32>(ECommandForgeExitCode::ValidationFailed);
}
