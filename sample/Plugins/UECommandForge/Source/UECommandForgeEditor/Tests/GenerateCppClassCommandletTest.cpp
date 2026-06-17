#include "Misc/AutomationTest.h"
#include "Commandlets/GenerateCppClassCommandlet.h"
#include "CommandForgeTypes.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGenerateCppClassDryRunTest,
    "UECommandForge.Commandlets.GenerateCppClass.DryRunReportsPreview",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGenerateCppClassApplyTest,
    "UECommandForge.Commandlets.GenerateCppClass.ApplyWritesFilesAndChecksums",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGenerateCppClassRejectsOutsideModuleTest,
    "UECommandForge.Commandlets.GenerateCppClass.RejectsOutputOutsideModule",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGenerateCppClassRejectsInvalidTokensTest,
    "UECommandForge.Commandlets.GenerateCppClass.RejectsInvalidCppTokens",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGenerateCppClassApplyBuildCsTest,
    "UECommandForge.Commandlets.GenerateCppClass.ApplyBuildCsDependencies",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGenerateCppClassRejectsInvalidBuildDependencyArrayTest,
    "UECommandForge.Commandlets.GenerateCppClass.RejectsInvalidBuildDependencyArray",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGenerateCppClassMissingBuildCsDoesNotWriteClassFilesTest,
    "UECommandForge.Commandlets.GenerateCppClass.MissingBuildCsDoesNotWriteClassFiles",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
    bool LoadGenerateCppClassJsonObject(const FString& Path, TSharedPtr<FJsonObject>& OutObject)
    {
        FString Content;
        if (!FFileHelper::LoadFileToString(Content, *Path))
        {
            return false;
        }

        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
        return FJsonSerializer::Deserialize(Reader, OutObject) && OutObject.IsValid();
    }

    void AddGenerateCppMetadata(TSharedRef<FJsonObject> Object, const TMap<FString, FString>& Metadata)
    {
        TSharedRef<FJsonObject> MetadataObject = MakeShared<FJsonObject>();
        for (const TPair<FString, FString>& Entry : Metadata)
        {
            MetadataObject->SetStringField(Entry.Key, Entry.Value);
        }
        Object->SetObjectField(TEXT("metadata"), MetadataObject);
    }

    bool SaveGenerateCppClassSpec(const FString& Path, const FString& TransactionId,
        const FString& ClassName, const FString& OutputPath, const FString& ExtraInclude = TEXT(""),
        const FString& ModuleName = TEXT("UECommandForgeRuntime"),
        const FString& ExtraPublicDependency = TEXT(""), const FString& ExtraPrivateDependency = TEXT(""))
    {
        TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
        RootObject->SetStringField(TEXT("version"), TEXT("1"));
        RootObject->SetStringField(TEXT("kind"), TEXT("cpp_class"));
        RootObject->SetStringField(TEXT("transaction_id"), TransactionId);

        TSharedRef<FJsonObject> ClassObject = MakeShared<FJsonObject>();
        ClassObject->SetStringField(TEXT("name"), ClassName);
        ClassObject->SetStringField(TEXT("type"), TEXT("ActorComponent"));
        ClassObject->SetStringField(TEXT("module"), ModuleName);
        ClassObject->SetStringField(TEXT("output_path"), OutputPath);
        ClassObject->SetStringField(TEXT("base_class"), TEXT("UActorComponent"));
        ClassObject->SetBoolField(TEXT("blueprint_type"), true);
        ClassObject->SetBoolField(TEXT("tick"), false);
        RootObject->SetObjectField(TEXT("class"), ClassObject);

        TArray<TSharedPtr<FJsonValue>> Includes;
        Includes.Add(MakeShared<FJsonValueString>(TEXT("CoreMinimal.h")));
        Includes.Add(MakeShared<FJsonValueString>(TEXT("Components/ActorComponent.h")));
        if (!ExtraInclude.IsEmpty())
        {
            Includes.Add(MakeShared<FJsonValueString>(ExtraInclude));
        }
        RootObject->SetArrayField(TEXT("includes"), Includes);

        TSharedRef<FJsonObject> PropertyObject = MakeShared<FJsonObject>();
        PropertyObject->SetStringField(TEXT("name"), TEXT("MaxHealth"));
        PropertyObject->SetStringField(TEXT("type"), TEXT("float"));
        PropertyObject->SetStringField(TEXT("default"), TEXT("100.0f"));
        AddGenerateCppMetadata(PropertyObject, {
            { TEXT("EditAnywhere"), TEXT("true") },
            { TEXT("BlueprintReadOnly"), TEXT("true") },
            { TEXT("Category"), TEXT("Health") }
        });

        TArray<TSharedPtr<FJsonValue>> Properties;
        Properties.Add(MakeShared<FJsonValueObject>(PropertyObject));
        RootObject->SetArrayField(TEXT("properties"), Properties);

        TSharedRef<FJsonObject> FunctionObject = MakeShared<FJsonObject>();
        FunctionObject->SetStringField(TEXT("name"), TEXT("ApplyDamage"));
        FunctionObject->SetStringField(TEXT("return_type"), TEXT("void"));
        AddGenerateCppMetadata(FunctionObject, {
            { TEXT("BlueprintCallable"), TEXT("true") },
            { TEXT("Category"), TEXT("Health") }
        });

        TSharedRef<FJsonObject> ParamObject = MakeShared<FJsonObject>();
        ParamObject->SetStringField(TEXT("name"), TEXT("DamageAmount"));
        ParamObject->SetStringField(TEXT("type"), TEXT("float"));
        TArray<TSharedPtr<FJsonValue>> Params;
        Params.Add(MakeShared<FJsonValueObject>(ParamObject));
        FunctionObject->SetArrayField(TEXT("params"), Params);

        TArray<TSharedPtr<FJsonValue>> Functions;
        Functions.Add(MakeShared<FJsonValueObject>(FunctionObject));
        RootObject->SetArrayField(TEXT("functions"), Functions);

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

    FString GccRuntimeModuleRoot()
    {
        return FPaths::ConvertRelativePathToFull(FPaths::Combine(
            FPaths::ProjectPluginsDir(), TEXT("UECommandForge"), TEXT("Source"), TEXT("UECommandForgeRuntime")));
    }

    FString GccPluginModuleRoot(const FString& ModuleName)
    {
        return FPaths::ConvertRelativePathToFull(FPaths::Combine(
            FPaths::ProjectPluginsDir(), TEXT("UECommandForge"), TEXT("Source"), ModuleName));
    }

    FString GeneratedCppTestOutputPath(const FString& ClassName)
    {
        return TEXT("GeneratedTests") / ClassName;
    }

    FString GeneratedCppHeaderPath(const FString& ClassName)
    {
        return FPaths::Combine(GccRuntimeModuleRoot(), TEXT("Public"), GeneratedCppTestOutputPath(ClassName),
            ClassName + TEXT(".h"));
    }

    FString GeneratedCppSourcePath(const FString& ClassName)
    {
        return FPaths::Combine(GccRuntimeModuleRoot(), TEXT("Private"), GeneratedCppTestOutputPath(ClassName),
            ClassName + TEXT(".cpp"));
    }

    FString GeneratedCppHeaderPathForModule(const FString& ModuleName, const FString& ClassName)
    {
        return FPaths::Combine(GccPluginModuleRoot(ModuleName), TEXT("Public"), GeneratedCppTestOutputPath(ClassName),
            ClassName + TEXT(".h"));
    }

    FString GeneratedCppSourcePathForModule(const FString& ModuleName, const FString& ClassName)
    {
        return FPaths::Combine(GccPluginModuleRoot(ModuleName), TEXT("Private"), GeneratedCppTestOutputPath(ClassName),
            ClassName + TEXT(".cpp"));
    }

    void DeleteGeneratedCppClassFiles(const FString& ClassName)
    {
        const FString PublicPath = GeneratedCppHeaderPath(ClassName);
        const FString PrivatePath = GeneratedCppSourcePath(ClassName);
        IFileManager::Get().Delete(*PublicPath);
        IFileManager::Get().Delete(*PrivatePath);
        IFileManager::Get().DeleteDirectory(*FPaths::GetPath(PublicPath), false, true);
        IFileManager::Get().DeleteDirectory(*FPaths::GetPath(PrivatePath), false, true);
        IFileManager::Get().DeleteDirectory(
            *FPaths::Combine(GccRuntimeModuleRoot(), TEXT("Public"), TEXT("GeneratedTests")), false, true);
        IFileManager::Get().DeleteDirectory(
            *FPaths::Combine(GccRuntimeModuleRoot(), TEXT("Private"), TEXT("GeneratedTests")), false, true);
    }

    FString GccBuildCsTestModuleRoot()
    {
        return GccPluginModuleRoot(TEXT("CodexBuildCsTestModule"));
    }

    void GccDeleteBuildCsTestModule()
    {
        IFileManager::Get().DeleteDirectory(*GccBuildCsTestModuleRoot(), false, true);
    }

    bool GccCreateBuildCsTestModule()
    {
        GccDeleteBuildCsTestModule();
        const FString ModuleRoot = GccBuildCsTestModuleRoot();
        FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*ModuleRoot);
        const FString BuildCsPath = FPaths::Combine(ModuleRoot, TEXT("CodexBuildCsTestModule.Build.cs"));
        const FString BuildCs = TEXT(R"(using UnrealBuildTool;

public class CodexBuildCsTestModule : ModuleRules
{
    public CodexBuildCsTestModule(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core" });
    }
}
)");
        return FFileHelper::SaveStringToFile(BuildCs, *BuildCsPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    }

    bool SaveGenerateCppClassSpecWithInvalidBuildDependency(const FString& Path,
        const FString& ClassName)
    {
        TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
        RootObject->SetStringField(TEXT("version"), TEXT("1"));
        RootObject->SetStringField(TEXT("kind"), TEXT("cpp_class"));
        RootObject->SetStringField(TEXT("transaction_id"), TEXT("tx-cpp-class-invalid-build-dependency"));

        TSharedRef<FJsonObject> ClassObject = MakeShared<FJsonObject>();
        ClassObject->SetStringField(TEXT("name"), ClassName);
        ClassObject->SetStringField(TEXT("type"), TEXT("ActorComponent"));
        ClassObject->SetStringField(TEXT("module"), TEXT("UECommandForgeRuntime"));
        ClassObject->SetStringField(TEXT("output_path"), GeneratedCppTestOutputPath(ClassName));
        ClassObject->SetStringField(TEXT("base_class"), TEXT("UActorComponent"));
        ClassObject->SetBoolField(TEXT("blueprint_type"), true);
        RootObject->SetObjectField(TEXT("class"), ClassObject);

        TArray<TSharedPtr<FJsonValue>> Includes;
        Includes.Add(MakeShared<FJsonValueString>(TEXT("CoreMinimal.h")));
        RootObject->SetArrayField(TEXT("includes"), Includes);

        TSharedRef<FJsonObject> BuildDependenciesObject = MakeShared<FJsonObject>();
        TArray<TSharedPtr<FJsonValue>> PublicDependencies;
        PublicDependencies.Add(MakeShared<FJsonValueString>(TEXT("Core")));
        PublicDependencies.Add(MakeShared<FJsonValueNumber>(42.0));
        BuildDependenciesObject->SetArrayField(TEXT("public"), PublicDependencies);
        RootObject->SetObjectField(TEXT("build_dependencies"), BuildDependenciesObject);

        FString Serialized;
        const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
        FJsonSerializer::Serialize(RootObject, Writer);
        FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FPaths::GetPath(Path));
        return FFileHelper::SaveStringToFile(Serialized, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    }
}

