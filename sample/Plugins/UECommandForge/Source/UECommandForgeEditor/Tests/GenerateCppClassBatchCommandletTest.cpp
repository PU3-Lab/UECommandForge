#include "Misc/AutomationTest.h"
#include "Commandlets/GenerateCppClassBatchCommandlet.h"
#include "CommandForgeTypes.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGenerateCppClassBatchDryRunTest,
    "UECommandForge.Commandlets.GenerateCppClassBatch.DryRunReportsPreview",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGenerateCppClassBatchApplyTest,
    "UECommandForge.Commandlets.GenerateCppClassBatch.ApplyWritesFilesAndChecksums",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGenerateCppClassBatchRollbackTest,
    "UECommandForge.Commandlets.GenerateCppClassBatch.RollbackOnError",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGenerateCppClassBatchDuplicateTargetTest,
    "UECommandForge.Commandlets.GenerateCppClassBatch.RejectsDuplicateTargets",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGenerateCppClassBatchBuildCsRollbackTest,
    "UECommandForge.Commandlets.GenerateCppClassBatch.RollbackBuildCsOnError",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGenerateCppClassBatchDuplicateClassSymbolTest,
    "UECommandForge.Commandlets.GenerateCppClassBatch.RejectsDuplicateClassSymbols",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
    bool LoadBatchJsonObject(const FString& Path, TSharedPtr<FJsonObject>& OutObject)
    {
        FString Content;
        if (!FFileHelper::LoadFileToString(Content, *Path))
        {
            return false;
        }

        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
        return FJsonSerializer::Deserialize(Reader, OutObject) && OutObject.IsValid();
    }

    FString RuntimeModuleRoot()
    {
        return FPaths::ConvertRelativePathToFull(FPaths::Combine(
            FPaths::ProjectPluginsDir(), TEXT("UECommandForge"), TEXT("Source"), TEXT("UECommandForgeRuntime")));
    }

    FString PluginModuleRoot(const FString& ModuleName)
    {
        return FPaths::ConvertRelativePathToFull(FPaths::Combine(
            FPaths::ProjectPluginsDir(), TEXT("UECommandForge"), TEXT("Source"), ModuleName));
    }

    FString GeneratedHeaderPath(const FString& OutputSubPath, const FString& ClassName)
    {
        return FPaths::Combine(RuntimeModuleRoot(), TEXT("Public"), OutputSubPath, ClassName + TEXT(".h"));
    }

    FString GeneratedSourcePath(const FString& OutputSubPath, const FString& ClassName)
    {
        return FPaths::Combine(RuntimeModuleRoot(), TEXT("Private"), OutputSubPath, ClassName + TEXT(".cpp"));
    }

    bool SaveBatchSpec(const FString& Path, const FString& TransactionId,
        const TArray<FString>& ClassNames, const FString& OutputSubPath,
        const FString& ModuleName = TEXT("UECommandForgeRuntime"),
        const FString& ExtraPublicDependency = TEXT(""), const FString& ExtraPrivateDependency = TEXT(""))
    {
        TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
        RootObject->SetStringField(TEXT("version"), TEXT("1"));
        RootObject->SetStringField(TEXT("kind"), TEXT("cpp_class_batch"));
        RootObject->SetStringField(TEXT("transaction_id"), TransactionId);

        TArray<TSharedPtr<FJsonValue>> ClassesArray;
        for (const FString& ClassName : ClassNames)
        {
            TSharedRef<FJsonObject> ClassObj = MakeShared<FJsonObject>();
            ClassObj->SetStringField(TEXT("name"), ClassName);
            ClassObj->SetStringField(TEXT("type"), TEXT("Actor"));
            ClassObj->SetStringField(TEXT("module"), ModuleName);
            ClassObj->SetStringField(TEXT("output_path"), OutputSubPath);
            ClassObj->SetStringField(TEXT("base_class"), TEXT("AActor"));
            ClassObj->SetBoolField(TEXT("blueprint_type"), true);
            ClassObj->SetBoolField(TEXT("tick"), false);
            ClassesArray.Add(MakeShared<FJsonValueObject>(ClassObj));
        }
        RootObject->SetArrayField(TEXT("classes"), ClassesArray);

        TArray<TSharedPtr<FJsonValue>> Includes;
        Includes.Add(MakeShared<FJsonValueString>(TEXT("CoreMinimal.h")));
        RootObject->SetArrayField(TEXT("includes"), Includes);

        TSharedRef<FJsonObject> BuildDependenciesObject = MakeShared<FJsonObject>();
        TArray<TSharedPtr<FJsonValue>> PublicDependencies;
        PublicDependencies.Add(MakeShared<FJsonValueString>(TEXT("Core")));
        PublicDependencies.Add(MakeShared<FJsonValueString>(TEXT("CoreUObject")));
        PublicDependencies.Add(MakeShared<FJsonValueString>(TEXT("Engine")));
        if (!ExtraPublicDependency.IsEmpty())
        {
            PublicDependencies.Add(MakeShared<FJsonValueString>(ExtraPublicDependency));
        }
        BuildDependenciesObject->SetArrayField(TEXT("public"), PublicDependencies);

        TArray<TSharedPtr<FJsonValue>> PrivateDependencies;
        if (!ExtraPrivateDependency.IsEmpty())
        {
            PrivateDependencies.Add(MakeShared<FJsonValueString>(ExtraPrivateDependency));
        }
        BuildDependenciesObject->SetArrayField(TEXT("private"), PrivateDependencies);
        RootObject->SetObjectField(TEXT("build_dependencies"), BuildDependenciesObject);

        FString Serialized;
        const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
        FJsonSerializer::Serialize(RootObject, Writer);
        FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FPaths::GetPath(Path));
        return FFileHelper::SaveStringToFile(Serialized, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    }

    bool SaveRollbackBatchSpec(const FString& Path, const FString& TransactionId,
        const FString& ValidName, const FString& ValidName2,
        const FString& OutputSubPath, const FString& OutputSubPathFail,
        const FString& ModuleName = TEXT("UECommandForgeRuntime"))
    {
        TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
        RootObject->SetStringField(TEXT("version"), TEXT("1"));
        RootObject->SetStringField(TEXT("kind"), TEXT("cpp_class_batch"));
        RootObject->SetStringField(TEXT("transaction_id"), TransactionId);

        TArray<TSharedPtr<FJsonValue>> ClassesArray;

        TSharedRef<FJsonObject> ClassObj1 = MakeShared<FJsonObject>();
        ClassObj1->SetStringField(TEXT("name"), ValidName);
        ClassObj1->SetStringField(TEXT("type"), TEXT("Actor"));
        ClassObj1->SetStringField(TEXT("module"), ModuleName);
        ClassObj1->SetStringField(TEXT("output_path"), OutputSubPath);
        ClassObj1->SetStringField(TEXT("base_class"), TEXT("AActor"));
        ClassObj1->SetBoolField(TEXT("blueprint_type"), true);
        ClassesArray.Add(MakeShared<FJsonValueObject>(ClassObj1));

        TSharedRef<FJsonObject> ClassObj2 = MakeShared<FJsonObject>();
        ClassObj2->SetStringField(TEXT("name"), ValidName2);
        ClassObj2->SetStringField(TEXT("type"), TEXT("Actor"));
        ClassObj2->SetStringField(TEXT("module"), ModuleName);
        ClassObj2->SetStringField(TEXT("output_path"), OutputSubPathFail);
        ClassObj2->SetStringField(TEXT("base_class"), TEXT("AActor"));
        ClassObj2->SetBoolField(TEXT("blueprint_type"), true);
        ClassesArray.Add(MakeShared<FJsonValueObject>(ClassObj2));

        RootObject->SetArrayField(TEXT("classes"), ClassesArray);

        TArray<TSharedPtr<FJsonValue>> Includes;
        Includes.Add(MakeShared<FJsonValueString>(TEXT("CoreMinimal.h")));
        RootObject->SetArrayField(TEXT("includes"), Includes);

        FString Serialized;
        const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
        FJsonSerializer::Serialize(RootObject, Writer);
        FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FPaths::GetPath(Path));
        return FFileHelper::SaveStringToFile(Serialized, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    }

    void DeleteGeneratedBatchFiles(const FString& OutputSubPath, const TArray<FString>& ClassNames)
    {
        for (const FString& ClassName : ClassNames)
        {
            IFileManager::Get().Delete(*GeneratedHeaderPath(OutputSubPath, ClassName));
            IFileManager::Get().Delete(*GeneratedSourcePath(OutputSubPath, ClassName));
        }
        if (!ClassNames.IsEmpty())
        {
            const FString HeaderDir = FPaths::GetPath(GeneratedHeaderPath(OutputSubPath, ClassNames[0]));
            const FString SourceDir = FPaths::GetPath(GeneratedSourcePath(OutputSubPath, ClassNames[0]));
            IFileManager::Get().DeleteDirectory(*HeaderDir, false, true);
            IFileManager::Get().DeleteDirectory(*SourceDir, false, true);
        }
        IFileManager::Get().DeleteDirectory(
            *FPaths::Combine(RuntimeModuleRoot(), TEXT("Public"), TEXT("GeneratedBatchTests")), false, true);
        IFileManager::Get().DeleteDirectory(
            *FPaths::Combine(RuntimeModuleRoot(), TEXT("Private"), TEXT("GeneratedBatchTests")), false, true);
    }

    bool CreateDirectoryBlockerFile(const FString& ModuleRoot, const FString& SubPathDir)
    {
        const FString BlockerPath = FPaths::Combine(ModuleRoot, TEXT("Public"), SubPathDir);
        IFileManager::Get().Delete(*BlockerPath);
        FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FPaths::GetPath(BlockerPath));
        return FFileHelper::SaveStringToFile(TEXT("DirectoryBlocker"), *BlockerPath);
    }

    void DeleteDirectoryBlockerFile(const FString& ModuleRoot, const FString& SubPathDir)
    {
        const FString BlockerPath = FPaths::Combine(ModuleRoot, TEXT("Public"), SubPathDir);
        IFileManager::Get().Delete(*BlockerPath);
        IFileManager::Get().DeleteDirectory(*FPaths::GetPath(BlockerPath), false, true);
    }

    FString BuildCsTestModuleRoot()
    {
        return PluginModuleRoot(TEXT("CodexBatchBuildCsTestModule"));
    }

    void DeleteBuildCsTestModule()
    {
        IFileManager::Get().DeleteDirectory(*BuildCsTestModuleRoot(), false, true);
    }

    bool CreateBuildCsTestModule()
    {
        DeleteBuildCsTestModule();
        const FString ModuleRoot = BuildCsTestModuleRoot();
        FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*ModuleRoot);
        const FString BuildCsPath = FPaths::Combine(ModuleRoot, TEXT("CodexBatchBuildCsTestModule.Build.cs"));
        const FString BuildCs = TEXT(R"(using UnrealBuildTool;

public class CodexBatchBuildCsTestModule : ModuleRules
{
    public CodexBatchBuildCsTestModule(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core" });
    }
}
)");
        return FFileHelper::SaveStringToFile(BuildCs, *BuildCsPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    }
}

