#include "Commandlets/ValidateAIFlowCommandlet.h"
#include "Validators/AIFlowValidator.h"
#include "Reports/JsonReportWriter.h"
#include "Specs/AIFlowSpec.h"
#include "CommandForgeTypes.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"

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
    if (!FJsonObjectConverter::JsonObjectStringToUStruct(JsonStr, &Spec, 0, 0))
    {
        FCommandForgeError Err;
        Err.Code    = TEXT("SPEC_PARSE_FAILED");
        Err.Message = FString::Printf(TEXT("AIFlowSpec JSON 파싱 실패: %s"), *SpecFile);
        Report.Errors.Add(Err);
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    TMap<FString, FString> Validation;
    TArray<FCommandForgeError> ValErrors;
    const bool bValid = UECommandForge::FAIFlowValidator::Validate(Spec, Validation, ValErrors);

    Report.Errors     = ValErrors;
    Report.Validation = Validation;

    if (!bValid)
    {
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::ValidationFailed);
    }

    Report.bOk = true;
    UECommandForge::FJsonReportWriter::Write(OutPath, Report);
    return 0;
}
