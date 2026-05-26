#include "Misc/AutomationTest.h"
#include "Specs/AIFlowSpecValidator.h"
#include "Specs/AIFlowSpec.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAIFlowSpecValidatorTest_RejectsBrokenSpec,
    "UECommandForge.Specs.AIFlowSpecValidator.RejectsBrokenSpec",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAIFlowSpecValidatorTest_RejectsBrokenSpec::RunTest(const FString& Parameters)
{
    FAIFlowSpec Spec;
    Spec.Character.AssetPath  = TEXT("not_game_path");
    Spec.Character.ParentClass = TEXT("");

    TArray<FCommandForgeError> Errors;
    const bool bValid = UECommandForge::FAIFlowSpecValidator::Validate(Spec, Errors);

    TestFalse(TEXT("validation should fail"), bValid);
    TestTrue(TEXT("at least one error"), Errors.Num() > 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAIFlowSpecValidatorTest_AcceptsValidSpec,
    "UECommandForge.Specs.AIFlowSpecValidator.AcceptsValidSpec",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAIFlowSpecValidatorTest_AcceptsValidSpec::RunTest(const FString& Parameters)
{
    FAIFlowSpec Spec;
    Spec.Version = TEXT("1");
    Spec.Character.AssetPath = TEXT("/Game/AI/BP_Guard");
    Spec.Character.ParentClass = TEXT("Character");
    Spec.AIController.AssetPath = TEXT("/Game/AI/BP_AICtrl");
    Spec.AIController.ParentClass = TEXT("AIController");
    Spec.StateTree.AssetPath = TEXT("/Game/AI/ST_Guard");
    Spec.Placement.MapPath = TEXT("/Game/Maps/TestMap");
    Spec.Placement.BlueprintPath = TEXT("/Game/AI/BP_Guard");

    TArray<FCommandForgeError> Errors;
    const bool bValid = UECommandForge::FAIFlowSpecValidator::Validate(Spec, Errors);

    TestTrue(TEXT("validation should pass"), bValid);
    TestEqual(TEXT("no errors"), Errors.Num(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAIFlowSpecValidatorTest_RejectsInvalidPaths,
    "UECommandForge.Specs.AIFlowSpecValidator.RejectsInvalidPaths",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAIFlowSpecValidatorTest_RejectsInvalidPaths::RunTest(const FString& Parameters)
{
    FAIFlowSpec Spec;
    Spec.Version = TEXT("1");
    Spec.Character.AssetPath = TEXT("/Content/AI/BP_Guard");  // Invalid: /Content/ not /Game/
    Spec.Character.ParentClass = TEXT("Character");
    Spec.AIController.AssetPath = TEXT("/Game/AI/BP_AICtrl");
    Spec.AIController.ParentClass = TEXT("AIController");
    Spec.StateTree.AssetPath = TEXT("/Game/AI/ST_Guard");
    Spec.Placement.MapPath = TEXT("/Game/Maps/TestMap");
    Spec.Placement.BlueprintPath = TEXT("/Game/AI/BP_Guard");

    TArray<FCommandForgeError> Errors;
    const bool bValid = UECommandForge::FAIFlowSpecValidator::Validate(Spec, Errors);

    TestFalse(TEXT("validation should fail"), bValid);
    TestTrue(TEXT("at least one error"), Errors.Num() > 0);
    TestEqual(TEXT("error field"), Errors[0].Field, TEXT("Character.AssetPath"));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAIFlowSpecValidatorTest_RejectsInvalidTaskType,
    "UECommandForge.Specs.AIFlowSpecValidator.RejectsInvalidTaskType",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAIFlowSpecValidatorTest_RejectsInvalidTaskType::RunTest(const FString& Parameters)
{
    FAIFlowSpec Spec;
    Spec.Version = TEXT("1");
    Spec.Character.AssetPath = TEXT("/Game/AI/BP_Guard");
    Spec.Character.ParentClass = TEXT("Character");
    Spec.AIController.AssetPath = TEXT("/Game/AI/BP_AICtrl");
    Spec.AIController.ParentClass = TEXT("AIController");
    Spec.StateTree.AssetPath = TEXT("/Game/AI/ST_Guard");
    Spec.Placement.MapPath = TEXT("/Game/Maps/TestMap");
    Spec.Placement.BlueprintPath = TEXT("/Game/AI/BP_Guard");

    // Add invalid task type
    FStateTreeTaskSpec InvalidTask;
    InvalidTask.TaskType = TEXT("InvalidTask");
    InvalidTask.StateName = TEXT("SomeState");
    Spec.StateTree.Tasks.Add(InvalidTask);

    TArray<FCommandForgeError> Errors;
    const bool bValid = UECommandForge::FAIFlowSpecValidator::Validate(Spec, Errors);

    TestFalse(TEXT("validation should fail"), bValid);
    TestTrue(TEXT("at least one error"), Errors.Num() > 0);
    TestEqual(TEXT("error code"), Errors[0].Code, TEXT("INVALID_TASK_TYPE"));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAIFlowSpecValidatorTest_AcceptsAllowedTaskTypes,
    "UECommandForge.Specs.AIFlowSpecValidator.AcceptsAllowedTaskTypes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAIFlowSpecValidatorTest_AcceptsAllowedTaskTypes::RunTest(const FString& Parameters)
{
    FAIFlowSpec Spec;
    Spec.Version = TEXT("1");
    Spec.Character.AssetPath = TEXT("/Game/AI/BP_Guard");
    Spec.Character.ParentClass = TEXT("Character");
    Spec.AIController.AssetPath = TEXT("/Game/AI/BP_AICtrl");
    Spec.AIController.ParentClass = TEXT("AIController");
    Spec.StateTree.AssetPath = TEXT("/Game/AI/ST_Guard");
    Spec.Placement.MapPath = TEXT("/Game/Maps/TestMap");
    Spec.Placement.BlueprintPath = TEXT("/Game/AI/BP_Guard");

    // Add allowed task types
    FStateTreeTaskSpec Task1;
    Task1.TaskType = TEXT("Wait");
    Task1.StateName = TEXT("WaitState");
    Spec.StateTree.Tasks.Add(Task1);

    FStateTreeTaskSpec Task2;
    Task2.TaskType = TEXT("MoveTo");
    Task2.StateName = TEXT("MoveState");
    Spec.StateTree.Tasks.Add(Task2);

    FStateTreeTaskSpec Task3;
    Task3.TaskType = TEXT("PlayMontage");
    Task3.StateName = TEXT("AnimState");
    Spec.StateTree.Tasks.Add(Task3);

    TArray<FCommandForgeError> Errors;
    const bool bValid = UECommandForge::FAIFlowSpecValidator::Validate(Spec, Errors);

    TestTrue(TEXT("validation should pass"), bValid);
    TestEqual(TEXT("no errors"), Errors.Num(), 0);
    return true;
}