bool FGenerateCppClassBatchDryRunTest::RunTest(const FString& Parameters)
{
    const FString SpecPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("CodexReports"), TEXT("test_cpp_class_batch_dry_run.json"));
    const FString ReportPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("CodexReports"), TEXT("test_cpp_class_batch_dry_run_report.json"));

    const TArray<FString> ClassNames = { TEXT("CodexBatchDryRunWeapon"), TEXT("CodexBatchDryRunArmor") };
    const FString OutputSubPath = TEXT("GeneratedBatchTests/DryRun");

    IFileManager::Get().Delete(*SpecPath);
    IFileManager::Get().Delete(*ReportPath);
    DeleteGeneratedBatchFiles(OutputSubPath, ClassNames);

    TestTrue(TEXT("C++ class batch spec 저장"), SaveBatchSpec(SpecPath,
        TEXT("tx-cpp-class-batch-dry-run"), ClassNames, OutputSubPath));

    UGenerateCppClassBatchCommandlet* Commandlet = NewObject<UGenerateCppClassBatchCommandlet>();
    const int32 ExitCode = Commandlet->Main(FString::Printf(
        TEXT("-Spec=\"%s\" -Output=\"%s\""), *SpecPath, *ReportPath));

    TestEqual(TEXT("dry-run exit code"), ExitCode, 0);

    TSharedPtr<FJsonObject> RootObject;
    TestTrue(TEXT("dry-run report JSON 파싱"),
        LoadBatchJsonObject(ReportPath, RootObject));
    TestTrue(TEXT("dry-run ok true"), RootObject->GetBoolField(TEXT("ok")));
    TestTrue(TEXT("dry-run flag true"), RootObject->GetBoolField(TEXT("dry_run")));
    TestFalse(TEXT("dry-run not applied"), RootObject->GetBoolField(TEXT("applied")));

    for (const FString& ClassName : ClassNames)
    {
        TestFalse(TEXT("dry-run did not write header"),
            FPaths::FileExists(GeneratedHeaderPath(OutputSubPath, ClassName)));
    }
    return true;
}

