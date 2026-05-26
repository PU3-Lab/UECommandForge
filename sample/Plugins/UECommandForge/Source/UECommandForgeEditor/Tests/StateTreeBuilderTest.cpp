#include "Misc/AutomationTest.h"
#include "Builders/StateTreeBuilder.h"
#include "Specs/StateTreeSpec.h"
#include "CommandForgeTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FStateTreeBuilderTest,
    "UECommandForge.Builders.StateTreeBuilder.CreatesStateTree",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStateTreeBuilderTest::RunTest(const FString& Parameters)
{
    FStateTreeSpec Spec;
    Spec.AssetPath = TEXT("/Game/Tests/ST_TestTree");
    Spec.Tasks.Add({ TEXT("Wait"),   TEXT("Idle") });
    Spec.Tasks.Add({ TEXT("MoveTo"), TEXT("Patrol") });

    TArray<FCommandForgeError> Errors;
    const bool bOk = UECommandForge::FStateTreeBuilder::Build(Spec, Errors);

    TestTrue(TEXT("빌드 성공"), bOk);
    TestTrue(TEXT("에러 없음"), Errors.IsEmpty());
    return true;
}
