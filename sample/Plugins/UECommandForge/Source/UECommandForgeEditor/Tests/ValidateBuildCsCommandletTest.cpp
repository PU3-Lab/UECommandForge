#include "Misc/AutomationTest.h"
#include "Commandlets/ValidateBuildCsCommandlet.h"
#include "CommandForgeTypes.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FValidateBuildCsSatisfiedPolicyTest,
    "UECommandForge.Commandlets.ValidateBuildCs.PassesSatisfiedPolicy",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FValidateBuildCsMissingDependencyTest,
    "UECommandForge.Commandlets.ValidateBuildCs.ReportsMissingDependency",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FValidateBuildCsInvalidDependencyNameTest,
    "UECommandForge.Commandlets.ValidateBuildCs.RejectsInvalidDependencyName",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FValidateBuildCsMultipleAddRangeBlocksTest,
    "UECommandForge.Commandlets.ValidateBuildCs.AcceptsDependencyInLaterAddRangeBlock",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
    bool LoadValidateBuildCsJsonObject(const FString& Path, TSharedPtr<FJsonObject>& OutObject)
    {
        FString Content;
        if (!FFileHelper::LoadFileToString(Content, *Path))
        {
            return false;
        }

        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
        return FJsonSerializer::Deserialize(Reader, OutObject) && OutObject.IsValid();
    }

    bool SaveValidateBuildCsPolicy(const FString& Path, const FString& RequiredPublicDependency)
    {
        const FString PolicyJson = FString::Printf(TEXT(R"({
            "version": "1",
            "kind": "buildcs_policy",
            "modules": [
                {
                    "name": "UECommandForgeRuntime",
                    "required_public": ["Core", "CoreUObject", "Engine", "%s"],
                    "required_private": []
                }
            ]
        })"), *RequiredPublicDependency);
        FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FPaths::GetPath(Path));
        return FFileHelper::SaveStringToFile(PolicyJson, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    }

    FString ValidateBuildCsTestModuleRoot()
    {
        return FPaths::ConvertRelativePathToFull(FPaths::Combine(
            FPaths::ProjectPluginsDir(), TEXT("UECommandForge"), TEXT("Source"), TEXT("CodexValidateBuildCsTestModule")));
    }

    void DeleteValidateBuildCsTestModule()
    {
        IFileManager::Get().DeleteDirectory(*ValidateBuildCsTestModuleRoot(), false, true);
    }

    bool CreateValidateBuildCsModuleWithMultipleAddRanges()
    {
        DeleteValidateBuildCsTestModule();
        const FString ModuleRoot = ValidateBuildCsTestModuleRoot();
        FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*ModuleRoot);
        const FString BuildCsPath = FPaths::Combine(ModuleRoot, TEXT("CodexValidateBuildCsTestModule.Build.cs"));
        const FString BuildCs = TEXT(R"(using UnrealBuildTool;

public class CodexValidateBuildCsTestModule : ModuleRules
{
    public CodexValidateBuildCsTestModule(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core" });
        PublicDependencyModuleNames.AddRange(new[] { "AIModule" });
    }
}
)");
        return FFileHelper::SaveStringToFile(BuildCs, *BuildCsPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    }

    bool SaveValidateBuildCsMultiAddRangePolicy(const FString& Path)
    {
        const FString PolicyJson = TEXT(R"({
            "version": "1",
            "kind": "buildcs_policy",
            "modules": [
                {
                    "name": "CodexValidateBuildCsTestModule",
                    "required_public": ["AIModule"],
                    "required_private": []
                }
            ]
        })");
        FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FPaths::GetPath(Path));
        return FFileHelper::SaveStringToFile(PolicyJson, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    }
}