bool FGenerateCppClassBatchApplyTest::RunTest(const FString& Parameters)
{
    const FString SpecPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("CodexReports"), TEXT("test_cpp_class_batch_apply.json"));
    const FString ReportPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("CodexReports"), TEXT("test_cpp_class_batch_apply_report.json"));

    const TArray<FString> ClassNames = { TEXT("CodexBatchApplyWeapon"), TEXT("CodexBatchApplyArmor") };
    const FString OutputSubPath = TEXT("GeneratedBatchTests/Apply");

    IFileManager::Get().Delete(*SpecPath);
    IFileManager::Get().Delete(*ReportPath);
    DeleteGeneratedBatchFiles(OutputSubPath, ClassNames);

    TestTrue(TEXT("C++ class batch apply spec 저장"), SaveBatchSpec(SpecPath,
        TEXT("tx-cpp-class-batch-apply"), ClassNames, OutputSubPath));

    UGenerateCppClassBatchCommandlet* Commandlet = NewObject<UGenerateCppClassBatchCommandlet>();
    const int32 ExitCode = Commandlet->Main(FString::Printf(
        TEXT("-Spec=\"%s\" -Output=\"%s\" -Apply"), *SpecPath, *ReportPath));

    TestEqual(TEXT("apply exit code"), ExitCode, 0);

    for (const FString& ClassName : ClassNames)
    {
        TestTrue(TEXT("header written"), FPaths::FileExists(GeneratedHeaderPath(OutputSubPath, ClassName)));
        TestTrue(TEXT("source written"), FPaths::FileExists(GeneratedSourcePath(OutputSubPath, ClassName)));
    }

    TSharedPtr<FJsonObject> RootObject;
    TestTrue(TEXT("apply report JSON 파싱"),
        LoadBatchJsonObject(ReportPath, RootObject));
    TestTrue(TEXT("apply ok true"), RootObject->GetBoolField(TEXT("ok")));
    TestTrue(TEXT("apply applied true"), RootObject->GetBoolField(TEXT("applied")));
    TestEqual(TEXT("changed file count"), RootObject->GetArrayField(TEXT("changed_files")).Num(), 4);

    DeleteGeneratedBatchFiles(OutputSubPath, ClassNames);
    return true;
}

