#include "Commandlets/CreateStateTreeCommandlet.h"
#include "Builders/StateTreeBuilder.h"
#include "CommandForgeTypes.h"
#include "Misc/FileHelper.h"
#include "Reports/JsonReportWriter.h"
#include "Specs/AIFlowSpecParser.h"
#include "Specs/AIFlowSpecValidator.h"

UCreateStateTreeCommandlet::UCreateStateTreeCommandlet()
{
    IsClient     = false;
    IsServer     = false;
    IsEditor     = true;
    LogToConsole = true;
}

int32 UCreateStateTreeCommandlet::Main(const FString& Params)
{
    TArray<FString> Tokens;
    TArray<FString> Switches;
    TMap<FString, FString> ParamsMap;
    UCommandlet::ParseCommandLine(*Params, Tokens, Switches, ParamsMap);

    const FString SpecFile = ParamsMap.FindRef(TEXT("SpecFile"));
    const FString OutPath  = ParamsMap.FindRef(TEXT("Output"));

    FCommandForgeReport Report;
    Report.Commandlet = TEXT("CreateStateTree");

    if (SpecFile.IsEmpty() || OutPath.IsEmpty())
    {
        FCommandForgeError Err;
        Err.Code    = TEXT("MISSING_PARAM");
        Err.Message = TEXT("-SpecFile 또는 -Output 파라미터 누락");
        Report.Errors.Add(Err);
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath.IsEmpty() ? TEXT("/dev/null") : OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    FString JsonStr;
    if (!FFileHelper::LoadFileToString(JsonStr, *SpecFile))
    {
        FCommandForgeError Err;
        Err.Code    = TEXT("SPEC_FILE_NOT_FOUND");
        Err.Message = FString::Printf(TEXT("파일 없음: %s"), *SpecFile);
        Report.Errors.Add(Err);
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    FAIFlowSpec Spec;
    TArray<FCommandForgeError> ParseErrors;
    if (!UECommandForge::FAIFlowSpecParser::Parse(JsonStr, Spec, ParseErrors))
    {
        Report.Errors = ParseErrors;
        Report.bOk    = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    TArray<FCommandForgeError> ValidateErrors;
    UECommandForge::FAIFlowSpecValidator::Validate(Spec, ValidateErrors);
    if (!ValidateErrors.IsEmpty())
    {
        Report.Errors = ValidateErrors;
        Report.bOk    = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::ValidationFailed);
    }

    TArray<FCommandForgeError> BuildErrors;
    TMap<FString, FString> Validation;
    const bool bBuilt = UECommandForge::FStateTreeBuilder::Build(Spec.StateTree, BuildErrors, Validation);

    Report.Errors     = BuildErrors;
    Report.Validation = Validation;

    if (!bBuilt)
    {
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::BuildFailed);
    }

    Report.bOk = true;
    Report.CreatedAssets.Add(Spec.StateTree.AssetPath);
    UECommandForge::FJsonReportWriter::Write(OutPath, Report);
    return 0;
}
