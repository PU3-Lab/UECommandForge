#include "Misc/AutomationTest.h"
#include "Commandlets/CreateProjectFoldersCommandlet.h"
#include "CommandForgeTypes.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCreateProjectFoldersDryRunTest,
    "UECommandForge.Commandlets.CreateProjectFolders.DryRunDoesNotCreateFolders",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCreateProjectFoldersApplyTest,
    "UECommandForge.Commandlets.CreateProjectFolders.ApplyCreatesFoldersAndRollbackPlan",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCreateProjectFoldersRejectsUnsafePathTest,
    "UECommandForge.Commandlets.CreateProjectFolders.RejectsUnsafeFolderPath",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
    bool LoadCreateProjectFoldersJsonObject(const FString& Path, TSharedPtr<FJsonObject>& OutObject)
    {
        FString Content;
        if (!FFileHelper::LoadFileToString(Content, *Path))
        {
            return false;
        }

        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
        return FJsonSerializer::Deserialize(Reader, OutObject) && OutObject.IsValid();
    }

    bool SaveCreateProjectFoldersSpec(
        const FString& Path, const FString& TransactionId, const TArray<FString>& Folders)
    {
        TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
        RootObject->SetStringField(TEXT("version"), TEXT("1"));
        RootObject->SetStringField(TEXT("kind"), TEXT("project_folders"));
        RootObject->SetStringField(TEXT("transaction_id"), TransactionId);

        TArray<TSharedPtr<FJsonValue>> FolderValues;
        for (const FString& Folder : Folders)
        {
            FolderValues.Add(MakeShared<FJsonValueString>(Folder));
        }
        RootObject->SetArrayField(TEXT("folders"), FolderValues);

        FString Serialized;
        const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
        FJsonSerializer::Serialize(RootObject, Writer);
        FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FPaths::GetPath(Path));
        return FFileHelper::SaveStringToFile(Serialized, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    }

    FString ProjectFolderDiskPath(const FString& LongPackageName)
    {
        return FPackageName::LongPackageNameToFilename(LongPackageName);
    }
}

bool FCreateProjectFoldersDryRunTest::RunTest(const FString& Parameters)
{
    const FString SpecPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("UECommandForge"), TEXT("Reports"), TEXT("test_project_folders_dry_run.json"));
    const FString ReportPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("UECommandForge"), TEXT("Reports"), TEXT("test_project_folders_dry_run_report.json"));
    const FString RootFolder = TEXT("/Game/Tests/ProjectFoldersDryRun");

    IFileManager::Get().Delete(*SpecPath);
    IFileManager::Get().Delete(*ReportPath);
    IFileManager::Get().DeleteDirectory(*ProjectFolderDiskPath(RootFolder), false, true);

    TestTrue(TEXT("dry-run folder spec 저장"), SaveCreateProjectFoldersSpec(SpecPath,
        TEXT("tx-project-folders-dry-run"), {
            RootFolder / TEXT("Blueprints"),
            RootFolder / TEXT("Maps")
        }));

    UCreateProjectFoldersCommandlet* Commandlet = NewObject<UCreateProjectFoldersCommandlet>();
    const int32 ExitCode = Commandlet->Main(FString::Printf(
        TEXT("-Spec=\"%s\" -Output=\"%s\""), *SpecPath, *ReportPath));

    TestEqual(TEXT("dry-run exit code"), ExitCode, 0);
    TestFalse(TEXT("dry-run does not create root"),
        FPaths::DirectoryExists(ProjectFolderDiskPath(RootFolder)));

    TSharedPtr<FJsonObject> RootObject;
    TestTrue(TEXT("dry-run report JSON 파싱"),
        LoadCreateProjectFoldersJsonObject(ReportPath, RootObject));
    TestTrue(TEXT("dry-run ok true"), RootObject->GetBoolField(TEXT("ok")));
    TestTrue(TEXT("dry-run flag true"), RootObject->GetBoolField(TEXT("dry_run")));
    TestFalse(TEXT("dry-run not applied"), RootObject->GetBoolField(TEXT("applied")));
    TestEqual(TEXT("folder count"), RootObject->GetObjectField(TEXT("validation"))->GetStringField(TEXT("folder_count")),
        TEXT("2"));
    return true;
}

