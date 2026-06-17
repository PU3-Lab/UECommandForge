#include "Misc/AutomationTest.h"
#include "Commandlets/DiffPlatformConfigCommandlet.h"
#include "CommandForgeTypes.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace DiffPlatformConfigTestHelpers
{
    inline FString TempDir()
    {
        return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Tmp"), TEXT("ConfigDiffTest"));
    }

    inline bool LoadReport(const FString& Path, TSharedPtr<FJsonObject>& OutObject)
    {
        FString Content;
        if (!FFileHelper::LoadFileToString(Content, *Path)) { return false; }
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
        return FJsonSerializer::Deserialize(Reader, OutObject) && OutObject.IsValid();
    }

    struct FConfigTestFixture
    {
        FString DefaultIniPath;
        FString WinIniPath;
        FString MacIniPath;
        FString Category;

        FConfigTestFixture(const FString& InCategory = TEXT("UECommandForgeTest"))
            : Category(InCategory)
        {
            DefaultIniPath = FPaths::Combine(FPaths::ProjectConfigDir(), FString::Printf(TEXT("Default%s.ini"), *Category));
            WinIniPath = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("Windows"), FString::Printf(TEXT("Windows%s.ini"), *Category));
            MacIniPath = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("Mac"), FString::Printf(TEXT("Mac%s.ini"), *Category));
        }

        void InjectValueDiff()
        {
            IFileManager::Get().MakeDirectory(*FPaths::GetPath(WinIniPath), true);
            IFileManager::Get().MakeDirectory(*FPaths::GetPath(MacIniPath), true);

            FFileHelper::SaveStringToFile(TEXT("[UECommandForgeTestSection]\nTestKey=CommonValue\n"), *DefaultIniPath,
                FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
            FFileHelper::SaveStringToFile(TEXT("[UECommandForgeTestSection]\nTestKey=WindowsValue\n"), *WinIniPath,
                FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
            FFileHelper::SaveStringToFile(TEXT("[UECommandForgeTestSection]\nTestKey=MacValue\n"), *MacIniPath,
                FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        }

        void InjectMissingKey()
        {
            IFileManager::Get().MakeDirectory(*FPaths::GetPath(WinIniPath), true);
            IFileManager::Get().MakeDirectory(*FPaths::GetPath(MacIniPath), true);

            // Default 및 Mac 에는 TestKey를 적지 않아 Mac에서 완전히 missing 상태로 로드되게 유발
            FFileHelper::SaveStringToFile(TEXT("[UECommandForgeTestSection]\n"), *DefaultIniPath,
                FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
            FFileHelper::SaveStringToFile(TEXT("[UECommandForgeTestSection]\nTestKey=WindowsValue\n"), *WinIniPath,
                FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
            FFileHelper::SaveStringToFile(TEXT("[UECommandForgeTestSection]\n"), *MacIniPath,
                FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        }

        void InjectArrayDiff()
        {
            IFileManager::Get().MakeDirectory(*FPaths::GetPath(WinIniPath), true);
            IFileManager::Get().MakeDirectory(*FPaths::GetPath(MacIniPath), true);

            FFileHelper::SaveStringToFile(TEXT("[UECommandForgeTestSection]\n+Paths=CommonPath\n"), *DefaultIniPath,
                FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
            FFileHelper::SaveStringToFile(TEXT("[UECommandForgeTestSection]\n+Paths=WindowsOnlyPath\n"), *WinIniPath,
                FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
            FFileHelper::SaveStringToFile(TEXT("[UECommandForgeTestSection]\n+Paths=MacOnlyPath\n"), *MacIniPath,
                FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        }

        void Cleanup()
        {
            IFileManager::Get().Delete(*DefaultIniPath);
            IFileManager::Get().Delete(*WinIniPath);
            IFileManager::Get().Delete(*MacIniPath);
        }
    };
}

// 1. 값 차이 감지 테스트
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDiffPlatformConfigDetectsValueDiffTest,
    "UECommandForge.Commandlets.DiffPlatformConfig.DetectsValueDiff",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiffPlatformConfigDetectsValueDiffTest::RunTest(const FString& Parameters)
{
    using namespace DiffPlatformConfigTestHelpers;
    FConfigTestFixture Fixture;
    Fixture.Cleanup();
    Fixture.InjectValueDiff();

    const FString ProjPath = FPaths::GetProjectFilePath();
    const FString AllowlistPath = FPaths::Combine(TempDir(), TEXT("allowlist_none.json"));
    const FString OutPath = FPaths::Combine(TempDir(), TEXT("diff_val_fail.json"));

    IFileManager::Get().Delete(*OutPath);
    FFileHelper::SaveStringToFile(TEXT("[]"), *AllowlistPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

    UDiffPlatformConfigCommandlet* Commandlet = NewObject<UDiffPlatformConfigCommandlet>();
    const FString Params = FString::Printf(
        TEXT("-Output=\"%s\" -Allowlist=\"%s\" -Project=\"%s\" -Platforms=Windows,Mac -Categories=UECommandForgeTest"),
        *OutPath, *AllowlistPath, *ProjPath);

    const int32 Exit = Commandlet->Main(Params);
    TestEqual(TEXT("should fail due to value mismatch"), Exit, static_cast<int32>(ECommandForgeExitCode::ValidationFailed));

    TSharedPtr<FJsonObject> Report;
    if (LoadReport(OutPath, Report) && Report.IsValid())
    {
        TestFalse(TEXT("ok is false"), Report->GetBoolField(TEXT("ok")));
    }

    Fixture.Cleanup();
    return true;
}

// 2. 키 누락 감지 테스트
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDiffPlatformConfigDetectsMissingKeyTest,
    "UECommandForge.Commandlets.DiffPlatformConfig.DetectsMissingKey",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiffPlatformConfigDetectsMissingKeyTest::RunTest(const FString& Parameters)
{
    using namespace DiffPlatformConfigTestHelpers;
    FConfigTestFixture Fixture;
    Fixture.Cleanup();
    Fixture.InjectMissingKey();

    const FString ProjPath = FPaths::GetProjectFilePath();
    const FString AllowlistPath = FPaths::Combine(TempDir(), TEXT("allowlist_none.json"));
    const FString OutPath = FPaths::Combine(TempDir(), TEXT("diff_key_fail.json"));

    IFileManager::Get().Delete(*OutPath);
    FFileHelper::SaveStringToFile(TEXT("[]"), *AllowlistPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

    UDiffPlatformConfigCommandlet* Commandlet = NewObject<UDiffPlatformConfigCommandlet>();
    const FString Params = FString::Printf(
        TEXT("-Output=\"%s\" -Allowlist=\"%s\" -Project=\"%s\" -Platforms=Windows,Mac -Categories=UECommandForgeTest"),
        *OutPath, *AllowlistPath, *ProjPath);

    const int32 Exit = Commandlet->Main(Params);
    TestEqual(TEXT("should fail due to missing key"), Exit, static_cast<int32>(ECommandForgeExitCode::ValidationFailed));

    TSharedPtr<FJsonObject> Report;
    if (LoadReport(OutPath, Report) && Report.IsValid())
    {
        TestFalse(TEXT("ok is false"), Report->GetBoolField(TEXT("ok")));
        const TArray<TSharedPtr<FJsonValue>>* Issues = nullptr;
        if (Report->TryGetArrayField(TEXT("issues"), Issues) && Issues)
        {
            bool bFoundMissing = false;
            for (const auto& Val : *Issues)
            {
                TSharedPtr<FJsonObject> Obj = Val->AsObject();
                if (Obj.IsValid() && Obj->GetStringField(TEXT("code")) == TEXT("CONFIG_PLATFORM_KEY_MISSING"))
                {
                    bFoundMissing = true;
                }
            }
            TestTrue(TEXT("CONFIG_PLATFORM_KEY_MISSING reported"), bFoundMissing);
        }
    }

    Fixture.Cleanup();
    return true;
}

// 3. Allowlist 매칭 및 Suppression 검증 테스트
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDiffPlatformConfigFiltersWithAllowlistTest,
    "UECommandForge.Commandlets.DiffPlatformConfig.FiltersWithAllowlist",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiffPlatformConfigFiltersWithAllowlistTest::RunTest(const FString& Parameters)
{
    using namespace DiffPlatformConfigTestHelpers;
    FConfigTestFixture Fixture;
    Fixture.Cleanup();
    Fixture.InjectValueDiff();

    const FString ProjPath = FPaths::GetProjectFilePath();
    const FString AllowlistPath = FPaths::Combine(TempDir(), TEXT("allowlist_exact.json"));
    const FString OutPath = FPaths::Combine(TempDir(), TEXT("diff_allowlist_pass.json"));

    IFileManager::Get().Delete(*OutPath);
    FFileHelper::SaveStringToFile(
        TEXT("[ { \"section\": \"UECommandForgeTestSection\", \"key\": \"TestKey\" } ]"),
        *AllowlistPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

    UDiffPlatformConfigCommandlet* Commandlet = NewObject<UDiffPlatformConfigCommandlet>();
    const FString Params = FString::Printf(
        TEXT("-Output=\"%s\" -Allowlist=\"%s\" -Project=\"%s\" -Platforms=Windows,Mac -Categories=UECommandForgeTest"),
        *OutPath, *AllowlistPath, *ProjPath);

    const int32 Exit = Commandlet->Main(Params);
    TestEqual(TEXT("should pass with allowlist matching the test key"), Exit, 0);

    TSharedPtr<FJsonObject> Report;
    if (LoadReport(OutPath, Report) && Report.IsValid())
    {
        TestTrue(TEXT("ok is true"), Report->GetBoolField(TEXT("ok")));
    }

    Fixture.Cleanup();
    return true;
}

// 4. 다중 값(배열 키) 차이 검증 테스트
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDiffPlatformConfigArrayKeyDiffTest,
    "UECommandForge.Commandlets.DiffPlatformConfig.DetectsArrayKeyDiff",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiffPlatformConfigArrayKeyDiffTest::RunTest(const FString& Parameters)
{
    using namespace DiffPlatformConfigTestHelpers;
    FConfigTestFixture Fixture;
    Fixture.Cleanup();
    Fixture.InjectArrayDiff();

    const FString ProjPath = FPaths::GetProjectFilePath();
    const FString AllowlistPath = FPaths::Combine(TempDir(), TEXT("allowlist_none.json"));
    const FString OutPath = FPaths::Combine(TempDir(), TEXT("diff_array_fail.json"));

    IFileManager::Get().Delete(*OutPath);
    FFileHelper::SaveStringToFile(TEXT("[]"), *AllowlistPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

    UDiffPlatformConfigCommandlet* Commandlet = NewObject<UDiffPlatformConfigCommandlet>();
    const FString Params = FString::Printf(
        TEXT("-Output=\"%s\" -Allowlist=\"%s\" -Project=\"%s\" -Platforms=Windows,Mac -Categories=UECommandForgeTest"),
        *OutPath, *AllowlistPath, *ProjPath);

    const int32 Exit = Commandlet->Main(Params);
    TestEqual(TEXT("should fail due to array value mismatch"), Exit, static_cast<int32>(ECommandForgeExitCode::ValidationFailed));

    TSharedPtr<FJsonObject> Report;
    if (LoadReport(OutPath, Report) && Report.IsValid())
    {
        TestFalse(TEXT("ok is false"), Report->GetBoolField(TEXT("ok")));
    }

    Fixture.Cleanup();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDiffPlatformConfigExternalProjectTest,
    "UECommandForge.Commandlets.DiffPlatformConfig.ExternalProjectTargeting",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiffPlatformConfigExternalProjectTest::RunTest(const FString& Parameters)
{
    using namespace DiffPlatformConfigTestHelpers;

    const FString ExtProjDir = FPaths::Combine(TempDir(), TEXT("ExtProj"));
    const FString ExtProjPath = FPaths::Combine(ExtProjDir, TEXT("T.uproject"));
    const FString ExtConfigDir = FPaths::Combine(ExtProjDir, TEXT("Config"));
    
    const FString UProjJson = TEXT(R"({ "FileVersion": 3, "EngineAssociation": "5.7" })");
    FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*ExtProjDir);
    FFileHelper::SaveStringToFile(UProjJson, *ExtProjPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

    const FString DefaultIni = FPaths::Combine(ExtConfigDir, TEXT("DefaultUECommandForgeTest.ini"));
    const FString WinIni = FPaths::Combine(ExtConfigDir, TEXT("Windows"), TEXT("WindowsUECommandForgeTest.ini"));
    const FString MacIni = FPaths::Combine(ExtConfigDir, TEXT("Mac"), TEXT("MacUECommandForgeTest.ini"));

    IFileManager::Get().MakeDirectory(*FPaths::GetPath(WinIni), true);
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(MacIni), true);

    FFileHelper::SaveStringToFile(TEXT("[UECommandForgeTestSection]\n"), *DefaultIni, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    FFileHelper::SaveStringToFile(TEXT("[UECommandForgeTestSection]\nExternalOnlyKey=WindowsValue\n"), *WinIni, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    FFileHelper::SaveStringToFile(TEXT("[UECommandForgeTestSection]\nExternalOnlyKey=MacValue\n"), *MacIni, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

    const FString AllowlistPath = FPaths::Combine(TempDir(), TEXT("allowlist_none_ext.json"));
    const FString OutPath = FPaths::Combine(TempDir(), TEXT("diff_ext_fail.json"));

    IFileManager::Get().Delete(*OutPath);
    FFileHelper::SaveStringToFile(TEXT("[]"), *AllowlistPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

    UDiffPlatformConfigCommandlet* Commandlet = NewObject<UDiffPlatformConfigCommandlet>();
    const FString Params = FString::Printf(
        TEXT("-Output=\"%s\" -Allowlist=\"%s\" -Project=\"%s\" -Platforms=Windows,Mac -Categories=UECommandForgeTest"),
        *OutPath, *AllowlistPath, *ExtProjPath);

    const int32 Exit = Commandlet->Main(Params);
    
    TestEqual(TEXT("should fail due to value mismatch in external project config"), Exit, static_cast<int32>(ECommandForgeExitCode::ValidationFailed));

    TSharedPtr<FJsonObject> Report;
    if (LoadReport(OutPath, Report) && Report.IsValid())
    {
        TestFalse(TEXT("ok is false for external project config diff"), Report->GetBoolField(TEXT("ok")));
        
        const TArray<TSharedPtr<FJsonValue>>* Issues = nullptr;
        if (Report->TryGetArrayField(TEXT("issues"), Issues) && Issues)
        {
            bool bFoundDiff = false;
            for (const auto& Val : *Issues)
            {
                TSharedPtr<FJsonObject> Obj = Val->AsObject();
                if (Obj.IsValid() && Obj->GetStringField(TEXT("code")) == TEXT("CONFIG_PLATFORM_VALUE_DIFF"))
                {
                    bFoundDiff = true;
                }
            }
            TestTrue(TEXT("CONFIG_PLATFORM_VALUE_DIFF reported for external project"), bFoundDiff);
        }
    }

    IFileManager::Get().Delete(*DefaultIni);
    IFileManager::Get().Delete(*WinIni);
    IFileManager::Get().Delete(*MacIni);
    IFileManager::Get().Delete(*ExtProjPath);
    IFileManager::Get().DeleteDirectory(*ExtConfigDir, false, true);
    IFileManager::Get().DeleteDirectory(*ExtProjDir, false, true);

    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDiffPlatformConfigEngineIniNotModifiedTest,
    "UECommandForge.Commandlets.DiffPlatformConfig.EngineIniNotModified",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiffPlatformConfigEngineIniNotModifiedTest::RunTest(const FString& Parameters)
{
    using namespace DiffPlatformConfigTestHelpers;

    const FString ConfigDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("Config"));
    TArray<FString> FoundIniFiles;
    IFileManager::Get().FindFilesRecursive(FoundIniFiles, *ConfigDir, TEXT("*.ini"), true, false);

    TMap<FString, FString> OriginalContents;
    for (const FString& IniPath : FoundIniFiles)
    {
        FString Content;
        if (FFileHelper::LoadFileToString(Content, *IniPath))
        {
            OriginalContents.Add(IniPath, Content);
        }
    }

    const FString ProjPath = FPaths::GetProjectFilePath();
    const FString AllowlistPath = FPaths::Combine(TempDir(), TEXT("allowlist_all_hash.json"));
    const FString OutPath = FPaths::Combine(TempDir(), TEXT("diff_hash_check.json"));

    IFileManager::Get().Delete(*OutPath);
    FFileHelper::SaveStringToFile(TEXT("[{\"section\":\"*\",\"key\":\"*\"}]"), *AllowlistPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

    UDiffPlatformConfigCommandlet* Commandlet = NewObject<UDiffPlatformConfigCommandlet>();
    const FString Params = FString::Printf(
        TEXT("-Output=\"%s\" -Allowlist=\"%s\" -Project=\"%s\" -Platforms=Windows,Mac -Categories=Engine"),
        *OutPath, *AllowlistPath, *ProjPath);

    Commandlet->Main(Params);

    for (const auto& Pair : OriginalContents)
    {
        FString CurrentContent;
        TestTrue(TEXT("Ini file should still exist"), IFileManager::Get().FileExists(*Pair.Key));
        if (FFileHelper::LoadFileToString(CurrentContent, *Pair.Key))
        {
            TestEqual(FString::Printf(TEXT("Config 파일 '%s'의 내용이 변하지 않아야 함 (F5 오염 방지 검증)"), *FPaths::GetCleanFilename(Pair.Key)),
                CurrentContent, Pair.Value);
        }
    }

    IFileManager::Get().Delete(*AllowlistPath);
    IFileManager::Get().Delete(*OutPath);
    return true;
}

void LinkDiffPlatformConfigCommandletTest() {}
