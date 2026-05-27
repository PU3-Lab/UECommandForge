#include "Misc/AutomationTest.h"
#include "Builders/AIControllerBlueprintBuilder.h"
#include "Builders/AIFlowBinder.h"
#include "Builders/CharacterBlueprintBuilder.h"
#include "Builders/StateTreeBuilder.h"
#include "CommandForgeTypes.h"
#include "Specs/AIFlowSpec.h"
#include "Validators/AIFlowValidator.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAIFlowBindingTest,
    "UECommandForge.Builders.AIFlowBinder.BindsAndValidatesFlow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAIFlowBindingTest::RunTest(const FString& Parameters)
{
    auto TestNoErrors = [this](const TCHAR* Label, const TArray<FCommandForgeError>& Errors) -> bool
    {
        if (!Errors.IsEmpty())
        {
            this->AddError(FString::Printf(TEXT("%s: %s"), Label, *Errors[0].Message));
            return false;
        }
        return true;
    };

    FAIFlowSpec Spec;
    Spec.Character.AssetPath = TEXT("/Game/Tests/BP_TestCharacter");
    Spec.Character.ParentClass = TEXT("Character");
    Spec.AIController.AssetPath = TEXT("/Game/Tests/BP_TestController");
    Spec.AIController.ParentClass = TEXT("AIController");
    Spec.StateTree.AssetPath = TEXT("/Game/Tests/ST_TestTree");
    Spec.StateTree.Tasks.Add({ TEXT("Wait"), TEXT("Idle") });
    Spec.StateTree.Tasks.Add({ TEXT("MoveTo"), TEXT("Patrol") });

    TArray<FCommandForgeError> CharacterErrors;
    TMap<FString, FString> CharacterValidation;
    if (!TestTrue(TEXT("Character BP 생성"),
            UECommandForge::FCharacterBlueprintBuilder::Build(Spec.Character, CharacterErrors, CharacterValidation)) ||
        !TestNoErrors(TEXT("Character BP 생성 에러"), CharacterErrors))
    {
        return false;
    }

    TArray<FCommandForgeError> ControllerErrors;
    TMap<FString, FString> ControllerValidation;
    if (!TestTrue(TEXT("AIController BP 생성"),
            UECommandForge::FAIControllerBlueprintBuilder::Build(Spec.AIController, ControllerErrors, ControllerValidation)) ||
        !TestNoErrors(TEXT("AIController BP 생성 에러"), ControllerErrors))
    {
        return false;
    }

    TArray<FCommandForgeError> StateTreeErrors;
    TMap<FString, FString> StateTreeValidation;
    if (!TestTrue(TEXT("StateTree 생성"),
            UECommandForge::FStateTreeBuilder::Build(Spec.StateTree, StateTreeErrors, StateTreeValidation)) ||
        !TestNoErrors(TEXT("StateTree 생성 에러"), StateTreeErrors))
    {
        return false;
    }

    TArray<FCommandForgeError> BindErrors;
    TMap<FString, FString> BindValidation;
    const bool bBound = UECommandForge::FAIFlowBinder::Bind(Spec, BindErrors, BindValidation);
    if (!TestTrue(TEXT("바인딩 성공"), bBound) ||
        !TestNoErrors(TEXT("바인딩 에러"), BindErrors))
    {
        return false;
    }
    TestEqual(TEXT("ai_controller_class"), BindValidation.FindRef(TEXT("ai_controller_class")), FString(TEXT("ok")));
    TestEqual(TEXT("auto_possess_ai"), BindValidation.FindRef(TEXT("auto_possess_ai")), FString(TEXT("ok")));
    TestEqual(TEXT("statetree_component"), BindValidation.FindRef(TEXT("statetree_component")), FString(TEXT("ok")));

    TArray<FCommandForgeError> ValidateErrors;
    TMap<FString, FString> ValidateResults;
    const bool bValid = UECommandForge::FAIFlowValidator::Validate(Spec, ValidateResults, ValidateErrors);
    if (!TestTrue(TEXT("검증 성공"), bValid) ||
        !TestNoErrors(TEXT("검증 에러"), ValidateErrors))
    {
        return false;
    }
    TestEqual(TEXT("검증 ai_controller_class"), ValidateResults.FindRef(TEXT("ai_controller_class")), FString(TEXT("ok")));
    TestEqual(TEXT("검증 auto_possess_ai"), ValidateResults.FindRef(TEXT("auto_possess_ai")), FString(TEXT("ok")));
    TestEqual(TEXT("검증 statetree_component"), ValidateResults.FindRef(TEXT("statetree_component")), FString(TEXT("ok")));

    return true;
}