bool FGenerateCppClassBatchRollbackTest::RunTest(const FString& Parameters)
{
    const FString SpecPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("CodexReports"), TEXT("test_cpp_class_batch_rollback.json"));
    const FString ReportPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("CodexReports"), TEXT("test_cpp_class_batch_rollback_report.json"));

    const FString ValidClassName1 = TEXT("CodexRollbackValidClassA");
    const FString ValidClassName2 = TEXT("CodexRollbackValidClassB");

    const FString OutputSubPath = TEXT("GeneratedBatchTests/Rollback/Success");
    // 2번 클래스의 실제 파일 쓰기를 차단할 디렉토리 경로 지정
    const FString OutputSubPathFail = TEXT("GeneratedBatchTests/Rollback/FailedDir/ClassB");

    IFileManager::Get().Delete(*SpecPath);
    IFileManager::Get().Delete(*ReportPath);
    DeleteGeneratedBatchFiles(OutputSubPath, { ValidClassName1 });
    DeleteGeneratedBatchFiles(TEXT("GeneratedBatchTests/Rollback/FailedDir"), { ValidClassName2 });

    // Seam: FailedDir를 디렉토리가 아닌 '일반 파일'로 미리 생성해 둠으로써 실제 apply 시점의 쓰기 실패 유도
    TestTrue(TEXT("Directory blocker file 생성"), CreateDirectoryBlockerFile(RuntimeModuleRoot(), TEXT("GeneratedBatchTests/Rollback/FailedDir")));

    TestTrue(TEXT("C++ class batch rollback spec 저장"), SaveRollbackBatchSpec(SpecPath,
        TEXT("tx-cpp-class-batch-rollback"), ValidClassName1, ValidClassName2, OutputSubPath, OutputSubPathFail));

    UGenerateCppClassBatchCommandlet* Commandlet = NewObject<UGenerateCppClassBatchCommandlet>();
    const int32 ExitCode = Commandlet->Main(FString::Printf(
        TEXT("-Spec=\"%s\" -Output=\"%s\" -Apply"), *SpecPath, *ReportPath));

    TestEqual(TEXT("apply exit code should be ValidationFailed"), ExitCode,
        static_cast<int32>(ECommandForgeExitCode::ValidationFailed));

    TSharedPtr<FJsonObject> RootObject;
    TestTrue(TEXT("apply report JSON 파싱"), LoadBatchJsonObject(ReportPath, RootObject));
    TestFalse(TEXT("apply ok should be false"), RootObject->GetBoolField(TEXT("ok")));
    TestFalse(TEXT("apply applied should be false"), RootObject->GetBoolField(TEXT("applied")));

    // 롤백 성공 검증: 1번 성공 클래스의 C++ 생성물이 완전히 디스크에서 삭제되었는지 확인
    TestFalse(TEXT("1번 유효 클래스의 헤더 파일이 롤백되어 존재하지 않아야 함"),
        FPaths::FileExists(GeneratedHeaderPath(OutputSubPath, ValidClassName1)));
    TestFalse(TEXT("1번 유효 클래스의 소스 파일이 롤백되어 존재하지 않아야 함"),
        FPaths::FileExists(GeneratedSourcePath(OutputSubPath, ValidClassName1)));

    TSharedPtr<FJsonObject> ValidationObj = RootObject->GetObjectField(TEXT("validation"));
    TestTrue(TEXT("validation 오브젝트 존재"), ValidationObj.IsValid());
    if (ValidationObj.IsValid())
    {
        TestEqual(TEXT("rollback_failed false 검증"), ValidationObj->GetStringField(TEXT("rollback_failed")), TEXT("false"));
        TestEqual(TEXT("partial_changes_remaining false 검증"), ValidationObj->GetStringField(TEXT("partial_changes_remaining")), TEXT("false"));
    }

    // 2번 실패 클래스의 partial artifact가 디스크에 존재하지 않아야 함
    TestFalse(TEXT("2번 실패 클래스의 헤더 파일이 존재하지 않아야 함"),
        FPaths::FileExists(GeneratedHeaderPath(OutputSubPathFail, ValidClassName2)));
    TestFalse(TEXT("2번 실패 클래스의 소스 파일이 존재하지 않아야 함"),
        FPaths::FileExists(GeneratedSourcePath(OutputSubPathFail, ValidClassName2)));

    // Seam 및 테스트 픽스처 정리
    DeleteDirectoryBlockerFile(RuntimeModuleRoot(), TEXT("GeneratedBatchTests/Rollback/FailedDir"));
    DeleteGeneratedBatchFiles(OutputSubPath, { ValidClassName1 });
    return true;
}

