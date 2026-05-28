#include "Misc/AutomationTest.h"
#include "Commandlets/AssetSnapshotCommandlet.h"
#include "CommandForgeTypes.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/UObjectGlobals.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAssetSnapshotCommandletTest,
    "UECommandForge.Commandlets.AssetSnapshot.WritesAssetSnapshot",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAssetSnapshotCommandletTest::RunTest(const FString& Parameters)
{
    const FString OutPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("CodexReports"), TEXT("test_asset_snapshot.json"));
    IFileManager::Get().Delete(*OutPath);

    UAssetSnapshotCommandlet* Commandlet = NewObject<UAssetSnapshotCommandlet>();
    const int32 ExitCode = Commandlet->Main(
        FString::Printf(TEXT("-Output=\"%s\" -RootPaths=\"/Game/Tests\""), *OutPath));

    TestEqual(TEXT("snapshot exit code"), ExitCode, 0);

    FString Content;
    TestTrue(TEXT("snapshot report 파일 존재"), FFileHelper::LoadFileToString(Content, *OutPath));
    TestTrue(TEXT("snapshot report ok true"), Content.Contains(TEXT("\"ok\": true")));
    TestTrue(TEXT("commandlet 이름 포함"),
        Content.Contains(TEXT("\"commandlet\": \"AssetSnapshot\"")));
    TestTrue(TEXT("assets 배열 포함"), Content.Contains(TEXT("\"assets\"")));
    TestTrue(TEXT("asset_name 포함"), Content.Contains(TEXT("\"asset_name\"")));
    TestTrue(TEXT("package_path 포함"), Content.Contains(TEXT("\"package_path\"")));
    TestTrue(TEXT("object_path 포함"), Content.Contains(TEXT("\"object_path\"")));
    TestTrue(TEXT("asset_class 포함"), Content.Contains(TEXT("\"asset_class\"")));
    TestTrue(TEXT("package_name 포함"), Content.Contains(TEXT("\"package_name\"")));
    TestTrue(TEXT("disk_path 포함"), Content.Contains(TEXT("\"disk_path\"")));
    TestTrue(TEXT("is_redirector 포함"), Content.Contains(TEXT("\"is_redirector\"")));
    TestTrue(TEXT("package_dirty 포함"), Content.Contains(TEXT("\"package_dirty\"")));
    TestTrue(TEXT("dependencies 포함"), Content.Contains(TEXT("\"dependencies\"")));
    TestTrue(TEXT("referencers 포함"), Content.Contains(TEXT("\"referencers\"")));
    return true;
}
