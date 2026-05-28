#include "Commandlets/ValidateBuildCsCommandlet.h"

#include "BuildCs/BuildCsDependencyUtils.h"
#include "CommandForgeTypes.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Reports/JsonReportWriter.h"
#include "Specs/CommandForgePolicyParser.h"

UValidateBuildCsCommandlet::UValidateBuildCsCommandlet()
{
    IsClient     = false;
    IsServer     = false;
    IsEditor     = true;
    LogToConsole = true;
}

namespace UECommandForge::ValidateBuildCsPrivate
{
    struct FBuildCsPolicyModule
    {
        FString Name;
        TArray<FString> RequiredPublicDependencies;
        TArray<FString> RequiredPrivateDependencies;
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
        const FString& Field, const FString& FilePath, const FString& SuggestedFix)
    {
        FCommandForgeValidationIssue Issue;
        Issue.Severity = TEXT("error");
        Issue.Code = Code;
        Issue.Message = Message;
        Issue.Field = Field;
        Issue.FilePath = FilePath;
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

    bool TryGetStringArray(const TSharedPtr<FJsonObject>& Object, const FString& FieldName,
        TArray<FString>& OutValues)
    {
        const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
        if (!Object->TryGetArrayField(FieldName, Values) || Values == nullptr)
        {
            return true;
        }

        for (const TSharedPtr<FJsonValue>& Value : *Values)
        {
            FString StringValue;
            if (!Value->TryGetString(StringValue))
            {
                return false;
            }
            OutValues.Add(StringValue.TrimStartAndEnd());
        }
        return true;
    }

    bool ValidateDependencyNames(const TArray<FString>& Dependencies, const FString& FieldName,
        int32 ModuleIndex, FCommandForgeReport& Report)
    {
        bool bValid = true;
        for (int32 DependencyIndex = 0; DependencyIndex < Dependencies.Num(); ++DependencyIndex)
        {
            if (!UECommandForge::BuildCs::IsSafeModuleName(Dependencies[DependencyIndex]))
            {
                AddError(Report, TEXT("INVALID_BUILD_DEPENDENCY"),
                    TEXT("Build.cs dependency는 module identifier여야 합니다."),
                    FString::Printf(TEXT("modules[%d].%s[%d]"), ModuleIndex, *FieldName, DependencyIndex));
                bValid = false;
            }
        }
        return bValid;
    }

    bool ParsePolicy(const FCommandForgePolicyDocument& Document,
        TArray<FBuildCsPolicyModule>& OutModules, FCommandForgeReport& Report)
    {
        if (!Document.Kind.Equals(TEXT("buildcs_policy"), ESearchCase::IgnoreCase))
        {
            AddError(Report, TEXT("POLICY_KIND_MISMATCH"),
                TEXT("정책 kind는 'buildcs_policy'여야 합니다."), TEXT("kind"));
            return false;
        }

        const TArray<TSharedPtr<FJsonValue>>* ModuleValues = nullptr;
        if (!Document.RootObject->TryGetArrayField(TEXT("modules"), ModuleValues) || ModuleValues == nullptr)
        {
            AddError(Report, TEXT("REQUIRED_FIELD"), TEXT("'modules' 배열은 필수입니다."), TEXT("modules"));
            return false;
        }

        for (int32 Index = 0; Index < ModuleValues->Num(); ++Index)
        {
            const TSharedPtr<FJsonObject>* ModuleObject = nullptr;
            if (!(*ModuleValues)[Index]->TryGetObject(ModuleObject) ||
                ModuleObject == nullptr || !ModuleObject->IsValid())
            {
                AddError(Report, TEXT("INVALID_MODULE"),
                    TEXT("modules 항목은 object여야 합니다."),
                    FString::Printf(TEXT("modules[%d]"), Index));
                continue;
            }

            FBuildCsPolicyModule Module;
            (*ModuleObject)->TryGetStringField(TEXT("name"), Module.Name);
            Module.Name = Module.Name.TrimStartAndEnd();
            if (!TryGetStringArray(*ModuleObject, TEXT("required_public"), Module.RequiredPublicDependencies))
            {
                AddError(Report, TEXT("INVALID_DEPENDENCY_ARRAY"),
                    TEXT("required_public은 string 배열이어야 합니다."),
                    FString::Printf(TEXT("modules[%d].required_public"), Index));
            }
            if (!TryGetStringArray(*ModuleObject, TEXT("required_private"), Module.RequiredPrivateDependencies))
            {
                AddError(Report, TEXT("INVALID_DEPENDENCY_ARRAY"),
                    TEXT("required_private는 string 배열이어야 합니다."),
                    FString::Printf(TEXT("modules[%d].required_private"), Index));
            }
            if (Module.Name.IsEmpty())
            {
                AddError(Report, TEXT("REQUIRED_FIELD"),
                    TEXT("module name은 필수입니다."),
                    FString::Printf(TEXT("modules[%d].name"), Index));
            }
            ValidateDependencyNames(Module.RequiredPublicDependencies, TEXT("required_public"), Index, Report);
            ValidateDependencyNames(Module.RequiredPrivateDependencies, TEXT("required_private"), Index, Report);
            OutModules.Add(Module);
        }
        return Report.Errors.IsEmpty();
    }
}

int32 UValidateBuildCsCommandlet::Main(const FString& Params)
{
    namespace BuildCsPrivate = UECommandForge::ValidateBuildCsPrivate;

    TArray<FString> Tokens;
    TArray<FString> Switches;
    TMap<FString, FString> ParamsMap;
    UCommandlet::ParseCommandLine(*Params, Tokens, Switches, ParamsMap);

    const FString OutPath = ParamsMap.FindRef(TEXT("Output"));
    const FString PolicyPath = ParamsMap.FindRef(TEXT("Policy"));

    FCommandForgeReport Report;
    Report.Commandlet = TEXT("ValidateBuildCs");
    Report.bDryRun = true;
    Report.bApplied = false;

    if (OutPath.IsEmpty())
    {
        BuildCsPrivate::AddError(Report, TEXT("MISSING_ARG"), TEXT("-Output 인자가 없습니다."), TEXT("Output"));
        Report.bOk = false;
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }
    if (PolicyPath.IsEmpty())
    {
        BuildCsPrivate::AddError(Report, TEXT("MISSING_ARG"), TEXT("-Policy 인자가 없습니다."), TEXT("Policy"));
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    FString PolicyJson;
    if (!FFileHelper::LoadFileToString(PolicyJson, *PolicyPath))
    {
        BuildCsPrivate::AddError(Report, TEXT("POLICY_READ_FAILED"), TEXT("Build.cs policy 파일을 읽을 수 없습니다."), TEXT("Policy"));
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

    TArray<BuildCsPrivate::FBuildCsPolicyModule> Modules;
    if (!BuildCsPrivate::ParsePolicy(Document, Modules, Report))
    {
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    int32 CheckedModuleCount = 0;
    for (const BuildCsPrivate::FBuildCsPolicyModule& Module : Modules)
    {
        if (!UECommandForge::BuildCs::IsSafeModuleName(Module.Name))
        {
            BuildCsPrivate::AddIssue(Report, TEXT("INVALID_MODULE_NAME"),
                TEXT("module name은 C++ identifier여야 합니다."),
                TEXT("modules.name"), Module.Name, TEXT("예: UECommandForgeRuntime"));
            continue;
        }

        const UECommandForge::BuildCs::FBuildCsDependencyCheck Check =
            UECommandForge::BuildCs::CheckDependencies(Module.Name,
                Module.RequiredPublicDependencies, Module.RequiredPrivateDependencies);
        ++CheckedModuleCount;

        Report.Validation.Add(Module.Name + TEXT(".build_cs_path"), Check.BuildCsPath);
        Report.Validation.Add(Module.Name + TEXT(".missing_public"),
            FString::Join(Check.MissingPublicDependencies, TEXT(",")));
        Report.Validation.Add(Module.Name + TEXT(".missing_private"),
            FString::Join(Check.MissingPrivateDependencies, TEXT(",")));

        if (!Check.bBuildCsExists)
        {
            BuildCsPrivate::AddIssue(Report, TEXT("BUILDCS_NOT_FOUND"),
                TEXT("module Build.cs 파일을 찾을 수 없습니다."),
                TEXT("modules.name"), Check.BuildCsPath,
                TEXT("module 이름과 plugin/project Source 경로를 확인하세요."));
            continue;
        }

        for (const FString& Dependency : Check.MissingPublicDependencies)
        {
            BuildCsPrivate::AddIssue(Report, TEXT("BUILDCS_PUBLIC_DEPENDENCY_MISSING"),
                TEXT("PublicDependencyModuleNames에 필요한 module dependency가 없습니다."),
                TEXT("required_public"), Check.BuildCsPath,
                FString::Printf(TEXT("PublicDependencyModuleNames에 \"%s\"를 추가하세요."), *Dependency));
        }
        for (const FString& Dependency : Check.MissingPrivateDependencies)
        {
            BuildCsPrivate::AddIssue(Report, TEXT("BUILDCS_PRIVATE_DEPENDENCY_MISSING"),
                TEXT("PrivateDependencyModuleNames에 필요한 module dependency가 없습니다."),
                TEXT("required_private"), Check.BuildCsPath,
                FString::Printf(TEXT("PrivateDependencyModuleNames에 \"%s\"를 추가하세요."), *Dependency));
        }
    }

    Report.Validation.Add(TEXT("checked_module_count"), FString::FromInt(CheckedModuleCount));
    Report.Validation.Add(TEXT("issue_count"), FString::FromInt(Report.ValidationIssues.Num()));
    Report.bOk = Report.Errors.IsEmpty() && !BuildCsPrivate::HasErrorIssue(Report.ValidationIssues);
    const bool bWrote = UECommandForge::FJsonReportWriter::Write(OutPath, Report);
    if (!bWrote)
    {
        return static_cast<int32>(ECommandForgeExitCode::EngineError);
    }
    return Report.bOk ? 0 : static_cast<int32>(ECommandForgeExitCode::ValidationFailed);
}