bool FGenerateCppClassDryRunTest::RunTest(const FString& Parameters)
{
    const FString SpecPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("UECommandForge"), TEXT("Reports"), TEXT("test_cpp_class_dry_run.json"));
    const FString ReportPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("UECommandForge"), TEXT("Reports"), TEXT("test_cpp_class_dry_run_report.json"));
    const FString ClassName = TEXT("CodexDryRunHealthComponent");

    IFileManager::Get().Delete(*SpecPath);
    IFileManager::Get().Delete(*ReportPath);
    DeleteGeneratedCppClassFiles(ClassName);

    TestTrue(TEXT("C++ class spec 저장"), SaveGenerateCppClassSpec(SpecPath,
        TEXT("tx-cpp-class-dry-run"), ClassName, GeneratedCppTestOutputPath(ClassName)));

    UGenerateCppClassCommandlet* Commandlet = NewObject<UGenerateCppClassCommandlet>();
    const int32 ExitCode = Commandlet->Main(FString::Printf(
        TEXT("-Spec=\"%s\" -Output=\"%s\""), *SpecPath, *ReportPath));

    TestEqual(TEXT("dry-run exit code"), ExitCode, 0);

    TSharedPtr<FJsonObject> RootObject;
    TestTrue(TEXT("dry-run report JSON 파싱"),
        LoadGenerateCppClassJsonObject(ReportPath, RootObject));
    TestTrue(TEXT("dry-run ok true"), RootObject->GetBoolField(TEXT("ok")));
    TestTrue(TEXT("dry-run flag true"), RootObject->GetBoolField(TEXT("dry_run")));
    TestFalse(TEXT("dry-run not applied"), RootObject->GetBoolField(TEXT("applied")));
    TestTrue(TEXT("header preview includes generated class"),
        RootObject->GetObjectField(TEXT("validation"))->GetStringField(TEXT("header_preview"))
            .Contains(TEXT("UCLASS(BlueprintType")));
    TestFalse(TEXT("dry-run did not write header"),
        FPaths::FileExists(GeneratedCppHeaderPath(ClassName)));
    return true;
}

