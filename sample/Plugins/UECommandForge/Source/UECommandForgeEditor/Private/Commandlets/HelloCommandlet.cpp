#include "Commandlets/HelloCommandlet.h"
#include "Reports/JsonReportWriter.h"
#include "CommandForgeTypes.h"

UHelloCommandlet::UHelloCommandlet()
{
    IsClient     = false;
    IsServer     = false;
    IsEditor     = true;
    LogToConsole = true;
}

int32 UHelloCommandlet::Main(const FString& Params)
{
    TArray<FString> Tokens;
    TArray<FString> Switches;
    TMap<FString, FString> ParamsMap;
    UCommandlet::ParseCommandLine(*Params, Tokens, Switches, ParamsMap);

    const FString OutPath = ParamsMap.FindRef(TEXT("Output"));
    if (OutPath.IsEmpty())
    {
        UE_LOG(LogInit, Error, TEXT("Hello: missing -Output=<path>"));
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    FCommandForgeReport Report;
    Report.bOk        = true;
    Report.Commandlet = TEXT("Hello");
    Report.Validation.Add(TEXT("engine_boot"), TEXT("ok"));

    const bool bWrote = UECommandForge::FJsonReportWriter::Write(OutPath, Report);
    return bWrote ? 0 : static_cast<int32>(ECommandForgeExitCode::EngineError);
}
