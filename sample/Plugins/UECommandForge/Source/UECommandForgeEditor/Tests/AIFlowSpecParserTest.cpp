#include "Misc/AutomationTest.h"
#include "Specs/AIFlowSpecParser.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAIFlowSpecParserTest,
    "UECommandForge.Specs.AIFlowSpecParser.ParsesGuardAiSpec",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAIFlowSpecParserTest::RunTest(const FString& Parameters)
{
    const FString Json = TEXT(R"({
        "Version": "1",
        "Character": { "AssetPath": "/Game/AI/BP_Guard", "ParentClass": "Character", "DisplayName": "Guard" },
        "AIController": { "AssetPath": "/Game/AI/BP_Ctrl", "ParentClass": "AIController", "DisplayName": "Ctrl" },
        "StateTree": { "AssetPath": "/Game/AI/ST_Guard", "Tasks": [] },
        "Placement": { "MapPath": "/Game/Maps/Test", "BlueprintPath": "/Game/AI/BP_Guard" }
    })");

    FAIFlowSpec Spec;
    TArray<FCommandForgeError> Errors;
    const bool bOk = UECommandForge::FAIFlowSpecParser::Parse(Json, Spec, Errors);

    TestTrue(TEXT("parse succeeded"), bOk);
    TestEqual(TEXT("character path"), Spec.Character.AssetPath, TEXT("/Game/AI/BP_Guard"));
    TestEqual(TEXT("statetree path"), Spec.StateTree.AssetPath, TEXT("/Game/AI/ST_Guard"));
    return true;
}