bool FGenerateCppClassApplyTest::RunTest(const FString& Parameters)
{
    const FString SpecPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("UECommandForge"), TEXT("Reports"), TEXT("test_cpp_class_apply.json"));
    const FString ReportPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("UECommandForge"), TEXT("Reports"), TEXT("test_cpp_class_apply_report.json"));
    const FString ClassName = TEXT("CodexApplyHealthComponent");
    const FString HeaderPath = GeneratedCppHeaderPath(ClassName);
    const FString SourcePath = GeneratedCppSourcePath(ClassName);

    IFileManager::Get().Delete(*SpecPath);
    IFileManager::Get().Delete(*ReportPath);
    DeleteGeneratedCppClassFiles(ClassName);

    TestTrue(TEXT("C++ class apply spec 저장"), SaveGenerateCppClassSpec(SpecPath,
        TEXT("tx-cpp-class-apply"), ClassName, GeneratedCppTestOutputPath(ClassName)));

    UGenerateCppClassCommandlet* Commandlet = NewObject<UGenerateCppClassCommandlet>();
    const int32 ExitCode = Commandlet->Main(FString::Printf(
        TEXT("-Spec=\"%s\" -Output=\"%s\" -Apply"), *SpecPath, *ReportPath));

    TestEqual(TEXT("apply exit code"), ExitCode, 0);
    TestTrue(TEXT("header written"), FPaths::FileExists(HeaderPath));
    TestTrue(TEXT("source written"), FPaths::FileExists(SourcePath));

    FString HeaderContent;
    TestTrue(TEXT("header read"), FFileHelper::LoadFileToString(HeaderContent, *HeaderPath));
    TestTrue(TEXT("UPROPERTY generated"), HeaderContent.Contains(TEXT("UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=\"Health\")")));
    TestTrue(TEXT("UFUNCTION generated"), HeaderContent.Contains(TEXT("UFUNCTION(BlueprintCallable, Category=\"Health\")")));

    TSharedPtr<FJsonObject> RootObject;
    TestTrue(TEXT("apply report JSON 파싱"),
        LoadGenerateCppClassJsonObject(ReportPath, RootObject));
    TestTrue(TEXT("apply ok true"), RootObject->GetBoolField(TEXT("ok")));
    TestTrue(TEXT("apply applied true"), RootObject->GetBoolField(TEXT("applied")));
    TestEqual(TEXT("changed file count"), RootObject->GetArrayField(TEXT("changed_files")).Num(), 2);
    TestFalse(TEXT("header checksum recorded"),
        RootObject->GetObjectField(TEXT("validation"))->GetStringField(TEXT("header_sha1")).IsEmpty());

    DeleteGeneratedCppClassFiles(ClassName);
    return true;
}