bool FGenerateCppClassBatchDuplicateTargetTest::RunTest(const FString& Parameters)
{
    const FString SpecPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("CodexReports"), TEXT("test_cpp_class_batch_dup.json"));
    const FString ReportPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("CodexReports"), TEXT("test_cpp_class_batch_dup_report.json"));

    // 대소문자만 다른 클래스명을 지정하여 대소문자 비구분 경로 중복 검증
    const TArray<FString> DuplicateClassNames = { TEXT("CodexDuplicateClass"), TEXT("codexduplicateclass") };
    const FString OutputSubPath = TEXT("GeneratedBatchTests/Duplicate");

    IFileManager::Get().Delete(*SpecPath);
    IFileManager::Get().Delete(*ReportPath);
    DeleteGeneratedBatchFiles(OutputSubPath, DuplicateClassNames);

    TestTrue(TEXT("중복 C++ class batch spec 저장"), SaveBatchSpec(SpecPath,
        TEXT("tx-cpp-class-batch-dup"), DuplicateClassNames, OutputSubPath));

    UGenerateCppClassBatchCommandlet* Commandlet = NewObject<UGenerateCppClassBatchCommandlet>();

    // 1. Dry-run 검증 (Preflight 단계에서 중복 정규화 경로가 탐지되는지 확인)
    const int32 DryRunExitCode = Commandlet->Main(FString::Printf(
        TEXT("-Spec=\"%s\" -Output=\"%s\""), *SpecPath, *ReportPath));

    TestEqual(TEXT("dry-run exit code should be ValidationFailed"), DryRunExitCode,
        static_cast<int32>(ECommandForgeExitCode::ValidationFailed));

    TSharedPtr<FJsonObject> DryRunRoot;
    TestTrue(TEXT("dry-run report JSON 파싱"), LoadBatchJsonObject(ReportPath, DryRunRoot));
    TestFalse(TEXT("dry-run ok should be false"), DryRunRoot->GetBoolField(TEXT("ok")));

    const TArray<TSharedPtr<FJsonValue>>* Issues = nullptr;
    TestTrue(TEXT("issues 배열 존재"), DryRunRoot->TryGetArrayField(TEXT("issues"), Issues));
    bool bFoundDupError = false;
    if (Issues)
    {
        for (const auto& Val : *Issues)
        {
            const TSharedPtr<FJsonObject>* Obj = nullptr;
            if (Val->TryGetObject(Obj) && Obj && (*Obj)->GetStringField(TEXT("code")) == TEXT("DUPLICATE_TARGET_CLASS"))
            {
                bFoundDupError = true;
                break;
            }
        }
    }
    TestTrue(TEXT("정규화 경로 기반 DUPLICATE_TARGET_CLASS 이슈 탐지 확인"), bFoundDupError);

    // 2. Apply 검증 (Preflight 중복 탐지로 실제 파일 생성을 사전에 전면 차단하는지 확인)
    IFileManager::Get().Delete(*ReportPath);
    const int32 ApplyExitCode = Commandlet->Main(FString::Printf(
        TEXT("-Spec=\"%s\" -Output=\"%s\" -Apply"), *SpecPath, *ReportPath));

    TestEqual(TEXT("apply exit code should be ValidationFailed"), ApplyExitCode,
        static_cast<int32>(ECommandForgeExitCode::ValidationFailed));

    TSharedPtr<FJsonObject> ApplyRoot;
    TestTrue(TEXT("apply report JSON 파싱"), LoadBatchJsonObject(ReportPath, ApplyRoot));
    TestFalse(TEXT("apply ok should be false"), ApplyRoot->GetBoolField(TEXT("ok")));
    TestFalse(TEXT("중복 클래스 헤더 파일이 실제로 생성되지 않아야 함"),
        FPaths::FileExists(GeneratedHeaderPath(OutputSubPath, TEXT("CodexDuplicateClass"))));

    DeleteGeneratedBatchFiles(OutputSubPath, DuplicateClassNames);
    return true;
}

