#include "Misc/AutomationTest.h"
#include "Builders/AIFlowBinder.h"
#include "Specs/AIFlowSpec.h"
#include "CommandForgeTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAIFlowBinderTest,
    "UECommandForge.Builders.AIFlowBinder.BindsCharacterToController",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAIFlowBinderTest::RunTest(const FString& Parameters)
{
    FAIFlowSpec Spec;
    Spec.Character.AssetPath    = TEXT("/Game/Tests/BP_TestCharacter");
    Spec.AIController.AssetPath = TEXT("/Game/Tests/BP_TestController");
    Spec.StateTree.AssetPath    = TEXT("/Game/Tests/ST_TestTree");

    TArray<FCommandForgeError> Errors;
    const bool bOk = UECommandForge::FAIFlowBinder::Bind(Spec, Errors);

    TestTrue(TEXT("바인딩 성공"), bOk);
    TestTrue(TEXT("에러 없음"), Errors.IsEmpty());
    return true;
}
