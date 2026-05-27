#include "Commandlets/CreateAIFlowCommandlet.h"
#include "Builders/AIControllerBlueprintBuilder.h"
#include "Builders/AIFlowBinder.h"
#include "Builders/CharacterBlueprintBuilder.h"
#include "Builders/MapActorPlacer.h"
#include "Builders/StateTreeBuilder.h"
#include "Misc/FileHelper.h"
#include "Specs/AIFlowSpecParser.h"
#include "Specs/AIFlowSpecValidator.h"

namespace UECommandForge
{
    namespace
    {
        void AddCommandletError(TArray<FCommandForgeError>& OutErrors,
                                const TCHAR* Code,
                                const FString& Message,
                                const TCHAR* Field)
        {
            OutErrors.Add({ Code, Message, Field });
        }

        void MergeValidation(FCommandForgeReport& Report, const TMap<FString, FString>& Validation)
        {
            for (const TPair<FString, FString>& Entry : Validation)
            {
                Report.Validation.Add(Entry.Key, Entry.Value);
            }
        }

        bool RunStep(FCommandForgeReport& Report,
                     const FString& StepName,
                     TFunctionRef<bool(FCommandForgeStepResult&)> Func)
        {
            FCommandForgeStepResult Step;
            Step.Step = StepName;
            Step.bOk = Func(Step);

            for (const FString& AssetPath : Step.CreatedAssets)
            {
                Report.CreatedAssets.Add(AssetPath);
            }
            MergeValidation(Report, Step.Validation);
            if (!Step.bOk)
            {
                Report.Errors.Append(Step.Errors);
                Report.bRollbackAvailable = !Report.CreatedAssets.IsEmpty();
            }

            const bool bStepOk = Step.bOk;
            Report.Steps.Add(MoveTemp(Step));
            return bStepOk;
        }

        bool WriteReport(const FString& OutPath, FCommandForgeReport& Report)
        {
            if (FJsonReportWriter::Write(OutPath, Report))
            {
                return true;
            }

            AddCommandletError(Report.Errors, TEXT("REPORT_WRITE_FAILED"),
                FString::Printf(TEXT("리포트 저장 실패: %s"), *OutPath), TEXT("Output"));
            Report.bOk = false;
            return false;
        }

        bool WriteFailure(const FString& OutPath, FCommandForgeReport& Report)
        {
            Report.bOk = false;
            WriteReport(OutPath, Report);
            return false;
        }

        bool HasErrorCode(const FCommandForgeReport& Report, const TCHAR* Code)
        {
            return Report.Errors.ContainsByPredicate(
                [Code](const FCommandForgeError& Error)
                {
                    return Error.Code == Code;
                });
        }
    }

