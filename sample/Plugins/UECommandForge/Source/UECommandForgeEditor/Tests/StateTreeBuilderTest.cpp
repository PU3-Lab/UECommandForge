#include "Misc/AutomationTest.h"
#include "Builders/StateTreeBuilder.h"
#include "Specs/StateTreeSpec.h"
#include "CommandForgeTypes.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/PackageName.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FStateTreeBuilderTest,
    "UECommandForge.Builders.StateTreeBuilder.CreatesAndCompiles",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStateTreeBuilderTest::RunTest(const FString& Parameters)
{
    FStateTreeSpec Spec;
    Spec.AssetPath = TEXT("/Game/Tests/ST_TestTree");
    Spec.Tasks.Add({ TEXT("Wait"), TEXT("Idle") });
    Spec.Tasks.Add({ TEXT("MoveTo"), TEXT("Patrol") });

    TArray<FCommandForgeError> Errors;
    TMap<FString, FString> Validation;
    const bool bOk = UECommandForge::FStateTreeBuilder::Build(Spec, Errors, Validation);

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