bool FGenerateCppClassRejectsOutsideModuleTest::RunTest(const FString& Parameters)
{
    const FString SpecPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("UECommandForge"), TEXT("Reports"), TEXT("test_cpp_class_invalid.json"));
    const FString ReportPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("UECommandForge"), TEXT("Reports"), TEXT("test_cpp_class_invalid_report.json"));

    IFileManager::Get().Delete(*SpecPath);
    IFileManager::Get().Delete(*ReportPath);
    TestTrue(TEXT("invalid C++ class spec 저장"), SaveGenerateCppClassSpec(SpecPath,
        TEXT("tx-cpp-class-invalid"), TEXT("CodexInvalidHealthComponent"), TEXT("../Outside")));

    UGenerateCppClassCommandlet* Commandlet = NewObject<UGenerateCppClassCommandlet>();
    const int32 ExitCode = Commandlet->Main(FString::Printf(
        TEXT("-Spec=\"%s\" -Output=\"%s\" -Apply"), *SpecPath, *ReportPath));

    TestEqual(TEXT("invalid exit code"), ExitCode,
        static_cast<int32>(ECommandForgeExitCode::ValidationFailed));

    TSharedPtr<FJsonObject> RootObject;
    TestTrue(TEXT("invalid report JSON 파싱"),
        LoadGenerateCppClassJsonObject(ReportPath, RootObject));
    TestFalse(TEXT("invalid ok false"), RootObject->GetBoolField(TEXT("ok")));
    TestFalse(TEXT("invalid not applied"), RootObject->GetBoolField(TEXT("applied")));

    const TArray<TSharedPtr<FJsonValue>>* Issues = nullptr;
    TestTrue(TEXT("invalid issues 배열 존재"),
        RootObject->TryGetArrayField(TEXT("issues"), Issues));
    TestTrue(TEXT("outside module issue present"), Issues != nullptr && Issues->Num() >= 1);
    return true;
}

