#include "Commandlets/CreateBlueprintCommandlet.h"
#include "Specs/BlueprintCreateSpecParser.h"
#include "Builders/GenericBlueprintBuilder.h"
#include "Builders/BlueprintValidationRunner.h"
#include "Reports/JsonReportWriter.h"
#include "CommandForgeTypes.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "HAL/PlatformFileManager.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

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

    FCommandForgeBlueprintCreateSpec Spec;
    TArray<FCommandForgeError> ParseErrors;
    if (!UECommandForge::FBlueprintCreateSpecParser::Parse(JsonStr, Spec, ParseErrors))
    {
        Report.Errors = ParseErrors;
        Report.bOk    = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    // 1. 에셋 존재 여부 및 allow_replace 검사
    const FString FileName = FPaths::ConvertRelativePathToFull(
        FPackageName::LongPackageNameToFilename(Spec.AssetPath, FPackageName::GetAssetPackageExtension()));
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    UPackage* ExistingPackage = FindPackage(nullptr, *Spec.AssetPath);
    const bool bAssetExists = ExistingPackage || PlatformFile.FileExists(*FileName);

    if (bAssetExists)
    {
        if (!Spec.bAllowReplace)
        {
            FCommandForgeError Err;
            Err.Code    = TEXT("BP_ALREADY_EXISTS");
            Err.Message = FString::Printf(TEXT("에셋이 이미 존재하며 allow_replace가 false입니다: %s"), *Spec.AssetPath);
            Err.Field   = TEXT("AssetPath");
            Report.Errors.Add(Err);
            Report.bOk = false;
            UECommandForge::FJsonReportWriter::Write(OutPath, Report);
            return static_cast<int32>(ECommandForgeExitCode::BuildFailed);
        }
        else
        {
            // allow_replace가 true인 경우 삭제 처리
            TArray<FCommandForgeError> DeleteErrors;
            if (!UECommandForge::FGenericBlueprintBuilder::DeleteExistingBlueprint(Spec.AssetPath, TEXT("AssetPath"), DeleteErrors))
            {
                Report.Errors = DeleteErrors;
                Report.bOk    = false;
                UECommandForge::FJsonReportWriter::Write(OutPath, Report);
                return static_cast<int32>(ECommandForgeExitCode::BuildFailed);
            }
        }
    }

    // 2. 부모 클래스 로드 및 검사
    UClass* ParentClass = UECommandForge::FGenericBlueprintBuilder::ResolveParentClass(Spec.ParentClass, { TEXT("Engine") });
    if (!ParentClass)
    {
        FCommandForgeError Err;
        Err.Code    = TEXT("PARENT_CLASS_NOT_FOUND");
        Err.Message = FString::Printf(TEXT("부모 클래스를 찾을 수 없습니다: %s"), *Spec.ParentClass);
        Err.Field   = TEXT("ParentClass");
        Report.Errors.Add(Err);
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::ValidationFailed);
    }

    Report.Validation.Add(TEXT("parent_class_loaded"), TEXT("true"));

    // AActor 계층인지 검사
    if (!ParentClass->IsChildOf(AActor::StaticClass()))
    {
        FCommandForgeError Err;
        Err.Code    = TEXT("PARENT_CLASS_NOT_ACTOR");
        Err.Message = FString::Printf(TEXT("부모 클래스가 Actor 계층이 아닙니다: %s"), *ParentClass->GetName());
        Err.Field   = TEXT("ParentClass");
        Report.Errors.Add(Err);
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::ValidationFailed);
    }

    // Blueprintable 클래스인지 검사
    if (!FKismetEditorUtilities::CanCreateBlueprintOfClass(ParentClass))
    {
        FCommandForgeError Err;
        Err.Code    = TEXT("PARENT_CLASS_NOT_BLUEPRINTABLE");
        Err.Message = FString::Printf(TEXT("부모 클래스로부터 Blueprint를 생성할 수 없습니다: %s"), *ParentClass->GetName());
        Err.Field   = TEXT("ParentClass");
        Report.Errors.Add(Err);
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::ValidationFailed);
    }

    Report.Validation.Add(TEXT("parent_class_blueprintable"), TEXT("true"));

    // 3. 블루프린트 생성
    UPackage* Package = CreatePackage(*Spec.AssetPath);
    if (!Package)
    {
        FCommandForgeError Err;
        Err.Code    = TEXT("PACKAGE_CREATE_FAILED");
        Err.Message = FString::Printf(TEXT("패키지 생성에 실패했습니다: %s"), *Spec.AssetPath);
        Err.Field   = TEXT("AssetPath");
        Report.Errors.Add(Err);
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::BuildFailed);
    }

    const FString AssetName = FPackageName::GetLongPackageAssetName(Spec.AssetPath);
    UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
        ParentClass,
        Package,
        *AssetName,
        BPTYPE_Normal,
        UBlueprint::StaticClass(),
        UBlueprintGeneratedClass::StaticClass());

    if (!Blueprint)
    {
        FCommandForgeError Err;
        Err.Code    = TEXT("BP_CREATE_FAILED");
        Err.Message = FString::Printf(TEXT("Blueprint 생성 실패: %s"), *Spec.AssetPath);
        Err.Field   = TEXT("AssetPath");
        Report.Errors.Add(Err);
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::BuildFailed);
    }

    // GC 보호 강제 등록 및 해제 가드
    Blueprint->AddToRoot();
    Package->AddToRoot();

    ON_SCOPE_EXIT
    {
        if (Blueprint)
        {
            Blueprint->RemoveFromRoot();
        }
        if (Package)
        {
            Package->RemoveFromRoot();
        }
    };

    Report.Validation.Add(TEXT("asset_created"), TEXT("true"));

    // 4. 컴파일 및 저장 검증
    TArray<FCommandForgeError> BuildErrors;
    TMap<FString, FString> ValidationMap;
    bool bSuccess = true;

    if (Spec.bCompile)
    {
        if (!UECommandForge::FBlueprintValidationRunner::Compile(Blueprint, Spec.AssetPath, TEXT("AssetPath"), BuildErrors, ValidationMap))
        {
            bSuccess = false;
        }
    }
    else
    {
        ValidationMap.Add(TEXT("compile_status"), TEXT("skipped"));
    }

    if (bSuccess && Spec.bSave)
    {
        if (!UECommandForge::FBlueprintValidationRunner::Save(Blueprint, Spec.AssetPath, TEXT("AssetPath"), BuildErrors, ValidationMap))
        {
            bSuccess = false;
        }
    }
    else if (!Spec.bSave)
    {
        ValidationMap.Add(TEXT("asset_on_disk"), TEXT("skipped"));
        ValidationMap.Add(TEXT("asset_in_registry"), TEXT("skipped"));
    }

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
        return static_cast<int32>(ECommandForgeExitCode::BuildFailed);
    }

    Report.bOk = true;
    Report.CreatedAssets.Add(Spec.AssetPath);
    UECommandForge::FJsonReportWriter::Write(OutPath, Report);
    return 0;
}
