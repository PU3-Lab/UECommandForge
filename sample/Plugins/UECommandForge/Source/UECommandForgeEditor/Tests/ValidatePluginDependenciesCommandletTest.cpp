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

    inline bool ReportHasIssueCode(const TSharedPtr<FJsonObject>& Report, const FString& Code)
    {
        const TArray<TSharedPtr<FJsonValue>>* Issues = nullptr;
        if (!Report->TryGetArrayField(TEXT("issues"), Issues) || !Issues) { return false; }
        for (const TSharedPtr<FJsonValue>& V : *Issues)
        {
            const TSharedPtr<FJsonObject>* Obj = nullptr;
            if (V->TryGetObject(Obj) && Obj)
            {
                FString C;
                if ((*Obj)->TryGetStringField(TEXT("code"), C) && C == Code) { return true; }
            }
        }
        return false;
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

    inline bool SaveEditorOnlyPlugin(const FString& ProjectDir, const FString& PluginName)
    {
        const FString PluginPath = FPaths::Combine(ProjectDir, TEXT("Plugins"), PluginName,
            PluginName + TEXT(".uplugin"));
        const FString Json = FString::Printf(TEXT(R"({
  "FileVersion": 3,
  "FriendlyName": "%s",
  "Modules": [ { "Name": "%sEditor", "Type": "Editor", "LoadingPhase": "Default" } ]
})"), *PluginName, *PluginName);
        FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FPaths::GetPath(PluginPath));
        return FFileHelper::SaveStringToFile(Json, *PluginPath,
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    }

    inline bool SaveUProjectWithPlugin(const FString& Path, const FString& PluginName)
    {
        const FString Json = FString::Printf(TEXT(R"({
  "FileVersion": 3, "EngineAssociation": "5.7",
  "Plugins": [ { "Name": "%s", "Enabled": true } ]
})"), *PluginName);
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
        FString CommandVal;
        TestTrue(TEXT("report has command key"), Report->TryGetStringField(TEXT("command"), CommandVal));
        TestEqual(TEXT("command is ValidatePluginDependencies"), CommandVal, TEXT("ValidatePluginDependencies"));

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FValidatePluginDepsRequiredRulesTest,
    "UECommandForge.Commandlets.ValidatePluginDeps.FlagsMissingAndDisabledRequired",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FValidatePluginDepsRequiredRulesTest::RunTest(const FString& Parameters)
{
    using namespace PluginDepsTestHelpers;
    const FString ProjPath = FPaths::Combine(TempDir(), TEXT("Req"), TEXT("T.uproject"));
    const FString PolicyPath = FPaths::Combine(TempDir(), TEXT("req.policy.json"));
    const FString OutPath = FPaths::Combine(TempDir(), TEXT("req.json"));
    TestTrue(TEXT("uproject"), SaveUProject(ProjPath)); // Niagara enabled, OnlineSubsystem disabled
    TestTrue(TEXT("policy"), SavePolicy(PolicyPath, TEXT("plugin_dependency_policy"),
        TEXT(R"(["OnlineSubsystem","NonExistentPluginForTest"])"), TEXT("[]")));

    UValidatePluginDependenciesCommandlet* Commandlet =
        NewObject<UValidatePluginDependenciesCommandlet>();
    const int32 ExitCode = Commandlet->Main(FString::Printf(
        TEXT("-Output=\"%s\" -Policy=\"%s\" -Project=\"%s\""), *OutPath, *PolicyPath, *ProjPath));

    TestEqual(TEXT("exit ValidationFailed"), ExitCode,
        static_cast<int32>(ECommandForgeExitCode::ValidationFailed));
    TSharedPtr<FJsonObject> Report;
    TestTrue(TEXT("report"), LoadReport(OutPath, Report));
    if (Report.IsValid())
    {
        TestFalse(TEXT("ok false"), Report->GetBoolField(TEXT("ok")));
        TestTrue(TEXT("disabled flagged"), ReportHasIssueCode(Report, TEXT("PLUGIN_REQUIRED_DISABLED")));
        TestTrue(TEXT("missing flagged"), ReportHasIssueCode(Report, TEXT("PLUGIN_REQUIRED_MISSING")));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FValidatePluginDepsForbiddenShippingTest,
    "UECommandForge.Commandlets.ValidatePluginDeps.FlagsForbiddenOnlyInShipping",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FValidatePluginDepsForbiddenShippingTest::RunTest(const FString& Parameters)
{
    using namespace PluginDepsTestHelpers;
    const FString ProjPath = FPaths::Combine(TempDir(), TEXT("Forb"), TEXT("T.uproject"));
    const FString PolicyPath = FPaths::Combine(TempDir(), TEXT("forb.policy.json"));
    TestTrue(TEXT("uproject"), SaveUProject(ProjPath)); // Niagara enabled
    TestTrue(TEXT("policy"), SavePolicy(PolicyPath, TEXT("plugin_dependency_policy"),
        TEXT("[]"), TEXT(R"(["Niagara"])")));

    {
        const FString OutDev = FPaths::Combine(TempDir(), TEXT("forb_dev.json"));
        UValidatePluginDependenciesCommandlet* C = NewObject<UValidatePluginDependenciesCommandlet>();
        const int32 Exit = C->Main(FString::Printf(
            TEXT("-Output=\"%s\" -Policy=\"%s\" -Project=\"%s\" -Configuration=Development"),
            *OutDev, *PolicyPath, *ProjPath));
        TestEqual(TEXT("dev ok"), Exit, 0);
    }
    {
        const FString OutShip = FPaths::Combine(TempDir(), TEXT("forb_ship.json"));
        UValidatePluginDependenciesCommandlet* C = NewObject<UValidatePluginDependenciesCommandlet>();
        const int32 Exit = C->Main(FString::Printf(
            TEXT("-Output=\"%s\" -Policy=\"%s\" -Project=\"%s\" -Configuration=Shipping"),
            *OutShip, *PolicyPath, *ProjPath));
        TestEqual(TEXT("ship fail"), Exit,
            static_cast<int32>(ECommandForgeExitCode::ValidationFailed));
        TSharedPtr<FJsonObject> Report;
        if (LoadReport(OutShip, Report) && Report.IsValid())
        {
            TestTrue(TEXT("forbidden flagged"),
                ReportHasIssueCode(Report, TEXT("PLUGIN_FORBIDDEN_IN_SHIPPING_ENABLED")));
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FValidatePluginDepsEditorOnlyNotForbiddenTest,
    "UECommandForge.Commandlets.ValidatePluginDeps.EditorOnlyModuleNotForbiddenInShipping",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FValidatePluginDepsEditorOnlyNotForbiddenTest::RunTest(const FString& Parameters)
{
    using namespace PluginDepsTestHelpers;
    const FString ProjDir = FPaths::Combine(TempDir(), TEXT("EdOnly"));
    const FString ProjPath = FPaths::Combine(ProjDir, TEXT("T.uproject"));
    const FString PolicyPath = FPaths::Combine(TempDir(), TEXT("edonly.policy.json"));
    const FString OutPath = FPaths::Combine(TempDir(), TEXT("edonly.json"));
    TestTrue(TEXT("uproject"), SaveUProjectWithPlugin(ProjPath, TEXT("MyEditorTool")));
    TestTrue(TEXT("uplugin"), SaveEditorOnlyPlugin(ProjDir, TEXT("MyEditorTool")));
    TestTrue(TEXT("policy"), SavePolicy(PolicyPath, TEXT("plugin_dependency_policy"),
        TEXT("[]"), TEXT(R"(["MyEditorTool"])")));

    UValidatePluginDependenciesCommandlet* C = NewObject<UValidatePluginDependenciesCommandlet>();
    const int32 Exit = C->Main(FString::Printf(
        TEXT("-Output=\"%s\" -Policy=\"%s\" -Project=\"%s\" -Configuration=Shipping"),
        *OutPath, *PolicyPath, *ProjPath));

    TestEqual(TEXT("ok exit 0"), Exit, 0);
    TSharedPtr<FJsonObject> Report;
    if (LoadReport(OutPath, Report) && Report.IsValid())
    {
        TestFalse(TEXT("forbidden NOT flagged"),
            ReportHasIssueCode(Report, TEXT("PLUGIN_FORBIDDEN_IN_SHIPPING_ENABLED")));
    }
    return true;
}

void LinkValidatePluginDependenciesCommandletTest() {}