bool FGenerateCppClassBatchBuildCsRollbackTest::RunTest(const FString& Parameters)
{
    const FString SpecPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("CodexReports"), TEXT("test_cpp_class_batch_buildcs_rollback.json"));
    const FString ReportPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("CodexReports"), TEXT("test_cpp_class_batch_buildcs_rollback_report.json"));

    const FString ValidClassName1 = TEXT("CodexBuildCsRollbackValidClassA");
    const FString ValidClassName2 = TEXT("CodexBuildCsRollbackValidClassB");

    const FString OutputSubPath = TEXT("GeneratedBatchTests/BuildCsRollback/Success");
    const FString OutputSubPathFail = TEXT("GeneratedBatchTests/BuildCsRollback/FailedDir/ClassB");
    const FString TestModuleName = TEXT("CodexBatchBuildCsTestModule");
    const FString BuildCsPath = FPaths::Combine(BuildCsTestModuleRoot(), TEXT("CodexBatchBuildCsTestModule.Build.cs"));

    IFileManager::Get().Delete(*SpecPath);
    IFileManager::Get().Delete(*ReportPath);
    DeleteBuildCsTestModule();

    TestTrue(TEXT("Build.cs 테스트 모듈 생성"), CreateBuildCsTestModule());
    TestTrue(TEXT("Directory blocker file 생성"), CreateDirectoryBlockerFile(BuildCsTestModuleRoot(), TEXT("GeneratedBatchTests/BuildCsRollback/FailedDir")));

    // 1번 클래스는 의존성 모듈 "AIModule"을 적용하여 Build.cs 갱신 유도
    TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
    RootObject->SetStringField(TEXT("version"), TEXT("1"));
    RootObject->SetStringField(TEXT("kind"), TEXT("cpp_class_batch"));
    RootObject->SetStringField(TEXT("transaction_id"), TEXT("tx-cpp-batch-buildcs-rollback"));

    TArray<TSharedPtr<FJsonValue>> ClassesArray;

    TSharedRef<FJsonObject> ClassObj1 = MakeShared<FJsonObject>();
    ClassObj1->SetStringField(TEXT("name"), ValidClassName1);
    ClassObj1->SetStringField(TEXT("type"), TEXT("Actor"));
    ClassObj1->SetStringField(TEXT("module"), TestModuleName);
    ClassObj1->SetStringField(TEXT("output_path"), OutputSubPath);
    ClassObj1->SetStringField(TEXT("base_class"), TEXT("AActor"));
    ClassObj1->SetBoolField(TEXT("blueprint_type"), true);
    ClassesArray.Add(MakeShared<FJsonValueObject>(ClassObj1));

    TSharedRef<FJsonObject> ClassObj2 = MakeShared<FJsonObject>();
    ClassObj2->SetStringField(TEXT("name"), ValidClassName2);
    ClassObj2->SetStringField(TEXT("type"), TEXT("Actor"));
    ClassObj2->SetStringField(TEXT("module"), TestModuleName);
    ClassObj2->SetStringField(TEXT("output_path"), OutputSubPathFail);
    ClassObj2->SetStringField(TEXT("base_class"), TEXT("AActor"));
    ClassObj2->SetBoolField(TEXT("blueprint_type"), true);
    ClassesArray.Add(MakeShared<FJsonValueObject>(ClassObj2));
    RootObject->SetArrayField(TEXT("classes"), ClassesArray);

    TArray<TSharedPtr<FJsonValue>> Includes;
    Includes.Add(MakeShared<FJsonValueString>(TEXT("CoreMinimal.h")));
    RootObject->SetArrayField(TEXT("includes"), Includes);

    TSharedRef<FJsonObject> BuildDependenciesObject = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> PublicDependencies;
    PublicDependencies.Add(MakeShared<FJsonValueString>(TEXT("Core")));
    PublicDependencies.Add(MakeShared<FJsonValueString>(TEXT("AIModule"))); // 추가 의존성 모듈
    BuildDependenciesObject->SetArrayField(TEXT("public"), PublicDependencies);
    RootObject->SetObjectField(TEXT("build_dependencies"), BuildDependenciesObject);

    FString Serialized;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
    FJsonSerializer::Serialize(RootObject, Writer);
    FFileHelper::SaveStringToFile(Serialized, *SpecPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

    UGenerateCppClassBatchCommandlet* Commandlet = NewObject<UGenerateCppClassBatchCommandlet>();
    const int32 ExitCode = Commandlet->Main(FString::Printf(
        TEXT("-Spec=\"%s\" -Output=\"%s\" -Apply -ApplyBuildCs=true"), *SpecPath, *ReportPath));

    TestEqual(TEXT("apply exit code should be ValidationFailed"), ExitCode,
        static_cast<int32>(ECommandForgeExitCode::ValidationFailed));

    // 롤백 확인: Build.cs 파일 내용이 "AIModule"을 포함하지 않고 원래대로 복구되었는지 확인
    FString BuildCsContent;
    TestTrue(TEXT("Build.cs 읽기"), FFileHelper::LoadFileToString(BuildCsContent, *BuildCsPath));
    TestFalse(TEXT("Build.cs 롤백 복구 완료 확인"), BuildCsContent.Contains(TEXT("\"AIModule\"")));

    TSharedPtr<FJsonObject> ReportObj;
    TestTrue(TEXT("apply report JSON 파싱"), LoadBatchJsonObject(ReportPath, ReportObj));
    TestFalse(TEXT("apply ok should be false"), ReportObj->GetBoolField(TEXT("ok")));
    TestFalse(TEXT("apply applied should be false"), ReportObj->GetBoolField(TEXT("applied")));

    TSharedPtr<FJsonObject> ValidationObj = ReportObj->GetObjectField(TEXT("validation"));
    TestTrue(TEXT("validation 오브젝트 존재"), ValidationObj.IsValid());
    if (ValidationObj.IsValid())
    {
        TestEqual(TEXT("rollback_failed false 검증"), ValidationObj->GetStringField(TEXT("rollback_failed")), TEXT("false"));
        TestEqual(TEXT("partial_changes_remaining false 검증"), ValidationObj->GetStringField(TEXT("partial_changes_remaining")), TEXT("false"));
    }

    // 2번 실패 클래스의 partial artifact가 디스크에 존재하지 않아야 함
    TestFalse(TEXT("2번 실패 클래스의 헤더 파일이 존재하지 않아야 함"),
        FPaths::FileExists(GeneratedHeaderPath(OutputSubPathFail, ValidClassName2)));
    TestFalse(TEXT("2번 실패 클래스의 소스 파일이 존재하지 않아야 함"),
        FPaths::FileExists(GeneratedSourcePath(OutputSubPathFail, ValidClassName2)));

    DeleteDirectoryBlockerFile(BuildCsTestModuleRoot(), TEXT("GeneratedBatchTests/BuildCsRollback/FailedDir"));
    DeleteBuildCsTestModule();
    return true;
}

