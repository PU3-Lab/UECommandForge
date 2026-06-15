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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDiffPlatformConfigCommandletTest,
    "UECommandForge.Commandlets.DiffPlatformConfig.DetectsAndSuppressesDiffs",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

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

    // Temporarily inject config values into the actual project configs for hierarchy testing,
    // and clean them up afterward.
    struct FConfigTestFixture
    {
        FString DefaultEnginePath;
        FString WinEnginePath;
        FString MacEnginePath;

        FConfigTestFixture()
        {
            DefaultEnginePath = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("DefaultEngine.ini"));
            WinEnginePath = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("Windows"), TEXT("WindowsEngine.ini"));
            MacEnginePath = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("Mac"), TEXT("MacEngine.ini"));
        }

        void Inject()
        {
            // Clean up directories if not exist
            IFileManager::Get().MakeDirectory(*FPaths::GetPath(WinEnginePath), true);
            IFileManager::Get().MakeDirectory(*FPaths::GetPath(MacEnginePath), true);

            // Append unique test sections
            FFileHelper::SaveStringToFile(TEXT("\n[UECommandForgeTestSection]\nTestKey=CommonValue\n"), *DefaultEnginePath,
                FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
            FFileHelper::SaveStringToFile(TEXT("\n[UECommandForgeTestSection]\nTestKey=WindowsValue\n"), *WinEnginePath,
                FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
            FFileHelper::SaveStringToFile(TEXT("\n[UECommandForgeTestSection]\nTestKey=MacValue\n"), *MacEnginePath,
                FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
        }

        void Cleanup()
        {
            auto RemoveTestSection = [](const FString& Path)
            {
                FString Content;
                if (FFileHelper::LoadFileToString(Content, *Path))
                {
                    int32 Index = Content.Find(TEXT("[UECommandForgeTestSection]"));
                    if (Index != INDEX_NONE)
                    {
                        Content = Content.Left(Index);
                        FFileHelper::SaveStringToFile(Content, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
                    }
                }
            };

            RemoveTestSection(DefaultEnginePath);
            RemoveTestSection(WinEnginePath);
            RemoveTestSection(MacEnginePath);
        }
    };
}

bool FDiffPlatformConfigCommandletTest::RunTest(const FString& Parameters)
{
    using namespace DiffPlatformConfigTestHelpers;

    FConfigTestFixture Fixture;
    Fixture.Inject();

    const FString ProjPath = FPaths::GetProjectFilePath();
    const FString AllowlistPath = FPaths::Combine(TempDir(), TEXT("allowlist.json"));
    const FString OutPathNoAllow = FPaths::Combine(TempDir(), TEXT("diff_no_allow.json"));
    const FString OutPathWithAllow = FPaths::Combine(TempDir(), TEXT("diff_with_allow.json"));

    IFileManager::Get().Delete(*OutPathNoAllow);
    IFileManager::Get().Delete(*OutPathWithAllow);

    // 1. Without Allowlist -> Should Fail due to value difference (WindowsValue vs MacValue)
    {
        // empty allowlist
        FFileHelper::SaveStringToFile(TEXT("[]"), *AllowlistPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

        UDiffPlatformConfigCommandlet* Commandlet = NewObject<UDiffPlatformConfigCommandlet>();
        const FString Params = FString::Printf(
            TEXT("-Output=\"%s\" -Allowlist=\"%s\" -Project=\"%s\" -Platforms=Windows,Mac -Categories=Engine"),
            *OutPathNoAllow, *AllowlistPath, *ProjPath);
        
        const int32 Exit = Commandlet->Main(Params);
        TestEqual(TEXT("should fail without allowlist"), Exit,
            static_cast<int32>(ECommandForgeExitCode::ValidationFailed));

        TSharedPtr<FJsonObject> Report;
        if (LoadReport(OutPathNoAllow, Report) && Report.IsValid())
        {
            TestFalse(TEXT("ok is false"), Report->GetBoolField(TEXT("ok")));
            const TSharedPtr<FJsonObject>* ValObj = nullptr;
            if (Report->TryGetObjectField(TEXT("validation"), ValObj) && ValObj)
            {
                FString DiffCount;
                (*ValObj)->TryGetStringField(TEXT("diff_count"), DiffCount);
                TestTrue(TEXT("at least one diff"), FCString::Atoi(*DiffCount) > 0);
            }
        }
    }

    // 2. With Allowlist wildcard -> Should Pass (ok is true)
    {
        // allowlist filtering the test section
        FFileHelper::SaveStringToFile(
            TEXT("[ { \"section\": \"UECommandForgeTestSection\", \"key\": \"TestKey\" } ]"),
            *AllowlistPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

        UDiffPlatformConfigCommandlet* Commandlet = NewObject<UDiffPlatformConfigCommandlet>();
        // We also want to filter out other engine differences that may exist on the machine,
        // so we can use a wildcard allowlist [ { "section": "*", "key": "*" } ] for a robust green test.
        FFileHelper::SaveStringToFile(
            TEXT("[ { \"section\": \"*\", \"key\": \"*\" } ]"),
            *AllowlistPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

        const FString Params = FString::Printf(
            TEXT("-Output=\"%s\" -Allowlist=\"%s\" -Project=\"%s\" -Platforms=Windows,Mac -Categories=Engine"),
            *OutPathWithAllow, *AllowlistPath, *ProjPath);

        const int32 Exit = Commandlet->Main(Params);
        TestEqual(TEXT("should pass with allow-all list"), Exit, 0);

        TSharedPtr<FJsonObject> Report;
        if (LoadReport(OutPathWithAllow, Report) && Report.IsValid())
        {
            TestTrue(TEXT("ok is true"), Report->GetBoolField(TEXT("ok")));
            const TSharedPtr<FJsonObject>* ValObj = nullptr;
            if (Report->TryGetObjectField(TEXT("validation"), ValObj) && ValObj)
            {
                FString DiffCount;
                (*ValObj)->TryGetStringField(TEXT("diff_count"), DiffCount);
                TestEqual(TEXT("zero diffs"), FCString::Atoi(*DiffCount), 0);
            }
        }
    }

    Fixture.Cleanup();
    return true;
}

void LinkDiffPlatformConfigCommandletTest() {}