bool FGenerateCppClassRejectsInvalidTokensTest::RunTest(const FString& Parameters)
{
    const FString SpecPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("UECommandForge"), TEXT("Reports"), TEXT("test_cpp_class_invalid_tokens.json"));
    const FString ReportPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("UECommandForge"), TEXT("Reports"), TEXT("test_cpp_class_invalid_tokens_report.json"));

    IFileManager::Get().Delete(*SpecPath);
    IFileManager::Get().Delete(*ReportPath);
    TestTrue(TEXT("invalid token C++ class spec 저장"), SaveGenerateCppClassSpec(SpecPath,
        TEXT("tx-cpp-class-invalid-tokens"), TEXT("CodexInvalidTokenComponent"),
        GeneratedCppTestOutputPath(TEXT("CodexInvalidTokenComponent")),
        TEXT("CoreMinimal.h\"\n#include \"Injected.h")));

    UGenerateCppClassCommandlet* Commandlet = NewObject<UGenerateCppClassCommandlet>();
    const int32 ExitCode = Commandlet->Main(FString::Printf(
        TEXT("-Spec=\"%s\" -Output=\"%s\" -Apply"), *SpecPath, *ReportPath));

    TestEqual(TEXT("invalid token exit code"), ExitCode,
        static_cast<int32>(ECommandForgeExitCode::ValidationFailed));

    TSharedPtr<FJsonObject> RootObject;
    TestTrue(TEXT("invalid token report JSON 파싱"),
        LoadGenerateCppClassJsonObject(ReportPath, RootObject));
    TestFalse(TEXT("invalid token ok false"), RootObject->GetBoolField(TEXT("ok")));
    TestFalse(TEXT("invalid token not applied"), RootObject->GetBoolField(TEXT("applied")));

    const TArray<TSharedPtr<FJsonValue>>* Issues = nullptr;
    TestTrue(TEXT("invalid token issues 배열 존재"),
        RootObject->TryGetArrayField(TEXT("issues"), Issues));
    bool bFoundInvalidInclude = false;
    if (Issues != nullptr)
    {
        for (const TSharedPtr<FJsonValue>& IssueValue : *Issues)
        {
            const TSharedPtr<FJsonObject>* IssueObject = nullptr;
            if (IssueValue->TryGetObject(IssueObject) && IssueObject != nullptr &&
                (*IssueObject)->GetStringField(TEXT("code")) == TEXT("INVALID_INCLUDE"))
            {
                bFoundInvalidInclude = true;
                break;
            }
        }
    }
    TestTrue(TEXT("invalid include issue present"), bFoundInvalidInclude);
    return true;
}