bool FCreateProjectFoldersApplyTest::RunTest(const FString& Parameters)
{
    const FString SpecPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("UECommandForge"), TEXT("Reports"), TEXT("test_project_folders_apply.json"));
    const FString ReportPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("UECommandForge"), TEXT("Reports"), TEXT("test_project_folders_apply_report.json"));
    const FString RootFolder = TEXT("/Game/Tests/ProjectFoldersApply");

    IFileManager::Get().Delete(*SpecPath);
    IFileManager::Get().Delete(*ReportPath);
    IFileManager::Get().DeleteDirectory(*ProjectFolderDiskPath(RootFolder), false, true);

    TestTrue(TEXT("apply folder spec 저장"), SaveCreateProjectFoldersSpec(SpecPath,
        TEXT("tx-project-folders-apply"), {
            RootFolder / TEXT("Blueprints"),
            RootFolder / TEXT("Maps")
        }));

    UCreateProjectFoldersCommandlet* Commandlet = NewObject<UCreateProjectFoldersCommandlet>();
    const int32 ExitCode = Commandlet->Main(FString::Printf(
        TEXT("-Spec=\"%s\" -Output=\"%s\" -Apply"), *SpecPath, *ReportPath));

    TestEqual(TEXT("apply exit code"), ExitCode, 0);
    TestTrue(TEXT("apply created blueprints folder"),
        FPaths::DirectoryExists(ProjectFolderDiskPath(RootFolder / TEXT("Blueprints"))));
    TestTrue(TEXT("apply created maps folder"),
        FPaths::DirectoryExists(ProjectFolderDiskPath(RootFolder / TEXT("Maps"))));

    TSharedPtr<FJsonObject> RootObject;
    TestTrue(TEXT("apply report JSON 파싱"),
        LoadCreateProjectFoldersJsonObject(ReportPath, RootObject));
    TestTrue(TEXT("apply ok true"), RootObject->GetBoolField(TEXT("ok")));
    TestFalse(TEXT("apply dry_run false"), RootObject->GetBoolField(TEXT("dry_run")));
    TestTrue(TEXT("apply applied true"), RootObject->GetBoolField(TEXT("applied")));
    TestTrue(TEXT("rollback available"), RootObject->GetBoolField(TEXT("rollback_available")));
    TestTrue(TEXT("rollback plan exists"),
        FPaths::FileExists(RootObject->GetStringField(TEXT("rollback_plan_path"))));
    TestEqual(TEXT("post validation status"),
        RootObject->GetObjectField(TEXT("post_validation"))->GetStringField(TEXT("status")),
        TEXT("passed"));

    IFileManager::Get().DeleteDirectory(*ProjectFolderDiskPath(RootFolder), false, true);
    return true;
}

bool FCreateProjectFoldersRejectsUnsafePathTest::RunTest(const FString& Parameters)
{
    const FString SpecPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("UECommandForge"), TEXT("Reports"), TEXT("test_project_folders_invalid.json"));
    const FString ReportPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("UECommandForge"), TEXT("Reports"), TEXT("test_project_folders_invalid_report.json"));

    IFileManager::Get().Delete(*SpecPath);
    IFileManager::Get().Delete(*ReportPath);
    TestTrue(TEXT("invalid folder spec 저장"), SaveCreateProjectFoldersSpec(SpecPath,
        TEXT("tx-project-folders-invalid"), {
            TEXT("/Engine/Unsafe"),
            TEXT("/Game/Tests/*")
        }));

    UCreateProjectFoldersCommandlet* Commandlet = NewObject<UCreateProjectFoldersCommandlet>();
    const int32 ExitCode = Commandlet->Main(FString::Printf(
        TEXT("-Spec=\"%s\" -Output=\"%s\" -Apply"), *SpecPath, *ReportPath));

    TestEqual(TEXT("invalid exit code"), ExitCode,
        static_cast<int32>(ECommandForgeExitCode::ValidationFailed));

    TSharedPtr<FJsonObject> RootObject;
    TestTrue(TEXT("invalid report JSON 파싱"),
        LoadCreateProjectFoldersJsonObject(ReportPath, RootObject));
    TestFalse(TEXT("invalid ok false"), RootObject->GetBoolField(TEXT("ok")));
    TestFalse(TEXT("invalid not applied"), RootObject->GetBoolField(TEXT("applied")));

    const TArray<TSharedPtr<FJsonValue>>* Issues = nullptr;
    TestTrue(TEXT("invalid issues 배열 존재"),
        RootObject->TryGetArrayField(TEXT("issues"), Issues));
    TestTrue(TEXT("invalid issues count"), Issues != nullptr && Issues->Num() == 2);
    return true;
}
