#include "Misc/AutomationTest.h"
#include "Builders/MapActorPlacer.h"
#include "Specs/PlacementSpec.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMapActorPlacerTest,
    "UECommandForge.Builders.MapActorPlacer.PlacesActorInMap",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMapActorPlacerTest::RunTest(const FString& Parameters)
{
    FPlacementSpec Spec;
    Spec.MapPath = TEXT("/Game/Tests/TestPlacementMap");
    Spec.BlueprintPath = TEXT("/Game/Tests/BP_TestCharacter");
    Spec.Location = FVector(100.0f, 200.0f, 0.0f);
    Spec.Rotation = FRotator::ZeroRotator;

    TArray<FCommandForgeError> Errors;
    TMap<FString, FString> Validation;
    const bool bOk = UECommandForge::FMapActorPlacer::Place(Spec, Errors);
    const bool bValid = UECommandForge::FMapActorPlacer::ValidatePlacement(Spec, Validation, Errors);

    TestTrue(TEXT("배치 성공"), bOk);
    TestTrue(TEXT("검증 성공"), bValid);
    TestTrue(TEXT("에러 없음"), Errors.IsEmpty());
    TestEqual(TEXT("actor_placed"), Validation.FindRef(TEXT("actor_placed")), FString(TEXT("ok")));
    return true;
}
