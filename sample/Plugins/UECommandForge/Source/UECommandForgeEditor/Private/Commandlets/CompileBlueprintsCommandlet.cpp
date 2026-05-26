#include "Commandlets/CompileBlueprintsCommandlet.h"
#include "Reports/JsonReportWriter.h"
#include "CommandForgeTypes.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Engine/Blueprint.h"
#include "Misc/PackageName.h"

UCompileBlueprintsCommandlet::UCompileBlueprintsCommandlet()
{
    IsClient     = false;
    IsServer     = false;
    IsEditor     = true;
    LogToConsole = true;
}

int32 UCompileBlueprintsCommandlet::Main(const FString& Params)
{
    TArray<FString> Tokens;
    TArray<FString> Switches;
    TMap<FString, FString> ParamsMap;
    UCommandlet::ParseCommandLine(*Params, Tokens, Switches, ParamsMap);

    const FString AssetPathsStr = ParamsMap.FindRef(TEXT("AssetPaths"));
    const FString OutPath       = ParamsMap.FindRef(TEXT("Output"));

    FCommandForgeReport Report;
    Report.Commandlet = TEXT("CompileBlueprints");

    if (AssetPathsStr.IsEmpty() || OutPath.IsEmpty())
    {
        FCommandForgeError Err;
        Err.Code    = TEXT("MISSING_PARAM");
        Err.Message = TEXT("-AssetPaths 또는 -Output 파라미터 누락");
        Report.Errors.Add(Err);
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath.IsEmpty() ? TEXT("/dev/null") : OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    TArray<FString> AssetPaths;
    AssetPathsStr.ParseIntoArray(AssetPaths, TEXT(","), true);

    bool bAllOk = true;
    for (const FString& AssetPath : AssetPaths)
    {
        const FString TrimmedPath = AssetPath.TrimStartAndEnd();
        UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *TrimmedPath);
        if (!BP)
        {
            Report.Validation.Add(TrimmedPath, TEXT("not_found"));
            bAllOk = false;
            continue;
        }
        FKismetEditorUtilities::CompileBlueprint(BP);
        const bool bUpToDate = (BP->Status == BS_UpToDate);
        Report.Validation.Add(TrimmedPath, bUpToDate ? TEXT("ok") : TEXT("error"));
        if (!bUpToDate)
        {
            bAllOk = false;
        }
    }

    Report.bOk = bAllOk;
    UECommandForge::FJsonReportWriter::Write(OutPath, Report);
    return bAllOk ? 0 : static_cast<int32>(ECommandForgeExitCode::BuildFailed);
}
