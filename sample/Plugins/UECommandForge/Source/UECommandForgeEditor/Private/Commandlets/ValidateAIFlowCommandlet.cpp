#include "Commandlets/ValidateAIFlowCommandlet.h"
#include "CommandForgeTypes.h"
#include "Misc/FileHelper.h"
#include "Reports/JsonReportWriter.h"
#include "Specs/AIFlowSpecParser.h"
#include "Specs/AIFlowSpecValidator.h"
#include "Validators/AIFlowValidator.h"

UValidateAIFlowCommandlet::UValidateAIFlowCommandlet()
{
    IsClient     = false;
    IsServer     = false;
    IsEditor     = true;
    LogToConsole = true;
}

int32 UValidateAIFlowCommandlet::Main(const FString& Params)
{
    TArray<FString> Tokens;
    TArray<FString> Switches;
    TMap<FString, FString> ParamsMap;
    UCommandlet::ParseCommandLine(*Params, Tokens, Switches, ParamsMap);

    const FString SpecFile = ParamsMap.FindRef(TEXT("SpecFile"));
    const FString OutPath  = ParamsMap.FindRef(TEXT("Output"));

    FCommandForgeReport Report;
    Report.Commandlet = TEXT("ValidateAIFlow");

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
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    TArray<FCommandForgeError> ValidateErrors;
    UECommandForge::FAIFlowSpecValidator::Validate(Spec, ValidateErrors);
    if (!ValidateErrors.IsEmpty())
    {
        Report.Errors = ValidateErrors;
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::ValidationFailed);
    }

    TMap<FString, FString> Validation;
    TArray<FCommandForgeError> FlowErrors;
    const bool bValid = UECommandForge::FAIFlowValidator::Validate(Spec, Validation, FlowErrors);

    Report.Errors = FlowErrors;
    Report.Validation = Validation;
    Report.bOk = bValid;
    UECommandForge::FJsonReportWriter::Write(OutPath, Report);
    return bValid ? 0 : static_cast<int32>(ECommandForgeExitCode::ValidationFailed);
}
