#include "Commandlets/ParseSpecCheckCommandlet.h"
#include "Reports/JsonReportWriter.h"
#include "Specs/AIFlowSpecParser.h"
#include "Specs/AIFlowSpecValidator.h"
#include "CommandForgeTypes.h"
#include "Misc/FileHelper.h"

UParseSpecCheckCommandlet::UParseSpecCheckCommandlet()
{
    IsClient     = false;
    IsServer     = false;
    IsEditor     = true;
    LogToConsole = true;
}

int32 UParseSpecCheckCommandlet::Main(const FString& Params)
{
    TArray<FString> Tokens;
    TArray<FString> Switches;
    TMap<FString, FString> ParamsMap;
    UCommandlet::ParseCommandLine(*Params, Tokens, Switches, ParamsMap);

    const FString SpecFile = ParamsMap.FindRef(TEXT("SpecFile"));
    const FString OutPath  = ParamsMap.FindRef(TEXT("Output"));

    FCommandForgeReport Report;
    Report.Commandlet = TEXT("ParseSpecCheck");

    if (SpecFile.IsEmpty() || OutPath.IsEmpty())
    {
        FCommandForgeError Err;
        Err.Code    = TEXT("MISSING_ARG");
        Err.Message = TEXT("-SpecFile 또는 -Output 인자가 없습니다.");
        Report.Errors.Add(Err);
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath.IsEmpty() ? TEXT("/dev/null") : OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    FString JsonContent;
    if (!FFileHelper::LoadFileToString(JsonContent, *SpecFile))
    {
        FCommandForgeError Err;
        Err.Code    = TEXT("FILE_NOT_FOUND");
        Err.Message = FString::Printf(TEXT("파일을 읽을 수 없습니다: %s"), *SpecFile);
        Report.Errors.Add(Err);
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    FAIFlowSpec Spec;
    TArray<FCommandForgeError> Errors;

    if (!UECommandForge::FAIFlowSpecParser::Parse(JsonContent, Spec, Errors))
    {
        Report.Errors = Errors;
        Report.bOk    = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    UECommandForge::FAIFlowSpecValidator::Validate(Spec, Errors);
    Report.Errors = Errors;
    Report.bOk    = Errors.IsEmpty();

    const bool bWrote = UECommandForge::FJsonReportWriter::Write(OutPath, Report);
    if (!bWrote)
    {
        return static_cast<int32>(ECommandForgeExitCode::EngineError);
    }

    return Report.bOk ? 0 : static_cast<int32>(ECommandForgeExitCode::ValidationFailed);
}