bool FGenerateCppClassApplyBuildCsTest::RunTest(const FString& Parameters)
{
    const FString SpecPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("UECommandForge"), TEXT("Reports"), TEXT("test_cpp_class_apply_buildcs.json"));
    const FString ReportPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("UECommandForge"), TEXT("Reports"), TEXT("test_cpp_class_apply_buildcs_report.json"));
    const FString ClassName = TEXT("CodexBuildCsHealthComponent");
    const FString BuildCsPath = FPaths::Combine(GccBuildCsTestModuleRoot(), TEXT("CodexBuildCsTestModule.Build.cs"));

    IFileManager::Get().Delete(*SpecPath);
    IFileManager::Get().Delete(*ReportPath);
    GccDeleteBuildCsTestModule();
    TestTrue(TEXT("Build.cs test module 생성"), GccCreateBuildCsTestModule());
    TestTrue(TEXT("Build.cs apply spec 저장"), SaveGenerateCppClassSpec(SpecPath,
        TEXT("tx-cpp-class-buildcs"), ClassName, GeneratedCppTestOutputPath(ClassName),
        TEXT(""), TEXT("CodexBuildCsTestModule"), TEXT("AIModule"), TEXT("Slate")));

    UGenerateCppClassCommandlet* Commandlet = NewObject<UGenerateCppClassCommandlet>();
    const int32 ExitCode = Commandlet->Main(FString::Printf(
        TEXT("-Spec=\"%s\" -Output=\"%s\" -Apply -ApplyBuildCs=true"), *SpecPath, *ReportPath));

    TestEqual(TEXT("apply Build.cs exit code"), ExitCode, 0);
    FString BuildCsContent;
    TestTrue(TEXT("Build.cs read"), FFileHelper::LoadFileToString(BuildCsContent, *BuildCsPath));
    TestTrue(TEXT("public dependency added"), BuildCsContent.Contains(TEXT("\"AIModule\"")));
    TestTrue(TEXT("private dependency added"), BuildCsContent.Contains(TEXT("\"Slate\"")));

    TSharedPtr<FJsonObject> RootObject;
    TestTrue(TEXT("apply Build.cs report JSON 파싱"),
        LoadGenerateCppClassJsonObject(ReportPath, RootObject));
    TestTrue(TEXT("apply Build.cs ok true"), RootObject->GetBoolField(TEXT("ok")));
    TestEqual(TEXT("changed file count includes Build.cs"),
        RootObject->GetArrayField(TEXT("changed_files")).Num(), 3);
    TestEqual(TEXT("Build.cs changed validation"),
        RootObject->GetObjectField(TEXT("validation"))->GetStringField(TEXT("buildcs_changed")), TEXT("true"));

    GccDeleteBuildCsTestModule();
    return true;
}

