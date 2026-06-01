#include "Commandlets/SetBlueprintDefaultsCommandlet.h"

#include "Builders/BlueprintCDOPropertyApplier.h"
#include "Builders/BlueprintComponentApplier.h"
#include "Builders/BlueprintValidationRunner.h"
#include "CommandForgeTypes.h"
#include "Engine/Blueprint.h"
#include "Misc/FileHelper.h"
#include "Reports/JsonReportWriter.h"
#include "Specs/BlueprintDefaultsSpecParser.h"

USetBlueprintDefaultsCommandlet::USetBlueprintDefaultsCommandlet()
{
    IsClient = false;
    IsServer = false;
    IsEditor = true;
    LogToConsole = true;
}

int32 USetBlueprintDefaultsCommandlet::Main(const FString& Params)
{
    TArray<FString> Tokens;
    TArray<FString> Switches;
    TMap<FString, FString> ParamsMap;
    UCommandlet::ParseCommandLine(*Params, Tokens, Switches, ParamsMap);

    const FString SpecFile = ParamsMap.FindRef(TEXT("Spec"));
    const FString OutPath = ParamsMap.FindRef(TEXT("Output"));

    FCommandForgeReport Report;
    Report.Commandlet = TEXT("SetBlueprintDefaults");

    if (SpecFile.IsEmpty() || OutPath.IsEmpty())
    {
        Report.Errors.Add({ TEXT("MISSING_PARAM"),
            TEXT("-Spec 또는 -Output 파라미터 누락"), TEXT("") });
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath.IsEmpty() ? TEXT("/dev/null") : OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    FString JsonStr;
    if (!FFileHelper::LoadFileToString(JsonStr, *SpecFile))
    {
        Report.Errors.Add({ TEXT("SPEC_FILE_NOT_FOUND"),
            FString::Printf(TEXT("파일 없음: %s"), *SpecFile), TEXT("Spec") });
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    FBlueprintDefaultsSpec Spec;
    TArray<FCommandForgeError> ParseErrors;
    if (!UECommandForge::FBlueprintDefaultsSpecParser::ParseJson(JsonStr, Spec, ParseErrors))
    {
        Report.Errors = ParseErrors;
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    Report.TransactionId = Spec.TransactionId;
    Report.Validation.Add(TEXT("spec"), SpecFile);
    Report.Validation.Add(TEXT("asset_path"), Spec.AssetPath);

    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *Spec.AssetPath);
    if (!Blueprint)
    {
        Report.Errors.Add({ TEXT("BLUEPRINT_NOT_FOUND"),
            FString::Printf(TEXT("Blueprint 에셋을 찾을 수 없습니다: %s"), *Spec.AssetPath),
            TEXT("blueprint.asset_path") });
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::ValidationFailed);
    }

    TArray<UECommandForge::FBlueprintComponentSpec> Components;
    for (const FBlueprintDefaultComponentSpec& ComponentSpec : Spec.Components)
    {
        UECommandForge::FBlueprintComponentSpec Component;
        Component.Name = ComponentSpec.Name;
        Component.Class = ComponentSpec.Class;
        Components.Add(Component);
    }

    TArray<UECommandForge::FBlueprintCDOPropertyAssignment> Defaults;
    for (const FBlueprintDefaultPropertySpec& DefaultSpec : Spec.Defaults)
    {
        UECommandForge::FBlueprintCDOPropertyAssignment Assignment;
        Assignment.PropertyName = DefaultSpec.Property;
        Assignment.Value = DefaultSpec.Value;
        Defaults.Add(Assignment);
    }

    if (Spec.bCompile &&
        !UECommandForge::FBlueprintValidationRunner::Compile(
            Blueprint, Spec.AssetPath, TEXT("blueprint.asset_path"), Report.Errors, Report.Validation))
    {
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::ValidationFailed);
    }

    if (!UECommandForge::FBlueprintComponentApplier::Apply(
            Blueprint, Components, TEXT("blueprint"), Report.Errors, Report.Validation))
    {
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::ValidationFailed);
    }

    if (!Components.IsEmpty() && Spec.bCompile &&
        !UECommandForge::FBlueprintValidationRunner::Compile(
            Blueprint, Spec.AssetPath, TEXT("blueprint.asset_path"), Report.Errors, Report.Validation))
    {
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::ValidationFailed);
    }

    if (!UECommandForge::FBlueprintCDOPropertyApplier::Apply(
            Blueprint, Defaults, TEXT("blueprint"), Spec.bCompile, Report.Errors, Report.Validation))
    {
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::ValidationFailed);
    }

    const int32 ComponentCreatedCount = FCString::Atoi(*Report.Validation.FindRef(TEXT("component_created_count")));
    const int32 CdoChangedCount = FCString::Atoi(*Report.Validation.FindRef(TEXT("cdo_changed_count")));
    const bool bChanged = ComponentCreatedCount > 0 || CdoChangedCount > 0;
    Report.Validation.Add(TEXT("changed"), bChanged ? TEXT("true") : TEXT("false"));

    if (Spec.bSave && bChanged &&
        !UECommandForge::FBlueprintValidationRunner::Save(
            Blueprint, Spec.AssetPath, TEXT("blueprint.asset_path"), Report.Errors, Report.Validation))
    {
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::ValidationFailed);
    }

    if (bChanged)
    {
        Report.ModifiedAssets.Add(Spec.AssetPath);
        Report.ChangedAssets.Add(Spec.AssetPath);
    }
    Report.bApplied = bChanged;
    Report.bOk = true;
    UECommandForge::FJsonReportWriter::Write(OutPath, Report);
    return 0;
}
