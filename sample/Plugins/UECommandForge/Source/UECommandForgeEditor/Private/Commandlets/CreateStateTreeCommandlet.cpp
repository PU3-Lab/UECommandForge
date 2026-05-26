#include "Commandlets/CreateStateTreeCommandlet.h"
#include "Builders/StateTreeBuilder.h"
#include "Reports/JsonReportWriter.h"
#include "Specs/StateTreeSpec.h"
#include "CommandForgeTypes.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"

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

    FStateTreeSpec Spec;
    if (!FJsonObjectConverter::JsonObjectStringToUStruct(JsonStr, &Spec, 0, 0))
    {
        FCommandForgeError Err;
        Err.Code    = TEXT("SPEC_PARSE_FAILED");
        Err.Message = FString::Printf(TEXT("StateTreeSpec JSON 파싱 실패: %s"), *SpecFile);
        Report.Errors.Add(Err);
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    if (Spec.AssetPath.IsEmpty())
    {
        FCommandForgeError Err;
        Err.Code    = TEXT("MISSING_ASSET_PATH");
        Err.Message = TEXT("Spec에 assetPath가 없습니다");
        Err.Field   = TEXT("assetPath");
        Report.Errors.Add(Err);
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::ValidationFailed);
    }

    TArray<FCommandForgeError> BuildErrors;
    const bool bBuilt = UECommandForge::FStateTreeBuilder::Build(Spec, BuildErrors);

    Report.Errors = BuildErrors;

    if (!bBuilt)
    {
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::BuildFailed);
    }

    Report.bOk = true;
    Report.CreatedAssets.Add(Spec.AssetPath);
    UECommandForge::FJsonReportWriter::Write(OutPath, Report);
    return 0;
}