bool FGenerateCppClassRejectsInvalidBuildDependencyArrayTest::RunTest(const FString& Parameters)
{
    const FString SpecPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("UECommandForge"), TEXT("Reports"), TEXT("test_cpp_class_invalid_build_dep_array.json"));
    const FString ReportPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("UECommandForge"), TEXT("Reports"), TEXT("test_cpp_class_invalid_build_dep_array_report.json"));
    const FString ClassName = TEXT("CodexInvalidBuildDependencyComponent");

    IFileManager::Get().Delete(*SpecPath);
    IFileManager::Get().Delete(*ReportPath);
    DeleteGeneratedCppClassFiles(ClassName);
    TestTrue(TEXT("invalid build dependency spec 저장"),
        SaveGenerateCppClassSpecWithInvalidBuildDependency(SpecPath, ClassName));

    UGenerateCppClassCommandlet* Commandlet = NewObject<UGenerateCppClassCommandlet>();
    const int32 ExitCode = Commandlet->Main(FString::Printf(
        TEXT("-Spec=\"%s\" -Output=\"%s\" -Apply"), *SpecPath, *ReportPath));

    TestEqual(TEXT("invalid build dependency array exit code"), ExitCode,
        static_cast<int32>(ECommandForgeExitCode::SpecParseFailed));

    TSharedPtr<FJsonObject> RootObject;
    TestTrue(TEXT("invalid build dependency report JSON 파싱"),
        LoadGenerateCppClassJsonObject(ReportPath, RootObject));
    TestFalse(TEXT("invalid build dependency ok false"), RootObject->GetBoolField(TEXT("ok")));
    TestFalse(TEXT("invalid build dependency not applied"), RootObject->GetBoolField(TEXT("applied")));
    TestFalse(TEXT("invalid build dependency did not write header"),
        FPaths::FileExists(GeneratedCppHeaderPath(ClassName)));
    return true;
}

bool FGenerateCppClassMissingBuildCsDoesNotWriteClassFilesTest::RunTest(const FString& Parameters)
{
    const FString ModuleName = TEXT("CodexMissingBuildCsTestModule");
    const FString SpecPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("UECommandForge"), TEXT("Reports"), TEXT("test_cpp_class_missing_buildcs.json"));
    const FString ReportPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("UECommandForge"), TEXT("Reports"), TEXT("test_cpp_class_missing_buildcs_report.json"));
    const FString ClassName = TEXT("CodexMissingBuildCsHealthComponent");
    const FString ModuleRoot = GccPluginModuleRoot(ModuleName);
    const FString HeaderPath = GeneratedCppHeaderPathForModule(ModuleName, ClassName);
    const FString SourcePath = GeneratedCppSourcePathForModule(ModuleName, ClassName);

    IFileManager::Get().Delete(*SpecPath);
    IFileManager::Get().Delete(*ReportPath);
    IFileManager::Get().DeleteDirectory(*ModuleRoot, false, true);
    FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*ModuleRoot);
    TestTrue(TEXT("missing Build.cs apply spec 저장"), SaveGenerateCppClassSpec(SpecPath,
        TEXT("tx-cpp-class-missing-buildcs"), ClassName, GeneratedCppTestOutputPath(ClassName),
        TEXT(""), ModuleName, TEXT("AIModule")));

    UGenerateCppClassCommandlet* Commandlet = NewObject<UGenerateCppClassCommandlet>();
    const int32 ExitCode = Commandlet->Main(FString::Printf(
        TEXT("-Spec=\"%s\" -Output=\"%s\" -Apply -ApplyBuildCs=true"), *SpecPath, *ReportPath));

    TestEqual(TEXT("missing Build.cs exit code"), ExitCode,
        static_cast<int32>(ECommandForgeExitCode::ValidationFailed));
    TestFalse(TEXT("missing Build.cs did not write header"), FPaths::FileExists(HeaderPath));
    TestFalse(TEXT("missing Build.cs did not write source"), FPaths::FileExists(SourcePath));

    TSharedPtr<FJsonObject> RootObject;
    TestTrue(TEXT("missing Build.cs report JSON 파싱"),
        LoadGenerateCppClassJsonObject(ReportPath, RootObject));
    TestFalse(TEXT("missing Build.cs ok false"), RootObject->GetBoolField(TEXT("ok")));
    TestFalse(TEXT("missing Build.cs not applied"), RootObject->GetBoolField(TEXT("applied")));

    IFileManager::Get().DeleteDirectory(*ModuleRoot, false, true);
    return true;
}
