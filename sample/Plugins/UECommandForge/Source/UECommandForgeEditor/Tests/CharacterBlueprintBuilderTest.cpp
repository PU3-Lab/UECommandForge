#include "Misc/AutomationTest.h"
#include "Builders/CharacterBlueprintBuilder.h"
#include "Specs/BlueprintSpec.h"
#include "CommandForgeTypes.h"
#include "HAL/PlatformFileManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCharacterBlueprintBuilderTest,
    "UECommandForge.Builders.CharacterBlueprintBuilder.CreatesAndCompiles",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCharacterBlueprintBuilderTest::RunTest(const FString& Parameters)
{
    FBlueprintSpec Spec;
    Spec.AssetPath   = TEXT("/Game/Tests/BP_TestCharacter");
    Spec.ParentClass = TEXT("Character");
    Spec.DisplayName = TEXT("TestCharacter");
    Spec.bAllowReplace = true;

    TArray<FCommandForgeError> Errors;
    TMap<FString, FString> Validation;
    const bool bOk = UECommandForge::FCharacterBlueprintBuilder::Build(Spec, Errors, Validation);

    TestTrue(TEXT("빌드 성공"), bOk);
    TestTrue(TEXT("에러 없음"), Errors.IsEmpty());

    TestEqual(TEXT("컴파일 상태"), Validation.FindRef(TEXT("compile_status")), FString(TEXT("ok")));
    TestEqual(TEXT("디스크 저장"), Validation.FindRef(TEXT("asset_on_disk")), FString(TEXT("true")));
    TestTrue(TEXT("Registry 키 존재"), Validation.Contains(TEXT("asset_in_registry")));

    const FString FileName = FPackageName::LongPackageNameToFilename(
        Spec.AssetPath, FPackageName::GetAssetPackageExtension());
    TestTrue(TEXT(".uasset 파일 존재"), FPlatformFileManager::Get().GetPlatformFile().FileExists(*FileName));

    return true;
}
