#include "Commandlets/CreateBlueprintCommandlet.h"
#include "Specs/BlueprintCreateSpecParser.h"
#include "Builders/GenericBlueprintBuilder.h"
#include "Reports/JsonReportWriter.h"
#include "CommandForgeTypes.h"
#include "Misc/FileHelper.h"

UCreateBlueprintCommandlet::UCreateBlueprintCommandlet()
{
    IsClient     = false;
    IsServer     = false;
    IsEditor     = true;
    LogToConsole = true;
}

int32 UCreateBlueprintCommandlet::Main(const FString& Params)
{
    TArray<FString> Tokens;
    TArray<FString> Switches;
    TMap<FString, FString> ParamsMap;
    UCommandlet::ParseCommandLine(*Params, Tokens, Switches, ParamsMap);

    const FString SpecFile = ParamsMap.FindRef(TEXT("SpecFile"));
    const FString OutPath  = ParamsMap.FindRef(TEXT("Output"));

    FCommandForgeReport Report;
    Report.Commandlet = TEXT("CreateBlueprint");

    if (SpecFile.IsEmpty() || OutPath.IsEmpty())
    {
        FCommandForgeError Err;
        Err.Code    = TEXT("MISSING_PARAM");
        Err.Message = TEXT("-SpecFile 또는 -Output 파라미터 누락");
        Report.Errors.Add(Err);
        Report.bOk = false;
        if (!OutPath.IsEmpty())
        {
            UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        }
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

    FBlueprintSpec Spec;
    TArray<FCommandForgeError> ParseErrors;
    if (!UECommandForge::FBlueprintCreateSpecParser::Parse(JsonStr, Spec, ParseErrors))
    {
        Report.Errors = ParseErrors;
        Report.bOk    = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    // FGenericBlueprintBuilder::Build 위임
    UECommandForge::FGenericBlueprintBuildOptions Options;
    Options.FieldPrefix = TEXT("AssetPath");
    Options.ParentClassModules = { TEXT("Engine") };
    Options.bAllowReplace = Spec.bAllowReplace;
    Options.bRequireActorParent = true;
    Options.bRequireBlueprintable = true;
    Options.bCompile = Spec.bCompile;
    Options.bSave = Spec.bSave;

    TArray<FCommandForgeError> BuildErrors;
    TMap<FString, FString> ValidationMap;
    bool bSuccess = UECommandForge::FGenericBlueprintBuilder::Build(Spec, Options, BuildErrors, ValidationMap);

    // 결과 맵 취합
    for (const auto& Pair : ValidationMap)
    {
        Report.Validation.Add(Pair.Key, Pair.Value);
    }
    Report.Errors = BuildErrors;

    if (!bSuccess)
    {
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);

        ECommandForgeExitCode ExitCode = ECommandForgeExitCode::BuildFailed;
        if (BuildErrors.Num() > 0)
        {
            const FString& FirstErrCode = BuildErrors[0].Code;
            if (FirstErrCode == TEXT("PARENT_CLASS_NOT_FOUND") ||
                FirstErrCode == TEXT("PARENT_CLASS_NOT_ACTOR") ||
                FirstErrCode == TEXT("PARENT_CLASS_NOT_BLUEPRINTABLE"))
            {
                ExitCode = ECommandForgeExitCode::ValidationFailed;
            }
        }
        return static_cast<int32>(ExitCode);
    }

    Report.bOk = true;
    Report.CreatedAssets.Add(Spec.AssetPath);
    UECommandForge::FJsonReportWriter::Write(OutPath, Report);
    return 0;
}

