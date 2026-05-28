#include "Misc/AutomationTest.h"
#include "Commandlets/AssetSnapshotCommandlet.h"
#include "CommandForgeTypes.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
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

    TSharedPtr<FJsonObject> RootObject;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
    if (!TestTrue(TEXT("snapshot report JSON 파싱"), FJsonSerializer::Deserialize(Reader, RootObject)) ||
        !TestTrue(TEXT("snapshot report root object"), RootObject.IsValid()))
    {
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* Assets = nullptr;
    if (!TestTrue(TEXT("assets 배열 파싱"), RootObject->TryGetArrayField(TEXT("assets"), Assets)) ||
        !TestTrue(TEXT("asset snapshot 결과 존재"), Assets != nullptr && Assets->Num() > 0))
    {
        return false;
    }

    TSet<FString> PackageNames;
    bool bFoundMapWithUmapDiskPath = false;
    for (const TSharedPtr<FJsonValue>& AssetValue : *Assets)
    {
        const TSharedPtr<FJsonObject>* AssetObject = nullptr;
        if (!TestTrue(TEXT("asset object 파싱"), AssetValue->TryGetObject(AssetObject)) ||
            !TestTrue(TEXT("asset object 유효"), AssetObject != nullptr && AssetObject->IsValid()))
        {
            return false;
        }

        FString PackageName;
        TestTrue(TEXT("package_name field"), (*AssetObject)->TryGetStringField(TEXT("package_name"), PackageName));
        TestFalse(TEXT("package snapshot 중복 없음"), PackageNames.Contains(PackageName));
        PackageNames.Add(PackageName);

        FString AssetClass;
        FString DiskPath;
        (*AssetObject)->TryGetStringField(TEXT("asset_class"), AssetClass);
        (*AssetObject)->TryGetStringField(TEXT("disk_path"), DiskPath);
        if (AssetClass == TEXT("/Script/Engine.World") && DiskPath.EndsWith(TEXT(".umap")))
        {
            bFoundMapWithUmapDiskPath = true;
        }
    }

    TestTrue(TEXT("map asset disk_path는 .umap"), bFoundMapWithUmapDiskPath);
    return true;
}
