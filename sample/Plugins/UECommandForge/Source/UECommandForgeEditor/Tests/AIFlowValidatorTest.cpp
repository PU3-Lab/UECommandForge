#include "Misc/AutomationTest.h"
#include "Validators/AIFlowValidator.h"
#include "Specs/AIFlowSpec.h"
#include "CommandForgeTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAIFlowValidatorTest,
    "UECommandForge.Validators.AIFlowValidator.ValidatesBoundSpec",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAIFlowValidatorTest::RunTest(const FString& Parameters)
{
    FAIFlowSpec Spec;
    Spec.Character.AssetPath    = TEXT("/Game/Tests/BP_TestCharacter");
    Spec.AIController.AssetPath = TEXT("/Game/Tests/BP_TestController");
    Spec.StateTree.AssetPath    = TEXT("/Game/Tests/ST_TestTree");

    TMap<FString, FString> ValidationResults;
    TArray<FCommandForgeError> Errors;
    const bool bValid = UECommandForge::FAIFlowValidator::Validate(Spec, ValidationResults, Errors);

    TestTrue(TEXT("검증 통과"), bValid);
    TestEqual(TEXT("ai_controller_class"), ValidationResults.FindRef(TEXT("ai_controller_class")), FString(TEXT("ok")));
    TestEqual(TEXT("auto_possess_ai"),     ValidationResults.FindRef(TEXT("auto_possess_ai")),     FString(TEXT("ok")));
    return true;
}
