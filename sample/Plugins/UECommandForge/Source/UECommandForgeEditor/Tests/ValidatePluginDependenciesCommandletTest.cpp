#include "Misc/AutomationTest.h"
#include "Commandlets/ValidatePluginDependenciesCommandlet.h"
#include "CommandForgeTypes.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FValidatePluginDepsMissingPolicyArgTest,
    "UECommandForge.Commandlets.ValidatePluginDeps.FailsWithoutPolicyArg",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace PluginDepsTestHelpers
{
    inline FString TempDir()
    {
        return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Tmp"), TEXT("PluginDepsTest"));
    }

    inline bool LoadReport(const FString& Path, TSharedPtr<FJsonObject>& OutObject)
    {
        FString Content;
        if (!FFileHelper::LoadFileToString(Content, *Path)) { return false; }
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
        return FJsonSerializer::Deserialize(Reader, OutObject) && OutObject.IsValid();
    }

    inline bool SavePolicy(const FString& Path, const FString& Kind,
        const FString& RequiredJsonArray, const FString& ForbiddenJsonArray)
    {
        const FString Json = FString::Printf(TEXT(R"({
  "version": "1",
  "kind": "%s",
  "required": %s,
  "forbiddenInShipping": %s,
  "optional": [],
  "allowedEditorOnly": []
})"), *Kind, *RequiredJsonArray, *ForbiddenJsonArray);
        FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FPaths::GetPath(Path));
        return FFileHelper::SaveStringToFile(Json, *Path,
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    }

    inline bool SaveUProject(const FString& Path)
    {
        const FString Json = TEXT(R"({
  "FileVersion": 3,
  "EngineAssociation": "5.7",
  "Plugins": [
    { "Name": "Niagara", "Enabled": true },
    { "Name": "OnlineSubsystem", "Enabled": false }
  ]
})");
        FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FPaths::GetPath(Path));
        return FFileHelper::SaveStringToFile(Json, *Path,
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    }
}

bool FValidatePluginDepsMissingPolicyArgTest::RunTest(const FString& Parameters)
{
    using namespace PluginDepsTestHelpers;
    const FString OutPath = FPaths::Combine(TempDir(), TEXT("missing_policy.json"));
    IFileManager::Get().Delete(*OutPath);

    UValidatePluginDependenciesCommandlet* Commandlet =
        NewObject<UValidatePluginDependenciesCommandlet>();
    const FString Params = FString::Printf(TEXT("-Output=\"%s\""), *OutPath);
    const int32 ExitCode = Commandlet->Main(Params);

    TestEqual(TEXT("exit code is SpecParseFailed"), ExitCode,
        static_cast<int32>(ECommandForgeExitCode::SpecParseFailed));

    TSharedPtr<FJsonObject> Report;
    TestTrue(TEXT("report written"), LoadReport(OutPath, Report));
    if (Report.IsValid())
    {
        TestFalse(TEXT("ok is false"), Report->GetBoolField(TEXT("ok")));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FValidatePluginDepsRejectsWrongKindTest,
    "UECommandForge.Commandlets.ValidatePluginDeps.RejectsWrongPolicyKind",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FValidatePluginDepsRejectsWrongKindTest::RunTest(const FString& Parameters)
{
    using namespace PluginDepsTestHelpers;
    const FString PolicyPath = FPaths::Combine(TempDir(), TEXT("wrong_kind.policy.json"));
    const FString OutPath = FPaths::Combine(TempDir(), TEXT("wrong_kind.json"));
    TestTrue(TEXT("policy saved"),
        SavePolicy(PolicyPath, TEXT("buildcs_policy"), TEXT("[]"), TEXT("[]")));

    UValidatePluginDependenciesCommandlet* Commandlet =
        NewObject<UValidatePluginDependenciesCommandlet>();
    const FString Params = FString::Printf(
        TEXT("-Output=\"%s\" -Policy=\"%s\""), *OutPath, *PolicyPath);
    const int32 ExitCode = Commandlet->Main(Params);

    TestEqual(TEXT("exit code SpecParseFailed"), ExitCode,
        static_cast<int32>(ECommandForgeExitCode::SpecParseFailed));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FValidatePluginDepsReadsEnabledPluginsTest,
    "UECommandForge.Commandlets.ValidatePluginDeps.ReadsEnabledPlugins",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FValidatePluginDepsReadsEnabledPluginsTest::RunTest(const FString& Parameters)
{
    using namespace PluginDepsTestHelpers;
    const FString ProjPath = FPaths::Combine(TempDir(), TEXT("Read"), TEXT("T.uproject"));
    const FString PolicyPath = FPaths::Combine(TempDir(), TEXT("read.policy.json"));
    const FString OutPath = FPaths::Combine(TempDir(), TEXT("read.json"));
    TestTrue(TEXT("uproject saved"), SaveUProject(ProjPath));
    TestTrue(TEXT("policy saved"),
        SavePolicy(PolicyPath, TEXT("plugin_dependency_policy"), TEXT("[]"), TEXT("[]")));

    UValidatePluginDependenciesCommandlet* Commandlet =
        NewObject<UValidatePluginDependenciesCommandlet>();
    const FString Params = FString::Printf(
        TEXT("-Output=\"%s\" -Policy=\"%s\" -Project=\"%s\""), *OutPath, *PolicyPath, *ProjPath);
    Commandlet->Main(Params);

    TSharedPtr<FJsonObject> Report;
    TestTrue(TEXT("report"), LoadReport(OutPath, Report));
    if (Report.IsValid())
    {
        const TSharedPtr<FJsonObject>* Validation = nullptr;
        if (Report->TryGetObjectField(TEXT("validation"), Validation) && Validation)
        {
            FString Enabled;
            (*Validation)->TryGetStringField(TEXT("enabled_plugin_count"), Enabled);
            TestEqual(TEXT("one enabled plugin"), Enabled, TEXT("1"));
        }
        else
        {
            AddError(TEXT("validation object missing in report"));
        }
    }
    return true;
}