bool FValidateBuildCsSatisfiedPolicyTest::RunTest(const FString& Parameters)
{
    const FString PolicyPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("CodexReports"), TEXT("test_buildcs_policy_ok.json"));
    const FString ReportPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("CodexReports"), TEXT("test_buildcs_policy_ok_report.json"));

    IFileManager::Get().Delete(*PolicyPath);
    IFileManager::Get().Delete(*ReportPath);
    TestTrue(TEXT("satisfied Build.cs policy 저장"), SaveValidateBuildCsPolicy(PolicyPath, TEXT("Core")));

    UValidateBuildCsCommandlet* Commandlet = NewObject<UValidateBuildCsCommandlet>();
    const int32 ExitCode = Commandlet->Main(FString::Printf(
        TEXT("-Policy=\"%s\" -Output=\"%s\""), *PolicyPath, *ReportPath));

    TestEqual(TEXT("satisfied policy exit code"), ExitCode, 0);

    TSharedPtr<FJsonObject> RootObject;
    TestTrue(TEXT("satisfied policy report JSON 파싱"), LoadValidateBuildCsJsonObject(ReportPath, RootObject));
    TestTrue(TEXT("satisfied policy ok true"), RootObject->GetBoolField(TEXT("ok")));
    TestEqual(TEXT("commandlet 이름"), RootObject->GetStringField(TEXT("commandlet")),
        TEXT("ValidateBuildCs"));
    TestEqual(TEXT("no issues"), RootObject->GetArrayField(TEXT("issues")).Num(), 0);
    return true;
}

bool FValidateBuildCsMissingDependencyTest::RunTest(const FString& Parameters)
{
    const FString PolicyPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("CodexReports"), TEXT("test_buildcs_policy_missing.json"));
    const FString ReportPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("CodexReports"), TEXT("test_buildcs_policy_missing_report.json"));

    IFileManager::Get().Delete(*PolicyPath);
    IFileManager::Get().Delete(*ReportPath);
    TestTrue(TEXT("missing Build.cs policy 저장"), SaveValidateBuildCsPolicy(PolicyPath, TEXT("AIModule")));

    UValidateBuildCsCommandlet* Commandlet = NewObject<UValidateBuildCsCommandlet>();
    const int32 ExitCode = Commandlet->Main(FString::Printf(
        TEXT("-Policy=\"%s\" -Output=\"%s\""), *PolicyPath, *ReportPath));

    TestEqual(TEXT("missing policy exit code"), ExitCode,
        static_cast<int32>(ECommandForgeExitCode::ValidationFailed));

    TSharedPtr<FJsonObject> RootObject;
    TestTrue(TEXT("missing policy report JSON 파싱"), LoadValidateBuildCsJsonObject(ReportPath, RootObject));
    TestFalse(TEXT("missing policy ok false"), RootObject->GetBoolField(TEXT("ok")));

    const TArray<TSharedPtr<FJsonValue>>* Issues = nullptr;
    TestTrue(TEXT("missing policy issues 배열 존재"),
        RootObject->TryGetArrayField(TEXT("issues"), Issues));
    bool bFoundMissingPublicDependency = false;
    if (Issues != nullptr)
    {
        for (const TSharedPtr<FJsonValue>& IssueValue : *Issues)
        {
            const TSharedPtr<FJsonObject>* IssueObject = nullptr;
            if (IssueValue->TryGetObject(IssueObject) && IssueObject != nullptr &&
                (*IssueObject)->GetStringField(TEXT("code")) == TEXT("BUILDCS_PUBLIC_DEPENDENCY_MISSING"))
            {
                bFoundMissingPublicDependency = true;
                break;
            }
        }
    }
    TestTrue(TEXT("missing public dependency issue"), bFoundMissingPublicDependency);
    return true;
}

