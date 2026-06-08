#include "Commandlets/CreateBlueprintBatchCommandlet.h"

#include "Builders/BlueprintCDOPropertyApplier.h"
#include "Builders/BlueprintComponentApplier.h"
#include "Builders/BlueprintValidationRunner.h"
#include "Builders/GenericBlueprintBuilder.h"
#include "CommandForgeTypes.h"
#include "Engine/Blueprint.h"
#include "Misc/FileHelper.h"
#include "Reports/JsonReportWriter.h"
#include "Specs/BlueprintBatchSpecParser.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"
#include "Misc/Guid.h"
#include "UObject/GarbageCollection.h"

#if PLATFORM_WINDOWS
const FString NullDevice = TEXT("NUL");
#else
const FString NullDevice = TEXT("/dev/null");
#endif

UCreateBlueprintBatchCommandlet::UCreateBlueprintBatchCommandlet()
{
    IsClient = false;
    IsServer = false;
    IsEditor = true;
    LogToConsole = true;
}

int32 UCreateBlueprintBatchCommandlet::Main(const FString& Params)
{
    TArray<FString> Tokens;
    TArray<FString> Switches;
    TMap<FString, FString> ParamsMap;
    UCommandlet::ParseCommandLine(*Params, Tokens, Switches, ParamsMap);

    const FString SpecFile = ParamsMap.FindRef(TEXT("Spec"));
    const FString OutPath = ParamsMap.FindRef(TEXT("Output"));

    FCommandForgeReport Report;
    Report.Commandlet = TEXT("CreateBlueprintBatch");

    if (SpecFile.IsEmpty() || OutPath.IsEmpty())
    {
        Report.Errors.Add({ TEXT("MISSING_PARAM"),
            TEXT("-Spec 또는 -Output 파라미터 누락"), TEXT("") });
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath.IsEmpty() ? NullDevice : OutPath, Report);
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

    FBlueprintBatchSpec Spec;
    TArray<FCommandForgeError> ParseErrors;
    if (!UECommandForge::FBlueprintBatchSpecParser::Parse(JsonStr, Spec, ParseErrors))
    {
        Report.Errors = ParseErrors;
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    Report.TransactionId = Spec.TransactionId;
    Report.Validation.Add(TEXT("spec"), SpecFile);
    Report.Validation.Add(TEXT("item_count"), FString::FromInt(Spec.Items.Num()));

    struct FAssetBackupInfo
    {
        FString AssetPath;
        FString OriginalFilePath;
        FString BackupFilePath;
        bool bExistedBefore = false;
    };

    TArray<FAssetBackupInfo> Backups;
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    const FString BackupDir = FPaths::ProjectSavedDir() / TEXT("TmpBackup");
    PlatformFile.CreateDirectoryTree(*BackupDir);

    bool bPrepareOk = true;
    for (int32 Index = 0; Index < Spec.Items.Num(); ++Index)
    {
        const FBlueprintBatchItemSpec& Item = Spec.Items[Index];
        FAssetBackupInfo Backup;
        Backup.AssetPath = Item.AssetPath;
        Backup.OriginalFilePath = FPaths::ConvertRelativePathToFull(
            FPackageName::LongPackageNameToFilename(Item.AssetPath, FPackageName::GetAssetPackageExtension()));

        if (PlatformFile.FileExists(*Backup.OriginalFilePath))
        {
            Backup.bExistedBefore = true;
            Backup.BackupFilePath = BackupDir / FGuid::NewGuid().ToString() + TEXT(".uasset");
            if (!PlatformFile.CopyFile(*Backup.BackupFilePath, *Backup.OriginalFilePath))
            {
                Report.Errors.Add({ TEXT("BACKUP_FAILED"),
                    FString::Printf(TEXT("에셋 백업 실패: %s"), *Backup.OriginalFilePath), TEXT("") });
                bPrepareOk = false;
                break;
            }
        }
        Backups.Add(Backup);
    }

    if (!bPrepareOk)
    {
        for (const FAssetBackupInfo& Backup : Backups)
        {
            if (Backup.bExistedBefore && PlatformFile.FileExists(*Backup.BackupFilePath))
            {
                PlatformFile.DeleteFile(*Backup.BackupFilePath);
            }
        }
        Report.bOk = false;
        Report.bApplied = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::ValidationFailed);
    }

    int32 SuccessCount = 0;
    bool bBatchSuccess = true;

    for (int32 Index = 0; Index < Spec.Items.Num(); ++Index)
    {
        const FBlueprintBatchItemSpec& Item = Spec.Items[Index];
        const FString FieldPrefix = FString::Printf(TEXT("items[%d]"), Index);

        // 1. Generic Blueprint 생성 (컴파일과 저장은 보류)
        FBlueprintSpec CreateSpec;
        CreateSpec.AssetPath = Item.AssetPath;
        CreateSpec.ParentClass = Item.ParentClass;
        CreateSpec.bAllowReplace = Item.bAllowReplace;
        CreateSpec.bCompile = false;
        CreateSpec.bSave = false;

        UECommandForge::FGenericBlueprintBuildOptions Options;
        Options.FieldPrefix = FieldPrefix;
        Options.bAllowReplace = Item.bAllowReplace;
        Options.bCompile = false;
        Options.bSave = false;
        Options.ParentClassModules = { TEXT("Engine"), TEXT("AIModule") };

        TMap<FString, FString> StepValidation;
        if (!UECommandForge::FGenericBlueprintBuilder::Build(CreateSpec, Options, Report.Errors, StepValidation))
        {
            bBatchSuccess = false;
            break;
        }

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *Item.AssetPath);
        if (!Blueprint)
        {
            Report.Errors.Add({ TEXT("BLUEPRINT_LOAD_FAILED"),
                FString::Printf(TEXT("생성된 Blueprint 로드 실패: %s"), *Item.AssetPath),
                FieldPrefix + TEXT(".asset_path") });
            bBatchSuccess = false;
            break;
        }

        // 2. 컴포넌트 추가
        if (!UECommandForge::FBlueprintComponentApplier::Apply(
                Blueprint, Item.Components, FieldPrefix, Report.Errors, StepValidation))
        {
            bBatchSuccess = false;
            break;
        }

        // 컴포넌트 구조 변경이 있으면 중간 컴파일
        const int32 ComponentCreatedCount = FCString::Atoi(*StepValidation.FindRef(TEXT("component_created_count")));
        if (ComponentCreatedCount > 0 && Spec.bCompile)
        {
            if (!UECommandForge::FBlueprintValidationRunner::Compile(
                    Blueprint, Item.AssetPath, FieldPrefix + TEXT(".asset_path"), Report.Errors, StepValidation))
            {
                bBatchSuccess = false;
                break;
            }
        }

        // 3. CDO 프로퍼티 기본값 설정
        if (!UECommandForge::FBlueprintCDOPropertyApplier::Apply(
                Blueprint, Item.Defaults, FieldPrefix, Spec.bCompile, Report.Errors, StepValidation))
        {
            bBatchSuccess = false;
            break;
        }

        // 4. 최종 컴파일
        if (Spec.bCompile)
        {
            if (!UECommandForge::FBlueprintValidationRunner::Compile(
                    Blueprint, Item.AssetPath, FieldPrefix + TEXT(".asset_path"), Report.Errors, StepValidation))
            {
                bBatchSuccess = false;
                break;
            }
        }

        // 5. 최종 저장
        const int32 CdoChangedCount = FCString::Atoi(*StepValidation.FindRef(TEXT("cdo_changed_count")));
        const bool bChanged = ComponentCreatedCount > 0 || CdoChangedCount > 0;
        const bool bExisted = Backups[Index].bExistedBefore;

        if (Spec.bSave)
        {
            if (bChanged || !bExisted)
            {
                if (!UECommandForge::FBlueprintValidationRunner::Save(
                        Blueprint, Item.AssetPath, FieldPrefix + TEXT(".asset_path"), Report.Errors, StepValidation))
                {
                    bBatchSuccess = false;
                    break;
                }
            }
        }

        if (bExisted)
        {
            Report.ModifiedAssets.Add(Item.AssetPath);
            Report.ChangedAssets.Add(Item.AssetPath);
        }
        else
        {
            Report.CreatedAssets.Add(Item.AssetPath);
        }

        SuccessCount++;
    }

    if (!bBatchSuccess)
    {
        for (const FAssetBackupInfo& Backup : Backups)
        {
            TArray<FCommandForgeError> DeleteErrors;
            UECommandForge::FGenericBlueprintBuilder::DeleteExistingBlueprint(Backup.AssetPath, TEXT("Rollback"), DeleteErrors);

            if (Backup.bExistedBefore)
            {
                if (PlatformFile.FileExists(*Backup.BackupFilePath))
                {
                    PlatformFile.CopyFile(*Backup.OriginalFilePath, *Backup.BackupFilePath);
                }
            }
        }

        for (const FAssetBackupInfo& Backup : Backups)
        {
            if (Backup.bExistedBefore && PlatformFile.FileExists(*Backup.BackupFilePath))
            {
                PlatformFile.DeleteFile(*Backup.BackupFilePath);
            }
        }

        Report.bOk = false;
        Report.bApplied = false;
        Report.CreatedAssets.Empty();
        Report.ModifiedAssets.Empty();
        Report.ChangedAssets.Empty();
        Report.Validation.Add(TEXT("success_count"), TEXT("0"));

        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::ValidationFailed);
    }
    else
    {
        for (const FAssetBackupInfo& Backup : Backups)
        {
            if (Backup.bExistedBefore && PlatformFile.FileExists(*Backup.BackupFilePath))
            {
                PlatformFile.DeleteFile(*Backup.BackupFilePath);
            }
        }
    }

    Report.Validation.Add(TEXT("success_count"), FString::FromInt(SuccessCount));
    Report.bApplied = SuccessCount > 0;
    Report.bOk = Report.Errors.IsEmpty();

    UECommandForge::FJsonReportWriter::Write(OutPath, Report);
    return Report.bOk ? 0 : static_cast<int32>(ECommandForgeExitCode::ValidationFailed);
}