bool FGenerateCppClassBatchDuplicateClassSymbolTest::RunTest(const FString& Parameters)
{
    const FString SpecPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("CodexReports"), TEXT("test_cpp_class_batch_dup_symbol.json"));
    const FString ReportPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("CodexReports"), TEXT("test_cpp_class_batch_dup_symbol_report.json"));

    // MyActor(EnsureUnrealClassSymbol에 의해 AMyActor가 됨)와 AMyActor로 심볼 중복 상황 생성
    const TArray<FString> DuplicateClassNames = { TEXT("MyActor"), TEXT("AMyActor") };
    const FString OutputSubPath = TEXT("GeneratedBatchTests/DuplicateSymbol");

    IFileManager::Get().Delete(*SpecPath);
    IFileManager::Get().Delete(*ReportPath);
    DeleteGeneratedBatchFiles(OutputSubPath, DuplicateClassNames);

    TestTrue(TEXT("중복 C++ class batch spec 저장"), SaveBatchSpec(SpecPath,
        TEXT("tx-cpp-class-batch-dup-symbol"), DuplicateClassNames, OutputSubPath));

    UGenerateCppClassBatchCommandlet* Commandlet = NewObject<UGenerateCppClassBatchCommandlet>();

    // 1. Dry-run 검증 (Preflight 단계에서 중복 클래스 심볼이 탐지되는지 확인)
    const int32 DryRunExitCode = Commandlet->Main(FString::Printf(
        TEXT("-Spec=\"%s\" -Output=\"%s\""), *SpecPath, *ReportPath));

    TestEqual(TEXT("dry-run exit code should be ValidationFailed"), DryRunExitCode,
        static_cast<int32>(ECommandForgeExitCode::ValidationFailed));

    TSharedPtr<FJsonObject> DryRunRoot;
    TestTrue(TEXT("dry-run report JSON 파싱"), LoadBatchJsonObject(ReportPath, DryRunRoot));
    TestFalse(TEXT("dry-run ok should be false"), DryRunRoot->GetBoolField(TEXT("ok")));

    const TArray<TSharedPtr<FJsonValue>>* Issues = nullptr;
    TestTrue(TEXT("issues 배열 존재"), DryRunRoot->TryGetArrayField(TEXT("issues"), Issues));
    bool bFoundDupError = false;
    if (Issues)
    {
        for (const auto& Val : *Issues)
        {
            const TSharedPtr<FJsonObject>* Obj = nullptr;
            if (Val->TryGetObject(Obj) && Obj && (*Obj)->GetStringField(TEXT("code")) == TEXT("DUPLICATE_CLASS_SYMBOL"))
            {
                bFoundDupError = true;
                break;
            }
        }
    }
    TestTrue(TEXT("클래스 심볼 중복 DUPLICATE_CLASS_SYMBOL 이슈 탐지 확인"), bFoundDupError);

    // 2. Apply 검증 (Preflight 중복 탐지로 실제 파일 생성을 사전에 전면 차단하는지 확인)
    IFileManager::Get().Delete(*ReportPath);
    const int32 ApplyExitCode = Commandlet->Main(FString::Printf(
        TEXT("-Spec=\"%s\" -Output=\"%s\" -Apply"), *SpecPath, *ReportPath));

    TestEqual(TEXT("apply exit code should be ValidationFailed"), ApplyExitCode,
        static_cast<int32>(ECommandForgeExitCode::ValidationFailed));

    TSharedPtr<FJsonObject> ApplyRoot;
    TestTrue(TEXT("apply report JSON 파싱"), LoadBatchJsonObject(ReportPath, ApplyRoot));
    TestFalse(TEXT("apply ok should be false"), ApplyRoot->GetBoolField(TEXT("ok")));
    TestFalse(TEXT("중복 클래스 헤더 파일이 실제로 생성되지 않아야 함"),
        FPaths::FileExists(GeneratedHeaderPath(OutputSubPath, TEXT("MyActor"))));

    DeleteGeneratedBatchFiles(OutputSubPath, DuplicateClassNames);
    return true;
}