bool FValidateBuildCsInvalidDependencyNameTest::RunTest(const FString& Parameters)
{
    const FString PolicyPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("CodexReports"), TEXT("test_buildcs_policy_invalid_dep.json"));
    const FString ReportPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("CodexReports"), TEXT("test_buildcs_policy_invalid_dep_report.json"));

    const FString PolicyJson = TEXT(R"({
        "version": "1",
        "kind": "buildcs_policy",
        "modules": [
            {
                "name": "UECommandForgeRuntime",
                "required_public": ["Engine;Bad"],
                "required_private": []
            }
        ]
    })");

    IFileManager::Get().Delete(*PolicyPath);
    IFileManager::Get().Delete(*ReportPath);
    FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FPaths::GetPath(PolicyPath));
    TestTrue(TEXT("invalid dependency policy 저장"),
        FFileHelper::SaveStringToFile(PolicyJson, *PolicyPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));

    UValidateBuildCsCommandlet* Commandlet = NewObject<UValidateBuildCsCommandlet>();
    const int32 ExitCode = Commandlet->Main(FString::Printf(
        TEXT("-Policy=\"%s\" -Output=\"%s\""), *PolicyPath, *ReportPath));

    TestEqual(TEXT("invalid dependency exit code"), ExitCode,
        static_cast<int32>(ECommandForgeExitCode::SpecParseFailed));

    TSharedPtr<FJsonObject> RootObject;
    TestTrue(TEXT("invalid dependency report JSON 파싱"), LoadValidateBuildCsJsonObject(ReportPath, RootObject));
    TestFalse(TEXT("invalid dependency ok false"), RootObject->GetBoolField(TEXT("ok")));

    const TArray<TSharedPtr<FJsonValue>>* Errors = nullptr;
    TestTrue(TEXT("invalid dependency errors 배열 존재"),
        RootObject->TryGetArrayField(TEXT("errors"), Errors));
    bool bFoundInvalidDependency = false;
    if (Errors != nullptr)
    {
        for (const TSharedPtr<FJsonValue>& ErrorValue : *Errors)
        {
            const TSharedPtr<FJsonObject>* ErrorObject = nullptr;
            if (ErrorValue->TryGetObject(ErrorObject) && ErrorObject != nullptr &&
                (*ErrorObject)->GetStringField(TEXT("code")) == TEXT("INVALID_BUILD_DEPENDENCY"))
            {
                bFoundInvalidDependency = true;
                break;
            }
        }
    }
    TestTrue(TEXT("invalid dependency error"), bFoundInvalidDependency);
    return true;
}

bool FValidateBuildCsMultipleAddRangeBlocksTest::RunTest(const FString& Parameters)
{
    const FString PolicyPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("CodexReports"), TEXT("test_buildcs_policy_multi_addrange.json"));
    const FString ReportPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("CodexReports"), TEXT("test_buildcs_policy_multi_addrange_report.json"));

    IFileManager::Get().Delete(*PolicyPath);
    IFileManager::Get().Delete(*ReportPath);
    TestTrue(TEXT("multi AddRange Build.cs module 생성"), CreateValidateBuildCsModuleWithMultipleAddRanges());
    TestTrue(TEXT("multi AddRange policy 저장"), SaveValidateBuildCsMultiAddRangePolicy(PolicyPath));

    UValidateBuildCsCommandlet* Commandlet = NewObject<UValidateBuildCsCommandlet>();
    const int32 ExitCode = Commandlet->Main(FString::Printf(
        TEXT("-Policy=\"%s\" -Output=\"%s\""), *PolicyPath, *ReportPath));

    TestEqual(TEXT("multi AddRange policy exit code"), ExitCode, 0);

    TSharedPtr<FJsonObject> RootObject;
    TestTrue(TEXT("multi AddRange report JSON 파싱"), LoadValidateBuildCsJsonObject(ReportPath, RootObject));
    TestTrue(TEXT("multi AddRange ok true"), RootObject->GetBoolField(TEXT("ok")));
    TestEqual(TEXT("multi AddRange no issues"), RootObject->GetArrayField(TEXT("issues")).Num(), 0);

    DeleteValidateBuildCsTestModule();
    return true;
}