    bool FCreateAIFlowWorkflow::Execute(const FAIFlowSpec& Spec,
                                        const FString& OutPath,
                                        FCommandForgeReport& OutReport)
    {
        OutReport = FCommandForgeReport();
        OutReport.Commandlet = TEXT("CreateAIFlow");

        if (!RunStep(OutReport, TEXT("CreateCharacterBlueprint"),
            [&](FCommandForgeStepResult& Step)
            {
                const bool bBuilt = FCharacterBlueprintBuilder::Build(
                    Spec.Character, Step.Errors, Step.Validation);
                if (bBuilt)
                {
                    Step.CreatedAssets.Add(Spec.Character.AssetPath);
                }
                return bBuilt;
            }))
        {
            return WriteFailure(OutPath, OutReport);
        }

        if (!RunStep(OutReport, TEXT("CreateAIControllerBlueprint"),
            [&](FCommandForgeStepResult& Step)
            {
                const bool bBuilt = FAIControllerBlueprintBuilder::Build(
                    Spec.AIController, Step.Errors, Step.Validation);
                if (bBuilt)
                {
                    Step.CreatedAssets.Add(Spec.AIController.AssetPath);
                }
                return bBuilt;
            }))
        {
            return WriteFailure(OutPath, OutReport);
        }

        if (!RunStep(OutReport, TEXT("CreateStateTree"),
            [&](FCommandForgeStepResult& Step)
            {
                const bool bBuilt = FStateTreeBuilder::Build(
                    Spec.StateTree, Step.Errors, Step.Validation);
                if (bBuilt)
                {
                    Step.CreatedAssets.Add(Spec.StateTree.AssetPath);
                }
                return bBuilt;
            }))
        {
            return WriteFailure(OutPath, OutReport);
        }

        if (!RunStep(OutReport, TEXT("BindAIFlow"),
            [&](FCommandForgeStepResult& Step)
            {
                return FAIFlowBinder::Bind(Spec, Step.Errors, Step.Validation);
            }))
        {
            return WriteFailure(OutPath, OutReport);
        }

        if (!RunStep(OutReport, TEXT("PlaceActor"),
            [&](FCommandForgeStepResult& Step)
            {
                const bool bPlaced = FMapActorPlacer::Place(Spec.Placement, Step.Errors);
                if (bPlaced)
                {
                    OutReport.ModifiedAssets.Add(Spec.Placement.MapPath);
                }
                const bool bValid = bPlaced &&
                    FMapActorPlacer::ValidatePlacement(Spec.Placement, Step.Validation, Step.Errors);
                return bPlaced && bValid;
            }))
        {
            return WriteFailure(OutPath, OutReport);
        }

        OutReport.bOk = true;
        OutReport.bRollbackAvailable = false;
        return WriteReport(OutPath, OutReport);
    }
}

UCreateAIFlowCommandlet::UCreateAIFlowCommandlet()
{
    IsClient = false;
    IsServer = false;
    IsEditor = true;
    LogToConsole = true;
}

int32 UCreateAIFlowCommandlet::Main(const FString& Params)
{
    TArray<FString> Tokens;
    TArray<FString> Switches;
    TMap<FString, FString> ParamsMap;
    UCommandlet::ParseCommandLine(*Params, Tokens, Switches, ParamsMap);

    const FString SpecFile = ParamsMap.FindRef(TEXT("SpecFile"));
    const FString OutPath = ParamsMap.FindRef(TEXT("Output"));

    FCommandForgeReport Report;
    Report.Commandlet = TEXT("CreateAIFlow");

    if (SpecFile.IsEmpty() || OutPath.IsEmpty())
    {
        UECommandForge::AddCommandletError(Report.Errors, TEXT("MISSING_PARAM"),
            TEXT("-SpecFile 또는 -Output 파라미터 누락"), TEXT(""));
        UECommandForge::FJsonReportWriter::Write(OutPath.IsEmpty() ? TEXT("/dev/null") : OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    FString JsonStr;
    if (!FFileHelper::LoadFileToString(JsonStr, *SpecFile))
    {
        UECommandForge::AddCommandletError(Report.Errors, TEXT("SPEC_FILE_NOT_FOUND"),
            FString::Printf(TEXT("파일 없음: %s"), *SpecFile), TEXT("SpecFile"));
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    FAIFlowSpec Spec;
    TArray<FCommandForgeError> ParseErrors;
    if (!UECommandForge::FAIFlowSpecParser::Parse(JsonStr, Spec, ParseErrors))
    {
        Report.Errors = ParseErrors;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    TArray<FCommandForgeError> ValidateErrors;
    UECommandForge::FAIFlowSpecValidator::Validate(Spec, ValidateErrors);
    if (!ValidateErrors.IsEmpty())
    {
        Report.Errors = ValidateErrors;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::ValidationFailed);
    }

    const bool bOk = UECommandForge::FCreateAIFlowWorkflow::Execute(Spec, OutPath, Report);
    if (bOk)
    {
        return 0;
    }

    return UECommandForge::HasErrorCode(Report, TEXT("REPORT_WRITE_FAILED"))
        ? static_cast<int32>(ECommandForgeExitCode::EngineError)
        : static_cast<int32>(ECommandForgeExitCode::BuildFailed);
}
